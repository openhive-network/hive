#include <hive/chain/hive_fwd.hpp>

#include <hive/protocol/fixed_string.hpp>

#include <hive/chain/hive_evaluator.hpp>
#include <hive/chain/database.hpp>
#include <hive/chain/database_virtual_operations.hpp>
#include <hive/chain/detail/state/reward_fund_object.hpp>
#include <hive/chain/comment_object_multiindex.hpp>
#include <hive/chain/account_object_multiindex.hpp>
#include <hive/chain/evaluator_registry.hpp>

#include <hive/chain/util/reward.hpp>
#include <hive/chain/util/manabar.hpp>

#include <fc/uint128.hpp>
#include <fc/utf8.hpp>

namespace hive { namespace chain {

HIVE_DEFINE_EVALUATOR( comment )
HIVE_DEFINE_EVALUATOR( comment_options )
HIVE_DEFINE_EVALUATOR( delete_comment )
HIVE_DEFINE_EVALUATOR( vote )

void register_social_evaluators( evaluator_registry<operation>& registry )
{
  registry.register_evaluator< vote_evaluator            >();
  registry.register_evaluator< comment_evaluator         >();
  registry.register_evaluator< comment_options_evaluator >();
  registry.register_evaluator< delete_comment_evaluator  >();
}

inline void validate_permlink_0_1( const string& permlink )
{
  FC_ASSERT( permlink.size() > HIVE_MIN_PERMLINK_LENGTH && permlink.size() < HIVE_MAX_PERMLINK_LENGTH, "Permlink is not a valid size." );

  for( const auto& c : permlink )
  {
    FC_ASSERT( c == 'a' || c == 'b' || c == 'c' || c == 'd' || c == 'e' || c == 'f' ||
      c == 'g' || c == 'h' || c == 'i' || c == 'j' || c == 'k' || c == 'l' || c == 'm' ||
      c == 'n' || c == 'o' || c == 'p' || c == 'q' || c == 'r' || c == 's' || c == 't' ||
      c == 'u' || c == 'v' || c == 'w' || c == 'x' || c == 'y' || c == 'z' || c == '0' ||
      c == '1' || c == '2' || c == '3' || c == '4' || c == '5' || c == '6' || c == '7' ||
      c == '8' || c == '9' || c == '-',
      "Invalid permlink character: ${s}", ("s", std::string() + c ) );
  }
}

/**
  *  Because net_rshares is 0 there is no need to update any pending payout calculations or parent posts.
  */
void delete_comment_evaluator::do_apply( const delete_comment_operation& o )
{
  auto comment = _db.get_comment( o.author, o.permlink );
  const comment_cashout_object* comment_cashout = _db.find_comment_cashout( *comment );
  if( comment_cashout )
    FC_ASSERT( !comment_cashout->has_replies(), "Cannot delete a comment with replies." );

  //if( _db.has_hardfork( HIVE_HARDFORK_0_19__876 ) )
  /*
    Before `HF19`: `comment_cashout` exists for sure -  following assertion always is true
    After  `HF19`: when `comment_cashout` doesn't exist( it's possible ), then assertion should be triggered
  */
  FC_ASSERT( comment_cashout && "Cannot delete comment after payout." );

  if( _db.has_hardfork( HIVE_HARDFORK_0_19__977 ) )
    FC_ASSERT( comment_cashout->get_net_rshares() <= 0, "Cannot delete a comment with net positive votes." );

  if( comment_cashout->get_net_rshares() > 0 )
  {
    push_virtual_operation( _db, ineffective_delete_comment_operation( o.author, o.permlink ) );
    return;
  }

  const auto& vote_idx = _db.get_index<comment_vote_index, by_comment_voter>();

  auto vote_itr = vote_idx.lower_bound( comment->get_id() );
  while( vote_itr != vote_idx.end() && vote_itr->get_comment() == comment->get_id() )
  {
    const auto& cur_vote = *vote_itr;
    ++vote_itr;
    _db.remove(cur_vote);
  }

  if( !comment->is_root() )
  {
    const comment_cashout_object* parent = _db.find_comment_cashout( comment->get_parent_id() );
    if( parent )
    {
      _db.modify( *parent, [&]( comment_cashout_object& p )
      {
        p.on_reply( _db.head_block_time(), true );
      } );
    }
  }

  if( !_db.has_hardfork( HIVE_HARDFORK_0_19 ) )
  {
    const auto* c_ex = _db.find_comment_cashout_ex( *comment );
    _db.remove( *c_ex );
  }
  _db.remove( *comment_cashout );
  _db.remove( *comment );
}

struct comment_options_extension_visitor
{
  comment_options_extension_visitor( const comment_cashout_object& c, database& db ) : _c( c ), _db( db ) {}

  typedef void result_type;

  const comment_cashout_object& _c;
  database& _db;

#ifdef HIVE_ENABLE_SMT
  void operator()( const allowed_vote_assets& va) const
  {
    FC_ASSERT( !_c.has_votes(), "Comment must not have been voted on before specifying allowed vote assets." );
    auto remaining_asset_number = SMT_MAX_VOTABLE_ASSETS;
    FC_ASSERT( remaining_asset_number > 0 && "Votable assets limit must be positive" );
    _db.modify( _c, [&]( comment_cashout_object& c )
    {
      for( const auto& a : va.votable_assets )
      {
        if( a.first != HIVE_SYMBOL )
        {
          FC_ASSERT( remaining_asset_number > 0, "Comment votable assets number exceeds allowed limit ${ava}.",
                ("ava", SMT_MAX_VOTABLE_ASSETS) );
          --remaining_asset_number;
          c.allowed_vote_assets.emplace_back( a.first, a.second );
        }
      }
    });
  }
#endif

  void operator()( const comment_payout_beneficiaries& cpb ) const
  {
    FC_ASSERT( _c.get_beneficiaries().size() == 0, "Comment already has beneficiaries specified." );
    FC_ASSERT( !_c.has_votes() && "Comment must not have been voted on before specifying beneficiaries." );
    // check below moved from witness plugin, it remains softfork check
    FC_ASSERT( !_db.is_in_control() || cpb.beneficiaries.size() <= HIVE_MAX_COMMENT_BENEFICIARIES,
      "Cannot specify more than ${x} beneficiaries.", ( "x", HIVE_MAX_COMMENT_BENEFICIARIES ) );

    _db.modify( _c, [&]( comment_cashout_object& c )
    {
      for( auto& b : cpb.beneficiaries )
      {
        auto acc = _db.find< account_object, by_name >( b.account );
        FC_ASSERT( acc != nullptr, "Beneficiary \"${a}\" must exist.", ("a", b.account) );
        c.add_beneficiary( *acc, b.weight );
      }
    });
  }
};

void comment_options_evaluator::do_apply( const comment_options_operation& o )
{
  auto comment = _db.get_comment( o.author, o.permlink );

  const comment_cashout_object* comment_cashout = _db.find_comment_cashout( *comment );

  /*
    If `comment_cashout` doesn't exist then setting members needed for payout is not necessary
    Further operations can be skipped
  */
  if( !comment_cashout )
  {
    FC_ASSERT( !_db.has_hardfork( HIVE_HARDFORK_1_24 ), "Updating parameters for comment that is paid out is forbidden." );
    return;
  }

  if( !o.allow_curation_rewards || !o.allow_votes || o.max_accepted_payout < comment_cashout->get_max_accepted_payout() )
    FC_ASSERT( !comment_cashout->has_votes(), "One of the included comment options requires the comment to have no rshares allocated to it." );

  FC_ASSERT( comment_cashout->allows_curation_rewards() >= o.allow_curation_rewards, "Curation rewards cannot be re-enabled." );
  FC_ASSERT( comment_cashout->allows_votes() >= o.allow_votes, "Voting cannot be re-enabled." );
  FC_ASSERT( comment_cashout->get_max_accepted_payout() >= o.max_accepted_payout, "A comment cannot accept a greater payout." );
  FC_ASSERT( comment_cashout->get_percent_hbd() >= o.percent_hbd, "A comment cannot accept a greater percent HBD." );

  _db.modify( *comment_cashout, [&]( comment_cashout_object& c )
  {
    c.configure_options( o.percent_hbd, o.max_accepted_payout, o.allow_votes, o.allow_curation_rewards );
  });

  for( auto& e : o.extensions )
  {
    e.visit( comment_options_extension_visitor( *comment_cashout, _db ) );
  }
}

void comment_evaluator::do_apply( const comment_operation& o )
{ try {
  if( _db.has_hardfork( HIVE_HARDFORK_0_5__55 ) )
    FC_ASSERT( o.title.size() + o.body.size() + o.json_metadata.size(), "Cannot update comment because nothing appears to be changing." );

  const auto& auth = _db.get_account( o.author ); /// prove it exists

  auto _comment = _db.find_comment( auth.get_id(), o.permlink );
  auto _now = _db.head_block_time();

  comment parent;
  if( o.parent_author != HIVE_ROOT_POST_PARENT )
  {
    parent = _db.get_comment( o.parent_author, o.parent_permlink );
    uint16_t depth_limit = !_db.has_hardfork( HIVE_HARDFORK_0_17__767 ) ? HIVE_MAX_COMMENT_DEPTH_PRE_HF17 : HIVE_MAX_COMMENT_DEPTH;
    if( _db.is_in_control() && depth_limit > HIVE_SOFT_MAX_COMMENT_DEPTH )
      depth_limit = HIVE_SOFT_MAX_COMMENT_DEPTH; // soft check moved from witness plugin
    FC_ASSERT( parent->get_depth() < depth_limit,
      "Comment is nested ${x} posts deep, maximum depth is ${y}.", ( "x", parent->get_depth() )( "y", depth_limit ) );
  }

  FC_ASSERT( fc::is_utf8( o.json_metadata ), "JSON Metadata must be UTF-8" );

  if ( !_comment )
  {
    if( parent )
    {
      if( _db.has_hardfork( HIVE_HARDFORK_0_12__177 ) && !_db.has_hardfork( HIVE_HARDFORK_0_17__869 ) )
        FC_ASSERT( _db.calculate_discussion_payout_time( *parent ) != fc::time_point_sec::maximum(), "Discussion is frozen." );
    }

    FC_TODO( "Cleanup this logic after HF 20. Old ops don't need to check pre-hf20 times." )
    if( _db.has_hardfork( HIVE_HARDFORK_0_20__2019 ) )
    {
      if( !parent )
        FC_ASSERT( ( _now - auth.last_root_post ) > HIVE_MIN_ROOT_COMMENT_INTERVAL && "Post HF20", "You may only post once every 5 minutes.", ("now",_now)("last_root_post", auth.last_root_post) );
      else
        FC_ASSERT( ( _now - auth.last_post ) >= HIVE_MIN_REPLY_INTERVAL_HF20, "You may only comment once every 3 seconds.", ("now",_now)("auth.last_post",auth.last_post) );
    }
    else if( _db.has_hardfork( HIVE_HARDFORK_0_12__176 ) )
    {
      if( !parent )
        FC_ASSERT( ( _now - auth.last_root_post ) > HIVE_MIN_ROOT_COMMENT_INTERVAL, "You may only post once every 5 minutes.", ("now",_now)("last_root_post", auth.last_root_post) );
      else
        FC_ASSERT( ( _now - auth.last_post ) > HIVE_MIN_REPLY_INTERVAL && "Post HF12", "You may only comment once every 20 seconds.", ("now",_now)("auth.last_post",auth.last_post) );
    }
    else if( _db.has_hardfork( HIVE_HARDFORK_0_6__113 ) )
    {
      if( !parent )
        FC_ASSERT( ( _now - auth.last_post ) > HIVE_MIN_ROOT_COMMENT_INTERVAL, "You may only post once every 5 minutes.", ("now",_now)("auth.last_post",auth.last_post) );
      else
        FC_ASSERT( ( _now - auth.last_post ) > HIVE_MIN_REPLY_INTERVAL, "You may only comment once every 20 seconds.", ("now",_now)("auth.last_post",auth.last_post) );
    }
    else
    {
      FC_ASSERT( ( _now - auth.last_post ) > fc::seconds(60), "You may only post once per minute.", ("now",_now)("auth.last_post",auth.last_post) );
    }

    uint16_t reward_weight = HIVE_100_PERCENT;
    uint64_t post_bandwidth = auth.post_bandwidth;

    if( _db.has_hardfork( HIVE_HARDFORK_0_12__176 ) && !_db.has_hardfork( HIVE_HARDFORK_0_17__733 ) && !parent )
    {
      uint64_t post_delta_time = std::min( _now.sec_since_epoch() - auth.last_root_post.sec_since_epoch(), HIVE_POST_AVERAGE_WINDOW );
      uint32_t old_weight = uint32_t( ( post_bandwidth * ( HIVE_POST_AVERAGE_WINDOW - post_delta_time ) ) / HIVE_POST_AVERAGE_WINDOW );
      post_bandwidth = ( old_weight + HIVE_100_PERCENT );
      reward_weight = uint16_t( std::min( ( HIVE_POST_WEIGHT_CONSTANT * HIVE_100_PERCENT ) / ( post_bandwidth * post_bandwidth ), uint64_t( HIVE_100_PERCENT ) ) );
    }

    _db.modify( auth, [&]( account_object& a )
    {
      if( !parent )
      {
        a.last_root_post = _now;
        a.post_bandwidth = uint32_t( post_bandwidth );
      }
      a.last_post = _now;
      a.last_post_edit = _now;
      a.post_count++;
    });

    if( _db.has_hardfork( HIVE_HARDFORK_0_1 ) )
    {
      validate_permlink_0_1( o.parent_permlink );
      validate_permlink_0_1( o.permlink );
    }

    const auto& new_comment = _db.create< comment_object >( auth, o.permlink, parent.get() );

    fc::time_point_sec cashout_time;
    if( _db.has_hardfork( HIVE_HARDFORK_0_17__769 ) )
      cashout_time = _now + HIVE_CASHOUT_WINDOW_SECONDS;
    else if( !parent && _db.has_hardfork( HIVE_HARDFORK_0_12__177 ) )
      cashout_time = _now + HIVE_CASHOUT_WINDOW_SECONDS_PRE_HF17;
    else
      cashout_time = fc::time_point_sec::maximum();

    _db.create< comment_cashout_object >( new_comment, auth, o.permlink, _now, cashout_time );
    if( !_db.has_hardfork( HIVE_HARDFORK_0_19 ) )
    {
      fc::optional< std::reference_wrapper< const comment_cashout_ex_object > > parent_comment_cashout_ex;
      if( parent )
        parent_comment_cashout_ex = *( _db.find_comment_cashout_ex( *parent ) );
      _db.create< comment_cashout_ex_object >( new_comment, parent_comment_cashout_ex, reward_weight );
    }

    if( parent )
    {
      const comment_cashout_object* parent_cashout = _db.find_comment_cashout( *parent );
      if( parent_cashout )
      {
        _db.modify( *parent_cashout, [&]( comment_cashout_object& p )
        {
          p.on_reply( _now );
        } );
      }
    }

  }
  else // start edit case
  {
    if( _db.has_hardfork( HIVE_HARDFORK_0_21__3313 ) )
    {
      FC_ASSERT( _now - auth.last_post_edit >= HIVE_MIN_COMMENT_EDIT_INTERVAL, "Can only perform one comment edit per block." );
    }

    if( !_db.has_hardfork( HIVE_HARDFORK_0_17__772 ) )
    {
      const comment_cashout_object* comment_cashout = _db.find_comment_cashout( *_comment );
      FC_ASSERT( comment_cashout && "Comment cashout object must exist" );
      if( _db.has_hardfork( HIVE_HARDFORK_0_14__306 ) )
        FC_ASSERT( _db.calculate_discussion_payout_time( *_comment, *comment_cashout ) != fc::time_point_sec::maximum(), "The comment is archived." );
      else if( _db.has_hardfork( HIVE_HARDFORK_0_10 ) )
        FC_ASSERT( !_db.find_comment_cashout_ex( *_comment )->was_paid(), "Can only edit during the first 24 hours." );
    }

    if( !parent )
    {
      FC_ASSERT( _comment->is_root(), "The parent of a comment cannot change." );
      //note for HiveMind: if someone tries to change 'category' (that no longer is part of consensus)
      //by providing different 'o.parent_permlink' than before, such change should be silently ignored
    }
    else if ( _db.has_hardfork( HIVE_HARDFORK_0_21__2203 ) )
    {
      //ABW: see creation tx 11ad62ee8f8e892cd5bd75fc2d3098427f7e47ac and edit tx dca209592c7129be36b069d033dfdb0f1f143b4e
      //both happened prior to HF21 when check was slightly more relaxed
      FC_ASSERT( _comment->get_parent_id() == parent->get_id(), "The parent of a comment cannot change." );
    }

    _db.modify( auth, [&]( account_object& a )
    {
      a.last_post_edit = _now;
    });

  } // end EDIT case

} FC_CAPTURE_AND_RETHROW( (o) ) }

void pre_hf20_vote_evaluator( const vote_operation& o, database& _db )
{
  auto comment = _db.get_comment( o.author, o.permlink );
  const comment_cashout_object* comment_cashout = _db.find_comment_cashout( *comment );

  const auto& voter = _db.get_account( o.voter );

  FC_ASSERT( voter.can_vote && "Voter has declined their voting rights." );

  if( comment_cashout )
  {
    if( o.weight > 0 ) FC_ASSERT( comment_cashout->allows_votes(), "Votes are not allowed on the comment." );
  }

  if( !comment_cashout || ( _db.has_hardfork( HIVE_HARDFORK_0_12__177 ) &&
    _db.calculate_discussion_payout_time( *comment, *comment_cashout ) == fc::time_point_sec::maximum() ) )
  {
    return; // comment already paid
  }

  auto _now = _db.head_block_time();

  int64_t current_power = 0;
  {
    int64_t elapsed_seconds = _now.sec_since_epoch() - voter.voting_manabar.last_update_time;
    if( _db.has_hardfork( HIVE_HARDFORK_0_11 ) )
      FC_ASSERT( elapsed_seconds >= HIVE_MIN_VOTE_INTERVAL_SEC, "Can only vote once every 3 seconds." );
    int64_t regenerated_power = (HIVE_100_PERCENT * elapsed_seconds) / HIVE_VOTING_MANA_REGENERATION_SECONDS;
    current_power = std::min( int64_t(voter.voting_manabar.current_mana) + regenerated_power, int64_t(HIVE_100_PERCENT) );
    FC_ASSERT( current_power > 0, "Account currently does not have voting power." );
  }
  int64_t abs_weight = abs(o.weight);
  // Less rounding error would occur if we did the division last, but we need to maintain backward
  // compatibility with the previous implementation which was replaced in #1285
  int64_t used_power = ((current_power * abs_weight) / HIVE_100_PERCENT) * (60*60*24);

  const dynamic_global_property_object& dgpo = _db.get_dynamic_global_properties();

  // The second multiplication is rounded up as of HF14#259
  int64_t max_vote_denom = dgpo.vote_power_reserve_rate * HIVE_VOTING_MANA_REGENERATION_SECONDS;
  FC_ASSERT( max_vote_denom > 0 && "Pre HF20" );

  if( !_db.has_hardfork( HIVE_HARDFORK_0_14__259 ) )
  {
    used_power = (used_power / max_vote_denom)+1;
  }
  else
  {
    used_power = (used_power + max_vote_denom - 1) / max_vote_denom;
  }
  FC_ASSERT( used_power <= current_power, "Account does not have enough power to vote." );

  int64_t abs_rshares = fc::uint128_to_uint64((uint128_t( voter.get_effective_vesting_shares( false ).value ) * used_power) / (HIVE_100_PERCENT));
  if( !_db.has_hardfork( HIVE_HARDFORK_0_14__259 ) && abs_rshares == 0 ) abs_rshares = 1;

  if( _db.has_hardfork( HIVE_HARDFORK_0_14__259 ) )
  {
    FC_ASSERT( abs_rshares > HIVE_VOTE_DUST_THRESHOLD || o.weight == 0, "Voting weight is too small, please accumulate more voting power or Hive Power." );
  }
  else if( _db.has_hardfork( HIVE_HARDFORK_0_13__248 ) )
  {
    FC_ASSERT( abs_rshares > HIVE_VOTE_DUST_THRESHOLD || abs_rshares == 1, "Voting weight is too small, please accumulate more voting power or Hive Power." );
  }

  const auto& comment_vote_idx = _db.get_index< comment_vote_index, by_comment_voter >();
  auto itr = comment_vote_idx.find( boost::make_tuple( comment->get_id(), voter.get_id() ) );

  /// this is the rshares voting for or against the post
  int64_t rshares = o.weight < 0 ? -abs_rshares : abs_rshares;
  {
    auto previous_vote_rshares = ( itr == comment_vote_idx.end() ? 0 : itr->get_rshares() );
    if( rshares > previous_vote_rshares )
    {
      if( _db.has_hardfork( HIVE_HARDFORK_0_17__900 ) )
        FC_ASSERT( _now < comment_cashout->get_cashout_time() - HIVE_UPVOTE_LOCKOUT_HF17, "Cannot increase payout within last twelve hours before payout." );
      else if( _db.has_hardfork( HIVE_HARDFORK_0_7 ) )
        FC_ASSERT( _now < _db.calculate_discussion_payout_time( *comment, *comment_cashout ) - HIVE_UPVOTE_LOCKOUT_HF7, "Cannot increase payout within last minute before payout." );
    }
  }

  _db.modify( voter, [&]( account_object& a )
  {
    a.voting_manabar.current_mana = current_power - used_power; // always nonnegative
    a.last_vote_time = _now;
    a.voting_manabar.last_update_time = _now.sec_since_epoch();
  } );

  /// if the current net_rshares is less than 0, the post is getting 0 rewards so it is not factored into total rshares^2
  fc::uint128_t old_rshares = std::max( comment_cashout->get_net_rshares(), int64_t( 0 ) );
  const auto* comment_cashout_ex = _db.find_comment_cashout_ex( *comment );

  if( !_db.has_hardfork( HIVE_HARDFORK_0_17__769 ) )
  {
    const auto& root = _db.get( comment_cashout_ex->get_root_id() );
    const comment_cashout_object* root_cashout = _db.find_comment_cashout( root );
    const comment_cashout_ex_object* root_cashout_ex = _db.find_comment_cashout_ex( root );
    FC_ASSERT( root_cashout && root_cashout_ex );

    _db.modify( *root_cashout, [&]( comment_cashout_object& c )
    {
      auto old_root_abs_rshares = root_cashout_ex->get_children_abs_rshares();
      _db.modify( *root_cashout_ex, [&]( comment_cashout_ex_object& c_ex )
      {
        c_ex.accumulate_children_abs_rshares( abs_rshares );
      } );

      if( _db.has_hardfork( HIVE_HARDFORK_0_12__177 ) && root_cashout_ex->was_paid() )
      {
        c.set_cashout_time( root_cashout_ex->get_last_payout() + HIVE_SECOND_CASHOUT_WINDOW );
      }
      else
      {
        fc::uint128_t cur_cashout_time_sec = _db.calculate_discussion_payout_time( *comment, *comment_cashout ).sec_since_epoch();
        fc::uint128_t avg_cashout_sec = 0;
        if( _db.has_hardfork( HIVE_HARDFORK_0_14__259 ) && abs_rshares == 0 )
        {
          avg_cashout_sec = cur_cashout_time_sec;
        }
        else
        {
          fc::uint128_t new_cashout_time_sec = 0;
          if( _db.has_hardfork( HIVE_HARDFORK_0_12__177 ) && !_db.has_hardfork( HIVE_HARDFORK_0_13__257 ) )
            new_cashout_time_sec = _now.sec_since_epoch() + HIVE_CASHOUT_WINDOW_SECONDS_PRE_HF17;
          else
            new_cashout_time_sec = _now.sec_since_epoch() + HIVE_CASHOUT_WINDOW_SECONDS_PRE_HF12;
          avg_cashout_sec = ( cur_cashout_time_sec * old_root_abs_rshares + new_cashout_time_sec * abs_rshares ) / root_cashout_ex->get_children_abs_rshares();
        }
        c.set_cashout_time( fc::time_point_sec( std::min( uint32_t( fc::uint128_to_uint64(avg_cashout_sec) ), root_cashout_ex->get_max_cashout_time().sec_since_epoch() ) ) );
      }

      if( root_cashout_ex->get_max_cashout_time() == fc::time_point_sec::maximum() )
      {
        _db.modify( *root_cashout_ex, [&]( comment_cashout_ex_object& c_ex )
        {
          c_ex.set_max_cashout_time( _now + fc::seconds( HIVE_MAX_CASHOUT_WINDOW_SECONDS ) );
        } );
      }
    } );
  }

  uint64_t vote_weight = 0;

  if( itr == comment_vote_idx.end() ) // new vote
  {
    FC_ASSERT( o.weight != 0, "Vote weight cannot be 0." );
    FC_ASSERT( abs_rshares > 0, "Cannot vote with 0 rshares." );

    auto old_vote_rshares = comment_cashout->get_vote_rshares();

    _db.modify( *comment_cashout, [&]( comment_cashout_object& c )
    {
      c.on_vote( o.weight );
      c.accumulate_vote_rshares( rshares, rshares > 0 ? rshares : 0 );
    });
    if( comment_cashout_ex )
    {
      _db.modify( *comment_cashout_ex, [&]( comment_cashout_ex_object& c_ex )
      {
        c_ex.accumulate_abs_rshares( abs_rshares );
      } );
    }

    /** this verifies uniqueness of voter
      *
      *  cv.weight / c.total_vote_weight ==> % of rshares increase that is accounted for by the vote
      *
      *  W(R) = B * R / ( R + 2S )
      *  W(R) is bounded above by B. B is fixed at 2^64 - 1, so all weights fit in a 64 bit integer.
      *
      *  The equation for an individual vote is:
      *    W(R_N) - W(R_N-1), which is the delta increase of proportional weight
      *
      *  c.total_vote_weight =
      *    W(R_1) - W(R_0) +
      *    W(R_2) - W(R_1) + ...
      *    W(R_N) - W(R_N-1) = W(R_N) - W(R_0)
      *
      *  Since W(R_0) = 0, c.total_vote_weight is also bounded above by B and will always fit in a 64 bit integer.
      *
    **/
    uint64_t max_vote_weight = 0;
    {
      bool curation_reward_eligible = rshares > 0 && comment_cashout->allows_curation_rewards();
      if( curation_reward_eligible && comment_cashout_ex )
        curation_reward_eligible = !comment_cashout_ex->was_paid();
      if( curation_reward_eligible && _db.has_hardfork( HIVE_HARDFORK_0_17__774 ) )
        curation_reward_eligible = _db.get_curation_rewards_percent() > 0;

      if( curation_reward_eligible )
      {
        if( comment_cashout->get_creation_time() < fc::time_point_sec(HIVE_HARDFORK_0_6_REVERSE_AUCTION_TIME) )
        {
          u512 rshares3(rshares);
          u256 total2( comment_cashout_ex->get_abs_rshares() );

          if( !_db.has_hardfork( HIVE_HARDFORK_0_1 ) )
          {
            rshares3 *= 1000000;
            total2 *= 1000000;
          }

          rshares3 = rshares3 * rshares3 * rshares3;

          total2 *= total2;
          vote_weight = static_cast<uint64_t>( rshares3 / total2 );
        } else {// cv.weight = W(R_1) - W(R_0)
          const uint128_t two_s = 2 * util::get_content_constant_s();
          if( _db.has_hardfork( HIVE_HARDFORK_0_17__774 ) )
          {
            const auto& reward_fund = _db.get_reward_fund();
            auto curve = !_db.has_hardfork( HIVE_HARDFORK_0_19__1052 ) && comment_cashout->get_creation_time() > HIVE_HF_19_SQRT_PRE_CALC
                      ? curve_id::square_root : reward_fund.curation_reward_curve;
            uint64_t old_weight = fc::uint128_to_uint64(util::evaluate_reward_curve( old_vote_rshares, curve, reward_fund.content_constant ));
            uint64_t new_weight = fc::uint128_to_uint64(util::evaluate_reward_curve( comment_cashout->get_vote_rshares(), curve, reward_fund.content_constant ));
            vote_weight = new_weight - old_weight;
          }
          else if ( _db.has_hardfork( HIVE_HARDFORK_0_1 ) )
          {
            uint64_t old_weight = fc::uint128_to_uint64( ( std::numeric_limits< uint64_t >::max() * fc::uint128_t( old_vote_rshares ) ) / ( two_s + old_vote_rshares ) );
            uint64_t new_weight = fc::uint128_to_uint64( ( std::numeric_limits< uint64_t >::max() * fc::uint128_t( comment_cashout->get_vote_rshares() ) ) / ( two_s + comment_cashout->get_vote_rshares() ) );
            vote_weight = new_weight - old_weight;
          }
          else
          {
            uint64_t old_weight = fc::uint128_to_uint64( ( std::numeric_limits< uint64_t >::max() * fc::uint128_t( 1000000 * old_vote_rshares ) ) / ( two_s + ( 1000000 * old_vote_rshares ) ) );
            uint64_t new_weight = fc::uint128_to_uint64( ( std::numeric_limits< uint64_t >::max() * fc::uint128_t( 1000000 * comment_cashout->get_vote_rshares() ) ) / ( two_s + ( 1000000 * comment_cashout->get_vote_rshares() ) ) );
            vote_weight = new_weight - old_weight;
          }
        }

        max_vote_weight = vote_weight;

        if( _now > fc::time_point_sec(HIVE_HARDFORK_0_6_REVERSE_AUCTION_TIME) )  /// start enforcing this prior to the hardfork
        {
          /// discount weight by time
          uint128_t w(max_vote_weight);
          uint64_t delta_t = std::min( uint64_t((_now - comment_cashout->get_creation_time()).to_seconds()), dgpo.reverse_auction_seconds );

          w *= delta_t;
          w /= dgpo.reverse_auction_seconds;
          vote_weight = fc::uint128_to_uint64(w);
        }
      }
    }

    _db.create<comment_vote_object>( voter, *comment, _now, o.weight, vote_weight, rshares );

    if( max_vote_weight ) // Optimization
    {
      _db.modify( *comment_cashout, [&]( comment_cashout_object& c )
      {
        c.accumulate_vote_weight( max_vote_weight );
      });
    }
  }
  else // edit of existing vote
  {
    FC_ASSERT( itr->get_number_of_changes() < HIVE_MAX_VOTE_CHANGES, "Voter has used the maximum number of vote changes on this comment." );

    if( _db.has_hardfork( HIVE_HARDFORK_0_6__112 ) )
      FC_ASSERT( itr->get_vote_percent() != o.weight && "You have already voted in a similar way." );

    _db.modify( *comment_cashout, [&]( comment_cashout_object& c )
    {
      c.on_vote( o.weight, itr->get_vote_percent() );
      c.accumulate_vote_rshares( rshares - itr->get_rshares(), 0 );
      c.accumulate_vote_weight( -itr->get_weight() );
    } );
    if( comment_cashout_ex )
    {
      _db.modify( *comment_cashout_ex, [&]( comment_cashout_ex_object& c_ex )
      {
        c_ex.accumulate_abs_rshares( abs_rshares );
      } );
    }

    _db.modify( *itr, [&]( comment_vote_object& cv )
    {
      cv.set( _now, o.weight, 0, rshares );
    } );
  }

  if( !_db.has_hardfork( HIVE_HARDFORK_0_17__774 ) )
  {
    fc::uint128_t new_rshares = std::max( comment_cashout->get_net_rshares(), int64_t( 0 ) );
    new_rshares = util::evaluate_reward_curve( new_rshares );
    old_rshares = util::evaluate_reward_curve( old_rshares );
    _db.adjust_rshares2( old_rshares, new_rshares );
  }

  push_virtual_operation( _db, effective_comment_vote_operation( o.voter, o.author, o.permlink, vote_weight, rshares, comment_cashout->get_total_vote_weight() ) );
}

void hf20_vote_evaluator( const vote_operation& o, database& _db )
{
  auto comment = _db.get_comment( o.author, o.permlink );
  const comment_cashout_object* comment_cashout = _db.find_comment_cashout( *comment );

  const auto& voter   = _db.get_account( o.voter );
  const auto& dgpo    = _db.get_dynamic_global_properties();

  FC_ASSERT( voter.can_vote, "Voter has declined their voting rights." );

  if( comment_cashout )
  {
    if( o.weight > 0 ) FC_ASSERT( comment_cashout->allows_votes() && "Votes are not allowed on the comment." );
  }

  if( !comment_cashout || _db.calculate_discussion_payout_time( *comment, *comment_cashout ) == fc::time_point_sec::maximum() )
  {
    return; // comment already paid
  }

  auto _now = _db.head_block_time();
  FC_ASSERT( _now < comment_cashout->get_cashout_time(), "Comment is actively being rewarded. Cannot vote on comment." );
  if( !_db.has_hardfork( HIVE_HARDFORK_1_26_NO_VOTE_COOLDOWN ) )
    FC_ASSERT( ( _now - voter.last_vote_time ).to_seconds() >= HIVE_MIN_VOTE_INTERVAL_SEC, "Can only vote once every 3 seconds." );

  const auto& comment_vote_idx = _db.get_index< comment_vote_index, by_comment_voter >();
  auto itr = comment_vote_idx.find( boost::make_tuple( comment->get_id(), voter.get_id() ) );

  int16_t previous_vote_percent = 0;
  int64_t previous_rshares = 0;
  int64_t previous_positive_rshares = 0;
  uint64_t previous_vote_weight = 0;
  if( itr == comment_vote_idx.end() )
  {
    FC_ASSERT( o.weight != 0 && "Vote weight cannot be 0." );
  }
  else
  {
    if( !_db.has_hardfork( HIVE_HARDFORK_1_28_NO_VOTE_LIMIT ) )
      FC_ASSERT( itr->get_number_of_changes() < HIVE_MAX_VOTE_CHANGES && "Voter has used the maximum number of vote changes on this comment." );
    FC_ASSERT( itr->get_vote_percent() != o.weight && "Your current vote on this comment is identical to this vote." );
    previous_vote_percent = itr->get_vote_percent();
    previous_rshares = itr->get_rshares();
    if( previous_rshares > 0 )
      previous_positive_rshares = previous_rshares;
    previous_vote_weight = itr->get_weight();
  }

  _db.modify( voter, [&]( account_object& a )
  {
    util::update_manabar( dgpo, a );
  });

  int16_t abs_weight = abs( o.weight );
  uint128_t used_mana = 0;

  if( _db.has_hardfork( HIVE_HARDFORK_1_28_STABLE_VOTE ) )
  {
    used_mana = ( uint128_t( voter.get_effective_vesting_shares().value ) * abs_weight * 60 * 60 * 24 ) / HIVE_100_PERCENT;
  }
  else if( dgpo.downvote_pool_percent && o.weight < 0 )
  {
    if( _db.has_hardfork( HIVE_HARDFORK_0_22__3485 ) )
    {
      used_mana = ( std::max( ( ( uint128_t( voter.downvote_manabar.current_mana ) * HIVE_100_PERCENT ) / dgpo.downvote_pool_percent ),
                      uint128_t( voter.voting_manabar.current_mana ) )
            * abs_weight * 60 * 60 * 24 ) / HIVE_100_PERCENT;
    }
    else
    {
      used_mana = ( std::max( ( uint128_t( voter.downvote_manabar.current_mana * HIVE_100_PERCENT ) / dgpo.downvote_pool_percent ),
                      uint128_t( voter.voting_manabar.current_mana ) )
            * abs_weight * 60 * 60 * 24 ) / HIVE_100_PERCENT;
    }
  }
  else
  {
    used_mana = ( uint128_t( voter.voting_manabar.current_mana ) * abs_weight * 60 * 60 * 24 ) / HIVE_100_PERCENT;
  }

  int64_t max_vote_denom = dgpo.vote_power_reserve_rate * HIVE_VOTING_MANA_REGENERATION_SECONDS;
  FC_ASSERT( max_vote_denom > 0 );

  used_mana = ( used_mana + max_vote_denom - 1 ) / max_vote_denom;
  if( dgpo.downvote_pool_percent && o.weight < 0 )
  {
    // note that downvote requires more mana than necessary, which prevents accounts with no stake from downvoting;
    // while the effect might be unintentional, it was like that for long time and there is enough drama with
    // downvotes as it is, enabling "no effect" downvotes is not necessary, so we are not correcting it
    FC_ASSERT( voter.voting_manabar.current_mana + voter.downvote_manabar.current_mana > fc::uint128_to_int64( used_mana ),
      "Account does not have enough mana to downvote. voting_mana: ${v} downvote_mana: ${d} required_mana: ${r}",
      ( "v", voter.voting_manabar.current_mana )( "d", voter.downvote_manabar.current_mana )( "r", fc::uint128_to_int64( used_mana ) ) );
  }
  else
  {
    // even after HF28 it is not possible to burn all mana in one 50 vote transaction due to "round up" code above
    FC_ASSERT( voter.voting_manabar.has_mana( fc::uint128_to_int64( used_mana ) ),
      "Account does not have enough mana to vote. voting_mana: ${v} required_mana: ${r}",
      ( "v", voter.voting_manabar.current_mana )( "r", fc::uint128_to_int64( used_mana ) ) );
  }

  int64_t abs_rshares = fc::uint128_to_int64(used_mana);

  abs_rshares -= HIVE_VOTE_DUST_THRESHOLD;
  abs_rshares = std::max( int64_t(0), abs_rshares );

  uint32_t cashout_delta = ( comment_cashout->get_cashout_time() - _now ).to_seconds();

  if( cashout_delta < HIVE_UPVOTE_LOCKOUT_SECONDS )
  {
    abs_rshares = (int64_t) fc::uint128_to_uint64( ( uint128_t( abs_rshares ) * cashout_delta ) / HIVE_UPVOTE_LOCKOUT_SECONDS );
  }

  _db.modify( voter, [&]( account_object& a )
  {
    if( dgpo.downvote_pool_percent && o.weight < 0 )
    {
      if( fc::uint128_to_int64(used_mana) > a.downvote_manabar.current_mana )
      {
        /* used mana is always less than downvote_mana + voting_mana because the amount used
          * is a fraction of max( downvote_mana, voting_mana ). If more mana is consumed than
          * there is downvote_mana, then it is because voting_mana is greater, and used_mana
          * is strictly smaller than voting_mana. This is the same reason why a check is not
          * required when using voting mana on its own as an upvote.
          */
        auto remainder = fc::uint128_to_int64(used_mana) - a.downvote_manabar.current_mana;
        a.downvote_manabar.use_mana( a.downvote_manabar.current_mana );
        a.voting_manabar.use_mana( remainder );
      }
      else
      {
        a.downvote_manabar.use_mana( fc::uint128_to_int64(used_mana) );
      }
    }
    else
    {
      a.voting_manabar.use_mana( fc::uint128_to_int64(used_mana) );
    }

    a.last_vote_time = _now;
  } );

  /// this is the rshares voting for or against the post
  int64_t rshares = o.weight < 0 ? -abs_rshares : abs_rshares;
  uint64_t vote_weight = 0;

  _db.modify( *comment_cashout, [&]( comment_cashout_object& c )
  {
    c.on_vote( o.weight, previous_vote_percent, abs_rshares != 0 || _db.has_hardfork( HIVE_HARDFORK_1_26_DUST_VOTE_FIX ) );

    auto old_vote_rshares = comment_cashout->get_vote_rshares();
    uint64_t max_vote_weight = 0;
    if( itr == comment_vote_idx.end() || _db.has_hardfork( HIVE_HARDFORK_1_26_NO_VOTE_EDIT_PENALTY ) )
    {
      c.accumulate_vote_rshares( rshares - previous_rshares, ( rshares > 0 ? rshares : 0 ) - previous_positive_rshares );
      if( _db.has_hardfork( HIVE_HARDFORK_1_26_NO_VOTE_EDIT_PENALTY ) )
        old_vote_rshares -= previous_positive_rshares;

      /** this verifies uniqueness of voter
        *
        *  cv.weight / c.total_vote_weight ==> % of rshares increase that is accounted for by the vote
        *
        *  W(R) = B * R / ( R + 2S )
        *  W(R) is bounded above by B. B is fixed at 2^64 - 1, so all weights fit in a 64 bit integer.
        *
        *  The equation for an individual vote is:
        *    W(R_N) - W(R_N-1), which is the delta increase of proportional weight
        *
        *  c.total_vote_weight =
        *    W(R_1) - W(R_0) +
        *    W(R_2) - W(R_1) + ...
        *    W(R_N) - W(R_N-1) = W(R_N) - W(R_0)
        *
        *  Since W(R_0) = 0, c.total_vote_weight is also bounded above by B and will always fit in a 64 bit integer.
        *
      **/
      {
        bool curation_reward_eligible = rshares > 0 && comment_cashout->allows_curation_rewards() &&
          ( _db.get_curation_rewards_percent() > 0 );

        if( curation_reward_eligible )
        {
          // cv.weight = W(R_1) - W(R_0)
          const auto& reward_fund = _db.get_reward_fund();
          auto curve = reward_fund.curation_reward_curve;
          uint64_t old_weight = fc::uint128_to_uint64(util::evaluate_reward_curve( old_vote_rshares, curve, reward_fund.content_constant ));
          uint64_t new_weight = fc::uint128_to_uint64(util::evaluate_reward_curve( c.get_vote_rshares(), curve, reward_fund.content_constant ));

          if( old_weight < new_weight ) // old_weight > new_weight should never happen, but == is ok
          {
            uint64_t _seconds = ( _now - c.get_creation_time() ).to_seconds();

            vote_weight = new_weight - old_weight;

            //In HF25 `dgpo.reverse_auction_seconds` is set to zero. It's replaced by `dgpo.early_voting_seconds` and `dgpo.mid_voting_seconds`.
            if( _seconds < dgpo.reverse_auction_seconds )
            {
              max_vote_weight = vote_weight;

              /// discount weight by time
              uint128_t w( max_vote_weight );
              uint64_t delta_t = std::min( _seconds, uint64_t( dgpo.reverse_auction_seconds ) );

              w *= delta_t;
              w /= dgpo.reverse_auction_seconds;
              vote_weight = fc::uint128_to_uint64(w);
            }
            else if( _seconds >= dgpo.early_voting_seconds && dgpo.early_voting_seconds )
            {
              //Following values are chosen empirically
              const uint32_t phase_1_factor = 2;
              const uint32_t phase_2_factor = 8;

              if( _seconds < ( dgpo.early_voting_seconds + dgpo.mid_voting_seconds ) )
                vote_weight /= phase_1_factor;
              else
                vote_weight /= phase_2_factor;

              max_vote_weight = vote_weight;
            }
            else
            {
              max_vote_weight = vote_weight;
            }
          }
        }
      }
    }
    else // pre-HF26 vote edit
    {
      c.accumulate_vote_rshares( rshares - previous_rshares, 0 );
    }

    c.accumulate_vote_weight( max_vote_weight - previous_vote_weight );
  } );

  if( itr == comment_vote_idx.end() ) // new vote
  {
    _db.create<comment_vote_object>( voter, *comment, _now, o.weight, vote_weight, rshares );
  }
  else // edit of existing vote
  {
    _db.modify( *itr, [&]( comment_vote_object& cv )
    {
      cv.set( _now, o.weight, vote_weight, rshares );
    });
  }

  push_virtual_operation( _db, effective_comment_vote_operation( o.voter, o.author, o.permlink, vote_weight, rshares, comment_cashout->get_total_vote_weight() ) );
}

void vote_evaluator::do_apply( const vote_operation& o )
{ try {
  if( _db.has_hardfork( HIVE_HARDFORK_0_20__2539 ) )
  {
    hf20_vote_evaluator( o, _db );
  }
  else
  {
    pre_hf20_vote_evaluator( o, _db );
  }
} FC_CAPTURE_AND_RETHROW( (o) ) }

} } // hive::chain
