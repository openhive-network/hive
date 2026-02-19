
#include <hive/chain/detail/state/account_object.hpp>

#include <hive/chain/database_virtual_operations.hpp>
#include <hive/chain/external_storage/comments_handler.hpp>
#include <hive/chain/notifications.hpp>

#include <hive/chain/account_object_multiindex.hpp>
#include <hive/chain/block_summary_object_multiindex.hpp>
#include <hive/chain/comment_object_multiindex.hpp>
#include <hive/chain/detail/state/reward_fund_object_multiindex.hpp>
#include <hive/chain/detail/state/feed_history_object.hpp>

#include <hive/chain/index.hpp>
#include <chainbase/chainbase.inl>

#include <hive/chain/util/type_registrar_definition.hpp>
#include <hive/chain/util/reward.hpp>
#include <hive/chain/global_property_object_multiindex.hpp>

#include <hive/protocol/hive_operations.hpp>
#include <hive/protocol/hive_virtual_operations.hpp>

#include "database_vesting_helpers.hpp"

#include <set>

namespace hive { namespace chain {

using hive::protocol::author_reward_operation;
using hive::protocol::comment_benefactor_reward_operation;
using hive::protocol::comment_reward_operation;
using hive::protocol::comment_payout_update_operation;
using hive::protocol::curation_reward_operation;
using hive::protocol::vesting_shares_split_operation;

// Local struct for process_comment_cashout
struct reward_fund_context
{
  uint128_t   recent_claims = 0;
  asset       reward_balance = asset( 0, HIVE_SYMBOL );
  share_type  hive_awarded = 0;
};

void initialize_core_indexes_03( database& db )
{
  HIVE_ADD_CORE_INDEX(db, block_summary_index);
  HIVE_ADD_CORE_INDEX(db, comment_index);
  HIVE_ADD_CORE_INDEX(db, comment_vote_index);
  HIVE_ADD_CORE_INDEX(db, comment_cashout_index);
  HIVE_ADD_CORE_INDEX(db, comment_cashout_ex_index);
}

const comment_object* database::find_comment( comment_id_type comment_id )const
{
  return find< comment_object, by_id >( comment_id );
}

const comment_object& database::get_comment_for_payout_time( const comment_object& comment )const
{
  if( has_hardfork( HIVE_HARDFORK_0_17__769 ) || comment.is_root() )
    return comment;
  else
    return get< comment_object >( find_comment_cashout_ex( comment )->get_root_id() );
}

const comment_cashout_object* database::find_comment_cashout( comment_id_type comment_id ) const
{
  const auto& idx = get_index< comment_cashout_index, by_id >();
  comment_cashout_object::id_type ccid( comment_id );
  auto found = idx.find( ccid );

  if( found == idx.end() )
    return nullptr;
  else
    return &( *found );
}

const comment_cashout_ex_object* database::find_comment_cashout_ex( comment_id_type comment_id ) const
{
  if( has_hardfork( HIVE_HARDFORK_0_19 ) )
    return nullptr;

  const auto& idx = get_index< comment_cashout_ex_index, by_id >();
  comment_cashout_ex_object::id_type ccid( comment_id );
  auto found = idx.find( ccid );

  FC_ASSERT( found != idx.end() && "by id" );
  return &( *found );
}

const comment_object& database::get_comment( const comment_cashout_object& comment_cashout ) const
{
  const auto& idx = get_index< comment_index >().indices().get< by_id >();
  auto found = idx.find( comment_cashout.get_comment_id() );

  FC_ASSERT( found != idx.end() && "by cashout object" );

  return *found;
}

void database::remove_old_cashouts()
{
  // Remove all cashout extras
  auto& comment_cashout_ex_idx = get_mutable_index< comment_cashout_ex_index >();
  comment_cashout_ex_idx.clear();

  // Remove regular cashouts for paid comments
  const auto& idx = get_index< comment_cashout_index, by_cashout_time >();
  auto itr = idx.find( fc::time_point_sec::maximum() );

  auto block_num = head_block_num();
  while( itr != idx.end() )
  {
    const auto& current = *itr;
    const auto& comment = get_comment( current );
    ++itr;
    get_comments_handler().on_cashout( block_num, comment, current );
    remove( current );
  }
}

/**
  *  This method will iterate through all comment_vote_objects and give them
  *  (max_rewards * weight) / c.total_vote_weight.
  *
  *  @returns unclaimed rewards.
  */
share_type database::pay_curators( const comment_object& comment, const comment_cashout_object& comment_cashout, share_type& max_rewards )
{
  struct cmp
  {
    bool operator()( const comment_vote_object* obj, const comment_vote_object* obj2 ) const
    {
      if( obj->get_weight() == obj2->get_weight() )
        return obj->get_voter() < obj2->get_voter();
      else
        return obj->get_weight() > obj2->get_weight();
    }
  };

  try
  {
    share_type unclaimed_rewards = max_rewards;

    if( !comment_cashout.allows_curation_rewards() )
    {
      unclaimed_rewards = 0;
      max_rewards = 0;
    }
    else if( comment_cashout.get_total_vote_weight() > 0 )
    {
      uint128_t total_weight( comment_cashout.get_total_vote_weight() );

      const auto& cvidx = get_index<comment_vote_index, by_comment_voter>();
      auto itr = cvidx.lower_bound( comment.get_id() );

      std::set< const comment_vote_object*, cmp > proxy_set;
      while( itr != cvidx.end() && itr->get_comment() == comment.get_id() )
      {
        proxy_set.insert( &( *itr ) );
        ++itr;
      }

      const auto& comment_author_name = get_account( comment_cashout.get_author_id() ).get_name();
      for( auto& item : proxy_set )
      { try {
        uint128_t weight( item->get_weight() );
        auto claim = fc::uint128_to_uint64( ( max_rewards.value * weight ) / total_weight );
        if( claim > 0 ) // min_amt is non-zero satoshis
        {
          unclaimed_rewards -= claim;
          // Use get_account() which goes through accounts_handler to support RocksDB archived accounts
          const auto& voter = get_account( item->get_voter() );
          operation vop = curation_reward_operation( voter.get_name(), asset(0, VESTS_SYMBOL), comment_author_name, to_string( comment_cashout.get_permlink() ), has_hardfork( HIVE_HARDFORK_0_17__659 ) );
          create_vesting2( *this, voter, asset( claim, HIVE_SYMBOL ), has_hardfork( HIVE_HARDFORK_0_17__659 ),
            [&]( const asset& reward )
            {
              vop.get< curation_reward_operation >().reward = reward;
              pre_push_virtual_operation( *this, vop );
            } );

            modify( get< assets_object, by_account_id >( voter.get_id() ), [&]( assets_object& assets )
            {
              assets.set_curation_rewards( assets.get_curation_rewards() + HIVE_asset( claim ) );
            });
          post_push_virtual_operation( *this, vop );
        }
      } FC_CAPTURE_AND_RETHROW( (*item) ) }
    }
    max_rewards -= unclaimed_rewards;

    return unclaimed_rewards;
  } FC_CAPTURE_AND_RETHROW( (max_rewards) )
}

share_type database::cashout_comment_helper( util::comment_reward_context& ctx, const comment_object& comment,
  const comment_cashout_object& comment_cashout, const comment_cashout_ex_object* comment_cashout_ex, bool forward_curation_remainder )
{
  try
  {
    share_type claimed_reward = 0;

    if( comment_cashout.get_net_rshares() > 0 )
    {
      ctx.rshares = comment_cashout.get_net_rshares();
      ctx.max_hbd = comment_cashout.get_max_accepted_payout();
      if( comment_cashout_ex )
        ctx.reward_weight = comment_cashout_ex->get_reward_weight();
      else
        ctx.reward_weight = HIVE_100_PERCENT;

      if( has_hardfork( HIVE_HARDFORK_0_17__774 ) )
      {
        const auto& rf = get_reward_fund();
        ctx.reward_curve = rf.author_reward_curve;
        ctx.content_constant = rf.content_constant;
      }

      const share_type reward = util::get_rshare_reward( ctx );
      uint128_t reward_tokens = uint128_t( reward.value );
      share_type curation_tokens;
      share_type author_tokens;

      if( reward_tokens > 0 )
      {
        curation_tokens = fc::uint128_to_uint64( ( reward_tokens * get_curation_rewards_percent() ) / HIVE_100_PERCENT );
        author_tokens = fc::uint128_to_uint64(reward_tokens) - curation_tokens;

        share_type curation_remainder = pay_curators( comment, comment_cashout, curation_tokens );

        if( forward_curation_remainder )
          author_tokens += curation_remainder;

        share_type total_beneficiary = 0;
        claimed_reward = author_tokens + curation_tokens;
        const auto& author = get_account( comment_cashout.get_author_id() );
        const auto& comment_author = author.get_name();

        for( auto& b : comment_cashout.get_beneficiaries() )
        {
          const auto& beneficiary = get_account( b.account_id );

          auto benefactor_tokens = ( author_tokens * b.weight ) / HIVE_100_PERCENT;
          auto benefactor_vesting_hive = benefactor_tokens;
          auto vop = comment_benefactor_reward_operation( beneficiary.get_name(), comment_author, to_string( comment_cashout.get_permlink() ),
            asset( 0, HBD_SYMBOL ), asset( 0, HIVE_SYMBOL ), asset( 0, VESTS_SYMBOL ), has_hardfork( HIVE_HARDFORK_0_17__659 ) );

          if( has_hardfork( HIVE_HARDFORK_0_21__3343 ) && is_treasury( beneficiary.get_name() ) )
          {
            benefactor_vesting_hive = 0;
            vop.hbd_payout = asset( benefactor_tokens, HIVE_SYMBOL ) * get_feed_history().current_median_history;
            vop.payout_must_be_claimed = false;
            adjust_balance( get_treasury(), vop.hbd_payout );
            adjust_supply( asset( -benefactor_tokens, HIVE_SYMBOL ) );
            adjust_supply( vop.hbd_payout );
          }
          else if( has_hardfork( HIVE_HARDFORK_0_20__2022 ) )
          {
            auto benefactor_hbd_hive = ( benefactor_tokens * comment_cashout.get_percent_hbd() ) / ( 2 * HIVE_100_PERCENT ) ;
            benefactor_vesting_hive  = benefactor_tokens - benefactor_hbd_hive;
            auto hbd_payout          = create_hbd( beneficiary, asset( benefactor_hbd_hive, HIVE_SYMBOL ), true );

            vop.hbd_payout   = hbd_payout.first; // HBD portion
            vop.hive_payout = hbd_payout.second; // HIVE portion
          }

          create_vesting2( *this, beneficiary, asset( benefactor_vesting_hive, HIVE_SYMBOL ), has_hardfork( HIVE_HARDFORK_0_17__659 ),
          [&]( const asset& reward )
          {
            vop.vesting_payout = reward;
            pre_push_virtual_operation( *this, vop );
          });

          post_push_virtual_operation( *this, vop );
          total_beneficiary += benefactor_tokens;
        }

        author_tokens -= total_beneficiary;

        auto hbd_hive     = ( author_tokens * comment_cashout.get_percent_hbd() ) / ( 2 * HIVE_100_PERCENT ) ;
        auto vesting_hive = author_tokens - hbd_hive;

        auto hbd_payout = create_hbd( author, asset( hbd_hive, HIVE_SYMBOL ), has_hardfork( HIVE_HARDFORK_0_17__659 ) );

        /*
          Total payout for curators is calculated due to the performance in third party softwares(f.e. `hivemind`).
          During payments calculations it's enough to catch `author_reward_operation`, without adding all values from `curation_reward_operation`.
        */
        auto curators_vesting_payout = calculate_vesting( *this, asset( curation_tokens, HIVE_SYMBOL ), has_hardfork( HIVE_HARDFORK_0_17__659 ) );

        operation vop = author_reward_operation( comment_author, to_string( comment_cashout.get_permlink() ), hbd_payout.first, hbd_payout.second, asset( 0, VESTS_SYMBOL ),
                                                curators_vesting_payout, has_hardfork( HIVE_HARDFORK_0_17__659 ) );

        create_vesting2( *this, author, asset( vesting_hive, HIVE_SYMBOL ), has_hardfork( HIVE_HARDFORK_0_17__659 ),
          [&]( const asset& vesting_payout )
          {
            vop.get< author_reward_operation >().vesting_payout = vesting_payout;
            pre_push_virtual_operation( *this, vop );
          } );
        post_push_virtual_operation( *this, vop );

        asset payout = hbd_payout.first + to_hbd( hbd_payout.second + asset( vesting_hive, HIVE_SYMBOL ) );
        asset curator_payout = to_hbd( asset( curation_tokens, HIVE_SYMBOL ) );
        asset beneficiary_payout = to_hbd( asset( total_beneficiary, HIVE_SYMBOL ) );
        if( !has_hardfork( HIVE_HARDFORK_0_19 ) )
        {
          modify( *comment_cashout_ex, [&]( comment_cashout_ex_object& c_ex )
          {
            c_ex.add_payout_values( payout, curator_payout, beneficiary_payout );
            payout = c_ex.get_total_payout();
            curator_payout = c_ex.get_curator_payout();
            beneficiary_payout = c_ex.get_beneficiary_payout();
          } );
        }
        vop = comment_reward_operation( comment_author, to_string( comment_cashout.get_permlink() ),
          to_hbd( asset( claimed_reward, HIVE_SYMBOL ) ), author_tokens, payout, curator_payout, beneficiary_payout );
        pre_push_virtual_operation( *this, vop );
        post_push_virtual_operation( *this, vop );

        modify( get< assets_object, by_account_id >( author.get_id() ), [&]( assets_object& assets )
        {
          assets.set_posting_rewards( assets.get_posting_rewards() + HIVE_asset( author_tokens ) );
        });
      }

      notify_comment_reward( { reward, author_tokens, curation_tokens } );

      if( !has_hardfork( HIVE_HARDFORK_0_17__774 ) )
        adjust_rshares2( util::evaluate_reward_curve( comment_cashout.get_net_rshares() ), 0 );
    }

    if( !has_hardfork( HIVE_HARDFORK_0_19 ) ) // paid comment is removed after HF19, so no point in modification
    {
      modify( comment_cashout, [&]( comment_cashout_object& c )
      {
        /**
        * A payout is only made for positive rshares, negative rshares hang around
        * for the next time this post might get an upvote.
        */
        int64_t negative_rshares = c.get_net_rshares() < 0 ? c.get_net_rshares() : 0;
        c.on_payout();
        c.accumulate_vote_rshares( negative_rshares, 0 );

        if( has_hardfork( HIVE_HARDFORK_0_17__769 ) )
        {
          c.set_cashout_time();
        }
        else if( comment.is_root() )
        {
          if( has_hardfork( HIVE_HARDFORK_0_12__177 ) && !comment_cashout_ex->was_paid() )
            c.set_cashout_time( head_block_time() + HIVE_SECOND_CASHOUT_WINDOW );
          else
            c.set_cashout_time();
        }
      } );
      modify( *comment_cashout_ex, [&]( comment_cashout_ex_object& c_ex )
      {
        c_ex.on_payout( head_block_time() );
      } );
    }

    if( has_hardfork( HIVE_HARDFORK_0_17__769 ) || calculate_discussion_payout_time( comment, comment_cashout ) == fc::time_point_sec::maximum() )
    {
      push_virtual_operation( *this, comment_payout_update_operation( get_account( comment_cashout.get_author_id() ).get_name(), to_string( comment_cashout.get_permlink() ) ) );
    }

    const auto& vote_idx = get_index< comment_vote_index, by_comment_voter >();
    auto vote_itr = vote_idx.lower_bound( comment.get_id() );
    while( vote_itr != vote_idx.end() && vote_itr->get_comment() == comment.get_id() )
    {
      const auto& cur_vote = *vote_itr;
      ++vote_itr;
      remove( cur_vote );
    }

    return claimed_reward;
  } FC_CAPTURE_AND_RETHROW( (comment)(comment_cashout)(ctx) )
}

void database::process_comment_cashout()
{
  /// don't allow any content to get paid out until the website is ready to launch
  /// and people have had a week to start posting.  The first cashout will be the biggest because it
  /// will represent 2+ months of rewards.
  if( !has_hardfork( HIVE_HARDFORK_0_7_FIRST_CASHOUT_TIME ) )
    return;

  const auto& gpo = get_dynamic_global_properties();
  auto _now = head_block_time();
  util::comment_reward_context ctx;
  ctx.current_hive_price = get_feed_history().current_median_history;

  vector< reward_fund_context > funds;
  vector< share_type > hive_awarded;
  const auto& reward_idx = get_index< reward_fund_index, by_id >();

  // Decay recent rshares of each fund
  for( auto itr = reward_idx.begin(); itr != reward_idx.end(); ++itr )
  {
    // Add all reward funds to the local cache and decay their recent rshares
    modify( *itr, [&]( reward_fund_object& rfo )
    {
      fc::microseconds decay_time;

      if( has_hardfork( HIVE_HARDFORK_0_19__1051 ) )
        decay_time = HIVE_RECENT_RSHARES_DECAY_TIME_HF19;
      else
        decay_time = HIVE_RECENT_RSHARES_DECAY_TIME_HF17;

      if( ( _now - rfo.last_update ) <= decay_time )
        rfo.recent_claims -= ( rfo.recent_claims * ( _now - rfo.last_update ).to_seconds() ) / decay_time.to_seconds();
      else
        rfo.recent_claims = 0; // this should never happen - requires chain to be inactive for more than decay_time
      rfo.last_update = _now;
    });

    reward_fund_context rf_ctx;
    rf_ctx.recent_claims = itr->recent_claims;
    rf_ctx.reward_balance = itr->reward_balance;

    // The index is by ID, so the ID should be the current size of the vector (0, 1, 2, etc...)
    assert( funds.size() == itr->get_id().get_value() );

    funds.push_back( rf_ctx );
  }

  const auto& cidx        = get_index< comment_cashout_index, by_cashout_time >();
  const auto& com_by_root = get_index< comment_cashout_ex_index, by_root >();

  auto _current = cidx.begin();
  // add all rshares about to be cashed out to the reward funds. This ensures equal satoshi per rshare payment
  if( has_hardfork( HIVE_HARDFORK_0_17__771 ) )
  {
    while( _current != cidx.end() && _current->get_cashout_time() <= _now )
    {
      if( _current->get_net_rshares() > 0 )
      {
        const auto& rf = get_reward_fund();
        funds[ rf.get_id() ].recent_claims += util::evaluate_reward_curve( _current->get_net_rshares(), rf.author_reward_curve, rf.content_constant );
      }

      ++_current;
    }

    _current = cidx.begin();
  }

  bool forward_curation_remainder = !has_hardfork( HIVE_HARDFORK_0_20__1877 );
  /*
    * Payout all comments
    *
    * Each payout follows a similar pattern, but for a different reason.
    * Cashout comment helper does not know about the reward fund it is paying from.
    * The helper only does token allocation based on curation rewards and the HBD
    * global %, etc.
    *
    * Each context is used by get_rshare_reward to determine what part of each budget
    * the comment is entitled to. Prior to hardfork 17, all payouts are done against
    * the global state updated each payout. After the hardfork, each payout is done
    * against a reward fund state that is snapshotted before all payouts in the block.
    */
  int count = 0;
  auto block_num = head_block_num();
  if( _benchmark_dumper.is_enabled() )
    _benchmark_dumper.begin();
  while( _current != cidx.end() && _current->get_cashout_time() <= _now )
  {
    if( has_hardfork( HIVE_HARDFORK_0_17__771 ) )
    {
      auto fund_id = get_reward_fund().get_id();
      ctx.total_reward_shares2 = funds[ fund_id ].recent_claims;
      ctx.total_reward_fund_hive = funds[ fund_id ].reward_balance;

      const comment_object& _comment = get_comment( *_current );
      funds[ fund_id ].hive_awarded += cashout_comment_helper( ctx, _comment, *_current,
        find_comment_cashout_ex( _comment ), forward_curation_remainder );
      ++count;

      if( has_hardfork( HIVE_HARDFORK_0_19 ) )
      {
        get_comments_handler().on_cashout( block_num, _comment, *_current );
        remove( *_current );
      }
    }
    else
    {
      comment_id_type root_id = find_comment_cashout_ex( _current->get_comment_id() )->get_root_id();
      auto itr = com_by_root.lower_bound( root_id );
      while( itr != com_by_root.end() && itr->get_root_id() == root_id )
      {
        const auto& comment_cashout_ex = *itr; ++itr;
        ctx.total_reward_shares2 = gpo.total_reward_shares2;
        ctx.total_reward_fund_hive = gpo.get_total_reward_fund_hive();

        const comment_object* comment = find_comment( comment_cashout_ex.get_comment_id() );
        FC_ASSERT( comment );
        const comment_cashout_object* comment_cashout = find_comment_cashout( comment_cashout_ex.get_comment_id() );
        FC_ASSERT( comment_cashout && "Not found" );
        auto reward = cashout_comment_helper( ctx, *comment, *comment_cashout, &comment_cashout_ex );
        ++count;

        if( reward > 0 )
        {
          modify( get_dynamic_global_properties(), [&]( dynamic_global_property_object& p )
          {
            p.total_reward_fund_hive.amount -= reward;
          });
        }
      }
    }

    _current = cidx.begin();
  }
  if( _benchmark_dumper.is_enabled() && count )
    _benchmark_dumper.end( "processing", "hive::protocol::comment_operation", count );

  // Write the cached fund state back to the database
  if( funds.size() )
  {
    for( size_t i = 0; i < funds.size(); i++ )
    {
      modify( get< reward_fund_object, by_id >( reward_fund_object::id_type( i ) ), [&]( reward_fund_object& rfo )
      {
        rfo.recent_claims = funds[ i ].recent_claims;
        rfo.reward_balance -= asset( funds[ i ].hive_awarded, HIVE_SYMBOL );
      });
    }
  }
}

void database::apply_hardfork_17_comment_cashout_fix()
{
  /*
  * For all current comments we will either keep their current cashout time, or extend it to 1 week
  * after creation.
  *
  * We cannot do a simple iteration by cashout time because we are editting cashout time.
  * More specifically, we will be adding an explicit cashout time to all comments with parents.
  * To find all discussions that have not been paid out we fir iterate over posts by cashout time.
  * Before the hardfork these are all root posts. Iterate over all of their children, adding each
  * to a specific list. Next, update payout times for all discussions on the root post. This defines
  * the min cashout time for each child in the discussion. Then iterate over the children and set
  * their cashout time in a similar way, grabbing the root post as their inherent cashout time.
  */
  const auto& comment_idx = get_index< comment_cashout_index, by_cashout_time >();
  const auto& by_root_idx = get_index< comment_cashout_ex_index, by_root >();
  vector< const comment_cashout_object* > root_posts;
  root_posts.reserve( HIVE_HF_17_NUM_POSTS );
  vector< const comment_cashout_object* > replies;
  replies.reserve( HIVE_HF_17_NUM_REPLIES );

  for( auto itr = comment_idx.begin(); itr != comment_idx.end() && itr->get_cashout_time() < fc::time_point_sec::maximum(); ++itr )
  {
    root_posts.push_back( &(*itr) );
    auto root_id = itr->get_comment_id();

    for( auto reply_itr = by_root_idx.lower_bound( root_id ); reply_itr != by_root_idx.end() && reply_itr->get_root_id() == root_id; ++reply_itr )
    {
      const comment_cashout_object* comment_cashout = find_comment_cashout( reply_itr->get_comment_id() );
      replies.push_back( comment_cashout );
    }
  }

  for( const auto& itr : root_posts )
  {
    modify( *itr, [&]( comment_cashout_object& c )
    {
      c.set_cashout_time( std::max( c.get_creation_time() + HIVE_CASHOUT_WINDOW_SECONDS, c.get_cashout_time() ) );
    });
  }

  for( const auto& itr : replies )
  {
    modify( *itr, [&]( comment_cashout_object& c )
    {
      c.set_cashout_time( std::max( calculate_discussion_payout_time( get_comment( c ), c ), c.get_creation_time() + HIVE_CASHOUT_WINDOW_SECONDS ) );
    });
  }
}

void database::apply_hardfork_12_comment_cashout_fix()
{
  const auto& comment_idx = get_index< comment_cashout_index >().indices();

  for( auto itr = comment_idx.begin(); itr != comment_idx.end(); ++itr )
  {
    // At the hardfork time, all new posts with no votes get their cashout time set to +12 hrs from head block time.
    // All posts with a payout get their cashout time set to +30 days. This hardfork takes place within 30 days
    // initial payout so we don't have to handle the case of posts that should be frozen that aren't
    const comment_object& comment = get_comment( *itr );
    const comment_cashout_ex_object* c_ex = find_comment_cashout_ex( comment );
    if( comment.is_root() )
    {
      // Post has not been paid out and has no votes (cashout_time == 0 === net_rshares == 0, under current semantics)
      if( !c_ex->was_paid() && itr->get_cashout_time() == fc::time_point_sec::maximum() )
      {
        modify( *itr, [&]( comment_cashout_object & c )
        {
          c.set_cashout_time( head_block_time() + HIVE_CASHOUT_WINDOW_SECONDS_PRE_HF17 );
        });
      }
      // Has been paid out, needs to be on second cashout window
      else if( c_ex->was_paid() )
      {
        modify( *itr, [&]( comment_cashout_object& c )
        {
          c.set_cashout_time( c_ex->get_last_payout() + HIVE_SECOND_CASHOUT_WINDOW );
        });
      }
    }
  }
}

void database::perform_vesting_share_split( uint32_t magnitude )
{
  try
  {
    modify( get_dynamic_global_properties(), [&]( dynamic_global_property_object& d )
    {
      d.total_vesting_shares.amount *= magnitude;
      d.total_reward_shares2 = 0;
    } );

    // Need to update all VESTS in accounts and the total VESTS in the dgpo
    for( const auto& account : get_index< account_index, by_id >() )
    {
      const auto& account_assets = get< assets_object, by_account_id >( account.get_id() );
      asset old_vesting_shares = account_assets.get_vesting();
      asset new_vesting_shares = old_vesting_shares;
      modify( account_assets, [&]( assets_object& a )
      {
        a.set_vesting( a.get_vesting() * magnitude );
        new_vesting_shares = a.get_vesting();
        a.set_withdrawn( a.get_withdrawn() * magnitude );
        a.set_to_withdraw( a.get_to_withdraw() * magnitude );
        a.set_vesting_withdraw_rate( VEST_asset( a.get_to_withdraw().amount / HIVE_VESTING_WITHDRAW_INTERVALS_PRE_HF_16 ) );
        FC_ASSERT( a.get_vesting_withdraw_rate().amount > 0 || a.get_to_withdraw().amount == 0, "Unexpected truncation to zero." );
      } );
      modify( account, [&]( account_object& a )
      {
        for( uint32_t i = 0; i < HIVE_MAX_PROXY_RECURSION_DEPTH; ++i )
          a.get_proxied_vsf_votes()[i] *= magnitude;
      } );
      if (old_vesting_shares != new_vesting_shares)
        push_virtual_operation( *this, vesting_shares_split_operation(account.get_name(), old_vesting_shares, new_vesting_shares) );
    }

    const auto& comments = get_index< comment_cashout_index >().indices();
    for( const auto& comment : comments )
    {
      modify( comment, [&]( comment_cashout_object& c )
      {
        //c.net_rshares *= magnitude;
        //c.vote_rshares *= magnitude;
        c.accumulate_vote_rshares( c.get_net_rshares() * ( magnitude - 1 ), c.get_vote_rshares() * ( magnitude - 1 ) );
      } );
      modify( *find_comment_cashout_ex( comment.get_comment_id() ), [&]( comment_cashout_ex_object& c_ex )
      {
        //c_ex.abs_rshares *= magnitude;
        c_ex.accumulate_abs_rshares( c_ex.get_abs_rshares() * ( magnitude - 1 ) );
      } );
    }

    for( const auto& c : comments )
    {
      if( c.get_net_rshares() > 0 )
        adjust_rshares2( 0, util::evaluate_reward_curve( c.get_net_rshares() ) );
    }

  }
  FC_CAPTURE_AND_RETHROW()
}

} }

HIVE_DEFINE_TYPE_REGISTRAR_REGISTER_TYPE(hive::chain::block_summary_index)
HIVE_DEFINE_TYPE_REGISTRAR_REGISTER_TYPE(hive::chain::comment_index)
HIVE_DEFINE_TYPE_REGISTRAR_REGISTER_TYPE(hive::chain::comment_vote_index)
HIVE_DEFINE_TYPE_REGISTRAR_REGISTER_TYPE(hive::chain::comment_cashout_index)
HIVE_DEFINE_TYPE_REGISTRAR_REGISTER_TYPE(hive::chain::comment_cashout_ex_index)

// Explicit template instantiations for chainbase::database methods
template const chainbase::generic_index<hive::chain::block_summary_index>& chainbase::database::get_index<hive::chain::block_summary_index>() const;
template chainbase::generic_index<hive::chain::block_summary_index>& chainbase::database::get_mutable_index<hive::chain::block_summary_index>();

template const chainbase::generic_index<hive::chain::comment_index>& chainbase::database::get_index<hive::chain::comment_index>() const;
template chainbase::generic_index<hive::chain::comment_index>& chainbase::database::get_mutable_index<hive::chain::comment_index>();

template const chainbase::generic_index<hive::chain::comment_vote_index>& chainbase::database::get_index<hive::chain::comment_vote_index>() const;
template chainbase::generic_index<hive::chain::comment_vote_index>& chainbase::database::get_mutable_index<hive::chain::comment_vote_index>();

template const chainbase::generic_index<hive::chain::comment_cashout_index>& chainbase::database::get_index<hive::chain::comment_cashout_index>() const;
template chainbase::generic_index<hive::chain::comment_cashout_index>& chainbase::database::get_mutable_index<hive::chain::comment_cashout_index>();

template const chainbase::generic_index<hive::chain::comment_cashout_ex_index>& chainbase::database::get_index<hive::chain::comment_cashout_ex_index>() const;
template chainbase::generic_index<hive::chain::comment_cashout_ex_index>& chainbase::database::get_mutable_index<hive::chain::comment_cashout_ex_index>();
