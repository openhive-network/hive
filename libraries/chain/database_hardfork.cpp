#include <hive/chain/hive_fwd.hpp>

#include <hive/chain/database.hpp>
#include <hive/chain/database_exceptions.hpp>
#include <hive/chain/account_object_multiindex.hpp>
#include <hive/chain/global_property_object_multiindex.hpp>
#include <hive/chain/hardfork_property_object_multiindex.hpp>
#include <hive/chain/comment_object_multiindex.hpp>
#include <hive/chain/detail/state/feed_history_object_multiindex.hpp>
#include <hive/chain/detail/state/liquidity_reward_balance_object_multiindex.hpp>
#include <hive/chain/detail/state/reward_fund_object_multiindex.hpp>
#include <hive/chain/witness_objects_multiindex.hpp>
#include <hive/chain/witness_schedule.hpp>
#include <hive/chain/smt_objects.hpp>

#include <hive/chain/rc/rc_objects.hpp>

#include <hive/protocol/get_config.hpp>
#include <hive/protocol/hardfork.hpp>

#include <hive/jsonball/jsonball.hpp>

namespace hive { namespace chain {

void database::init_hardforks()
{
  _hardfork_versions.times[ 0 ] = fc::time_point_sec( HIVE_GENESIS_TIME );
  _hardfork_versions.versions[ 0 ] = hardfork_version( 0, 0 );
  FC_ASSERT( HIVE_HARDFORK_0_1 == 1, "Invalid hardfork configuration" );
  _hardfork_versions.times[ HIVE_HARDFORK_0_1 ] = fc::time_point_sec( HIVE_HARDFORK_0_1_TIME );
  _hardfork_versions.versions[ HIVE_HARDFORK_0_1 ] = HIVE_HARDFORK_0_1_VERSION;
  FC_ASSERT( HIVE_HARDFORK_0_2 == 2, "Invlaid hardfork configuration" );
  _hardfork_versions.times[ HIVE_HARDFORK_0_2 ] = fc::time_point_sec( HIVE_HARDFORK_0_2_TIME );
  _hardfork_versions.versions[ HIVE_HARDFORK_0_2 ] = HIVE_HARDFORK_0_2_VERSION;
  FC_ASSERT( HIVE_HARDFORK_0_3 == 3, "Invalid hardfork configuration" );
  _hardfork_versions.times[ HIVE_HARDFORK_0_3 ] = fc::time_point_sec( HIVE_HARDFORK_0_3_TIME );
  _hardfork_versions.versions[ HIVE_HARDFORK_0_3 ] = HIVE_HARDFORK_0_3_VERSION;
  FC_ASSERT( HIVE_HARDFORK_0_4 == 4, "Invalid hardfork configuration" );
  _hardfork_versions.times[ HIVE_HARDFORK_0_4 ] = fc::time_point_sec( HIVE_HARDFORK_0_4_TIME );
  _hardfork_versions.versions[ HIVE_HARDFORK_0_4 ] = HIVE_HARDFORK_0_4_VERSION;
  FC_ASSERT( HIVE_HARDFORK_0_5 == 5, "Invalid hardfork configuration" );
  _hardfork_versions.times[ HIVE_HARDFORK_0_5 ] = fc::time_point_sec( HIVE_HARDFORK_0_5_TIME );
  _hardfork_versions.versions[ HIVE_HARDFORK_0_5 ] = HIVE_HARDFORK_0_5_VERSION;
  FC_ASSERT( HIVE_HARDFORK_0_6 == 6, "Invalid hardfork configuration" );
  _hardfork_versions.times[ HIVE_HARDFORK_0_6 ] = fc::time_point_sec( HIVE_HARDFORK_0_6_TIME );
  _hardfork_versions.versions[ HIVE_HARDFORK_0_6 ] = HIVE_HARDFORK_0_6_VERSION;
  FC_ASSERT( HIVE_HARDFORK_0_7 == 7, "Invalid hardfork configuration" );
  _hardfork_versions.times[ HIVE_HARDFORK_0_7 ] = fc::time_point_sec( HIVE_HARDFORK_0_7_TIME );
  _hardfork_versions.versions[ HIVE_HARDFORK_0_7 ] = HIVE_HARDFORK_0_7_VERSION;
  FC_ASSERT( HIVE_HARDFORK_0_8 == 8, "Invalid hardfork configuration" );
  _hardfork_versions.times[ HIVE_HARDFORK_0_8 ] = fc::time_point_sec( HIVE_HARDFORK_0_8_TIME );
  _hardfork_versions.versions[ HIVE_HARDFORK_0_8 ] = HIVE_HARDFORK_0_8_VERSION;
  FC_ASSERT( HIVE_HARDFORK_0_9 == 9, "Invalid hardfork configuration" );
  _hardfork_versions.times[ HIVE_HARDFORK_0_9 ] = fc::time_point_sec( HIVE_HARDFORK_0_9_TIME );
  _hardfork_versions.versions[ HIVE_HARDFORK_0_9 ] = HIVE_HARDFORK_0_9_VERSION;
  FC_ASSERT( HIVE_HARDFORK_0_10 == 10, "Invalid hardfork configuration" );
  _hardfork_versions.times[ HIVE_HARDFORK_0_10 ] = fc::time_point_sec( HIVE_HARDFORK_0_10_TIME );
  _hardfork_versions.versions[ HIVE_HARDFORK_0_10 ] = HIVE_HARDFORK_0_10_VERSION;
  FC_ASSERT( HIVE_HARDFORK_0_11 == 11, "Invalid hardfork configuration" );
  _hardfork_versions.times[ HIVE_HARDFORK_0_11 ] = fc::time_point_sec( HIVE_HARDFORK_0_11_TIME );
  _hardfork_versions.versions[ HIVE_HARDFORK_0_11 ] = HIVE_HARDFORK_0_11_VERSION;
  FC_ASSERT( HIVE_HARDFORK_0_12 == 12, "Invalid hardfork configuration" );
  _hardfork_versions.times[ HIVE_HARDFORK_0_12 ] = fc::time_point_sec( HIVE_HARDFORK_0_12_TIME );
  _hardfork_versions.versions[ HIVE_HARDFORK_0_12 ] = HIVE_HARDFORK_0_12_VERSION;
  FC_ASSERT( HIVE_HARDFORK_0_13 == 13, "Invalid hardfork configuration" );
  _hardfork_versions.times[ HIVE_HARDFORK_0_13 ] = fc::time_point_sec( HIVE_HARDFORK_0_13_TIME );
  _hardfork_versions.versions[ HIVE_HARDFORK_0_13 ] = HIVE_HARDFORK_0_13_VERSION;
  FC_ASSERT( HIVE_HARDFORK_0_14 == 14, "Invalid hardfork configuration" );
  _hardfork_versions.times[ HIVE_HARDFORK_0_14 ] = fc::time_point_sec( HIVE_HARDFORK_0_14_TIME );
  _hardfork_versions.versions[ HIVE_HARDFORK_0_14 ] = HIVE_HARDFORK_0_14_VERSION;
  FC_ASSERT( HIVE_HARDFORK_0_15 == 15, "Invalid hardfork configuration" );
  _hardfork_versions.times[ HIVE_HARDFORK_0_15 ] = fc::time_point_sec( HIVE_HARDFORK_0_15_TIME );
  _hardfork_versions.versions[ HIVE_HARDFORK_0_15 ] = HIVE_HARDFORK_0_15_VERSION;
  FC_ASSERT( HIVE_HARDFORK_0_16 == 16, "Invalid hardfork configuration" );
  _hardfork_versions.times[ HIVE_HARDFORK_0_16 ] = fc::time_point_sec( HIVE_HARDFORK_0_16_TIME );
  _hardfork_versions.versions[ HIVE_HARDFORK_0_16 ] = HIVE_HARDFORK_0_16_VERSION;
  FC_ASSERT( HIVE_HARDFORK_0_17 == 17, "Invalid hardfork configuration" );
  _hardfork_versions.times[ HIVE_HARDFORK_0_17 ] = fc::time_point_sec( HIVE_HARDFORK_0_17_TIME );
  _hardfork_versions.versions[ HIVE_HARDFORK_0_17 ] = HIVE_HARDFORK_0_17_VERSION;
  FC_ASSERT( HIVE_HARDFORK_0_18 == 18, "Invalid hardfork configuration" );
  _hardfork_versions.times[ HIVE_HARDFORK_0_18 ] = fc::time_point_sec( HIVE_HARDFORK_0_18_TIME );
  _hardfork_versions.versions[ HIVE_HARDFORK_0_18 ] = HIVE_HARDFORK_0_18_VERSION;
  FC_ASSERT( HIVE_HARDFORK_0_19 == 19, "Invalid hardfork configuration" );
  _hardfork_versions.times[ HIVE_HARDFORK_0_19 ] = fc::time_point_sec( HIVE_HARDFORK_0_19_TIME );
  _hardfork_versions.versions[ HIVE_HARDFORK_0_19 ] = HIVE_HARDFORK_0_19_VERSION;
  FC_ASSERT( HIVE_HARDFORK_0_20 == 20, "Invalid hardfork configuration" );
  _hardfork_versions.times[ HIVE_HARDFORK_0_20 ] = fc::time_point_sec( HIVE_HARDFORK_0_20_TIME );
  _hardfork_versions.versions[ HIVE_HARDFORK_0_20 ] = HIVE_HARDFORK_0_20_VERSION;
  FC_ASSERT( HIVE_HARDFORK_0_21 == 21, "Invalid hardfork configuration" );
  _hardfork_versions.times[ HIVE_HARDFORK_0_21 ] = fc::time_point_sec( HIVE_HARDFORK_0_21_TIME );
  _hardfork_versions.versions[ HIVE_HARDFORK_0_21 ] = HIVE_HARDFORK_0_21_VERSION;
  FC_ASSERT( HIVE_HARDFORK_0_22 == 22, "Invalid hardfork configuration" );
  _hardfork_versions.times[ HIVE_HARDFORK_0_22 ] = fc::time_point_sec( HIVE_HARDFORK_0_22_TIME );
  _hardfork_versions.versions[ HIVE_HARDFORK_0_22 ] = HIVE_HARDFORK_0_22_VERSION;
  FC_ASSERT( HIVE_HARDFORK_0_23 == 23, "Invalid hardfork configuration" );
  _hardfork_versions.times[ HIVE_HARDFORK_0_23 ] = fc::time_point_sec( HIVE_HARDFORK_0_23_TIME );
  _hardfork_versions.versions[ HIVE_HARDFORK_0_23 ] = HIVE_HARDFORK_0_23_VERSION;
  FC_ASSERT( HIVE_HARDFORK_1_24 == 24, "Invalid hardfork configuration" );
  _hardfork_versions.times[ HIVE_HARDFORK_1_24 ] = fc::time_point_sec( HIVE_HARDFORK_1_24_TIME );
  _hardfork_versions.versions[ HIVE_HARDFORK_1_24 ] = HIVE_HARDFORK_1_24_VERSION;
  FC_ASSERT( HIVE_HARDFORK_1_25 == 25, "Invalid hardfork configuration" );
  _hardfork_versions.times[ HIVE_HARDFORK_1_25 ] = fc::time_point_sec( HIVE_HARDFORK_1_25_TIME );
  _hardfork_versions.versions[ HIVE_HARDFORK_1_25 ] = HIVE_HARDFORK_1_25_VERSION;
  FC_ASSERT( HIVE_HARDFORK_1_26 == 26, "Invalid hardfork configuration" );
  _hardfork_versions.times[ HIVE_HARDFORK_1_26 ] = fc::time_point_sec( HIVE_HARDFORK_1_26_TIME );
  _hardfork_versions.versions[ HIVE_HARDFORK_1_26 ] = HIVE_HARDFORK_1_26_VERSION;
  FC_ASSERT( HIVE_HARDFORK_1_27 == 27, "Invalid hardfork configuration" );
  _hardfork_versions.times[ HIVE_HARDFORK_1_27 ] = fc::time_point_sec( HIVE_HARDFORK_1_27_TIME );
  _hardfork_versions.versions[ HIVE_HARDFORK_1_27 ] = HIVE_HARDFORK_1_27_VERSION;
  FC_ASSERT( HIVE_HARDFORK_1_28 == 28, "Invalid hardfork configuration" );
  _hardfork_versions.times[ HIVE_HARDFORK_1_28 ] = fc::time_point_sec( HIVE_HARDFORK_1_28_TIME );
  _hardfork_versions.versions[ HIVE_HARDFORK_1_28 ] = HIVE_HARDFORK_1_28_VERSION;
#if defined(USE_ALTERNATE_CHAIN_ID)
  /// put here another HF other than SMT
#if defined(IS_TEST_NET) && defined(HIVE_ENABLE_SMT)
  FC_ASSERT( HIVE_HARDFORK_1_29 == 29, "Invalid hardfork configuration" );
  _hardfork_versions.times[ HIVE_HARDFORK_1_29 ] = fc::time_point_sec( HIVE_HARDFORK_1_29_TIME );
  _hardfork_versions.versions[ HIVE_HARDFORK_1_29 ] = HIVE_HARDFORK_1_29_VERSION;
#endif
#endif
}

void database::process_hardforks()
{
  try
  {
    // If there are upcoming hardforks and the next one is later, do nothing
    const auto& hardforks = get_hardfork_property_object();

    if( has_hardfork( HIVE_HARDFORK_0_5__54 ) )
    {
      while( _hardfork_versions.versions[ hardforks.last_hardfork ] < hardforks.next_hardfork
        && hardforks.next_hardfork_time <= head_block_time() )
      {
        if( hardforks.last_hardfork < HIVE_NUM_HARDFORKS ) {
          apply_hardfork( hardforks.last_hardfork + 1 );
        }
        else
          throw unknown_hardfork_exception();
      }
    }
    else
    {
      while( hardforks.last_hardfork < HIVE_NUM_HARDFORKS
          && _hardfork_versions.times[ hardforks.last_hardfork + 1 ] <= head_block_time()
          && hardforks.last_hardfork < HIVE_HARDFORK_0_5__54 )
      {
        apply_hardfork( hardforks.last_hardfork + 1 );
      }
    }
  }
  FC_CAPTURE_AND_RETHROW()
}

bool database::has_hardfork( uint32_t hardfork )const
{
  return get_hardfork_property_object().processed_hardforks.size() > hardfork;
}

uint32_t database::get_hardfork()const
{
  return get_hardfork_property_object().processed_hardforks.size() - 1;
}

void database::set_hardfork( uint32_t hardfork, bool apply_now )
{
  auto const& hardforks = get_hardfork_property_object();

  for( uint32_t i = hardforks.last_hardfork + 1; i <= hardfork && i <= HIVE_NUM_HARDFORKS; i++ )
  {
    if( i <= HIVE_HARDFORK_0_5__54 )
      _hardfork_versions.times[i] = head_block_time();
    else
    {
      modify( hardforks, [&]( hardfork_property_object& hpo )
      {
        hpo.next_hardfork = _hardfork_versions.versions[i];
        hpo.next_hardfork_time = head_block_time();
      } );
    }

    if( apply_now )
      apply_hardfork( i );
  }
}

void database::apply_hardfork( uint32_t hardfork )
{
  if( _log_hardforks )
    elog( "HARDFORK ${hf} at block ${b}", ("hf", hardfork)("b", head_block_num()) );
  operation hardfork_vop = hardfork_operation( hardfork );

  pre_push_virtual_operation( hardfork_vop );
  const auto _op_in_trx = _current_op_in_trx;

  switch( hardfork )
  {
    case HIVE_HARDFORK_0_1:
      perform_vesting_share_split( 1000000 );
      break;
    case HIVE_HARDFORK_0_2:
      retally_witness_votes();
      break;
    case HIVE_HARDFORK_0_3:
      retally_witness_votes();
      break;
    case HIVE_HARDFORK_0_4:
      reset_virtual_schedule_time(*this);
      break;
    case HIVE_HARDFORK_0_5:
      break;
    case HIVE_HARDFORK_0_6:
      retally_witness_vote_counts();
      break;
    case HIVE_HARDFORK_0_7:
      break;
    case HIVE_HARDFORK_0_8:
      retally_witness_vote_counts(true);
      break;
    case HIVE_HARDFORK_0_9:
      {
        for( const std::string& acc : hardfork9::get_compromised_accounts() )
        {
          const account_object* account = find_account( acc );
          if( account == nullptr )
            continue;

          wlog("Setting key: ${k} as an owner authority for account: ${a}", ("k", HIVE_HF_9_COMPROMISED_ACCOUNTS_PUBLIC_KEY_STR)("a", acc));

          update_owner_authority( *account, authority( 1, public_key_type(HIVE_HF_9_COMPROMISED_ACCOUNTS_PUBLIC_KEY_STR), 1 ) );

          modify( get< account_authority_object, by_account >( account->get_name() ), [&]( account_authority_object& auth )
          {
            auth.active  = authority( 1, public_key_type(HIVE_HF_9_COMPROMISED_ACCOUNTS_PUBLIC_KEY_STR), 1 );
            auth.posting = authority( 1, public_key_type(HIVE_HF_9_COMPROMISED_ACCOUNTS_PUBLIC_KEY_STR), 1 );
          });
        }
      }
      break;
    case HIVE_HARDFORK_0_10:
      retally_liquidity_weight();
      break;
    case HIVE_HARDFORK_0_11:
      break;
    case HIVE_HARDFORK_0_12:
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

        // liquidity reward mechanism no longer active after HF12
        auto& liquidity_reward_balance_idx = get_mutable_index< liquidity_reward_balance_index >();
        liquidity_reward_balance_idx.clear();
      }
      break;
    case HIVE_HARDFORK_0_13:
      break;
    case HIVE_HARDFORK_0_14:
      break;
    case HIVE_HARDFORK_0_15:
      break;
    case HIVE_HARDFORK_0_16:
      {
        modify( get_feed_history(), [&]( feed_history_object& fho )
        {
          while( fho.price_history.size() > HIVE_FEED_HISTORY_WINDOW )
            fho.price_history.pop_front();
        });
      }
      break;
    case HIVE_HARDFORK_0_17:
      {
        static_assert(
          HIVE_MAX_VOTED_WITNESSES_HF0 + HIVE_MAX_MINER_WITNESSES_HF0 + HIVE_MAX_RUNNER_WITNESSES_HF0 == HIVE_MAX_WITNESSES,
          "HF0 witness counts must add up to HIVE_MAX_WITNESSES" );
        static_assert(
          HIVE_MAX_VOTED_WITNESSES_HF17 + HIVE_MAX_MINER_WITNESSES_HF17 + HIVE_MAX_RUNNER_WITNESSES_HF17 == HIVE_MAX_WITNESSES,
          "HF17 witness counts must add up to HIVE_MAX_WITNESSES" );

        modify( get_witness_schedule_object(), [&]( witness_schedule_object& wso )
        {
          wso.max_voted_witnesses = HIVE_MAX_VOTED_WITNESSES_HF17;
          wso.max_miner_witnesses = HIVE_MAX_MINER_WITNESSES_HF17;
          wso.max_runner_witnesses = HIVE_MAX_RUNNER_WITNESSES_HF17;
        });

        const auto& gpo = get_dynamic_global_properties();

        auto& post_rf = create< reward_fund_object >( HIVE_POST_REWARD_FUND_NAME, gpo.get_total_reward_fund_hive(), head_block_time()
#ifndef IS_TEST_NET
          , HIVE_HF_17_RECENT_CLAIMS
#endif
          );

        // As a shortcut in payout processing, we use the id as an array index.
        // The IDs must be assigned this way. The assertion is a dummy check to ensure this happens.
        FC_ASSERT( post_rf.get_id() == reward_fund_id_type() );

        modify( gpo, [&]( dynamic_global_property_object& g )
        {
          g.total_reward_fund_hive = asset( 0, HIVE_SYMBOL );
          g.total_reward_shares2 = 0;
        });

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
      break;
    case HIVE_HARDFORK_0_18:
      break;
    case HIVE_HARDFORK_0_19:
      {
        modify( get_dynamic_global_properties(), [&]( dynamic_global_property_object& gpo )
        {
          gpo.vote_power_reserve_rate = HIVE_REDUCED_VOTE_POWER_RATE;
        });

        modify( get< reward_fund_object, by_name >( HIVE_POST_REWARD_FUND_NAME ), [&]( reward_fund_object &rfo )
        {
#ifndef IS_TEST_NET
          rfo.recent_claims = HIVE_HF_19_RECENT_CLAIMS;
#endif
          rfo.author_reward_curve = curve_id::linear;
          rfo.curation_reward_curve = curve_id::square_root;
        });

        /* Remove all 0 delegation objects */
        vector< const vesting_delegation_object* > to_remove;
        const auto& delegation_idx = get_index< vesting_delegation_index, by_id >();
        auto delegation_itr = delegation_idx.begin();

        while( delegation_itr != delegation_idx.end() )
        {
          if( delegation_itr->get_vesting().amount == 0 )
            to_remove.push_back( &(*delegation_itr) );

          ++delegation_itr;
        }

        for( const vesting_delegation_object* delegation_ptr: to_remove )
        {
          remove( *delegation_ptr );
        }

        remove_old_cashouts();
      }
      break;
    case HIVE_HARDFORK_0_20:
      {
        const auto& dgpo = get_dynamic_global_properties();
        modify( dgpo, [&]( dynamic_global_property_object& gpo )
        {
          gpo.delegation_return_period = HIVE_DELEGATION_RETURN_PERIOD_HF20;
          gpo.reverse_auction_seconds = HIVE_REVERSE_AUCTION_WINDOW_SECONDS_HF20;
          gpo.hbd_stop_percent = HIVE_HBD_STOP_PERCENT_HF20;
          gpo.hbd_start_percent = HIVE_HBD_START_PERCENT_HF20;
          gpo.available_account_subsidies = 0;
        } );

        const auto& wso = get_witness_schedule_object();
        for( const auto& witness : wso.current_shuffled_witnesses )
        {
          // Required check when applying hardfork at genesis
          if( witness != account_name_type() )
          {
            modify( get< witness_object, by_name >( witness ), [&]( witness_object& w )
            {
              w.props.account_creation_fee = asset( w.props.account_creation_fee.amount * HIVE_CREATE_ACCOUNT_WITH_HIVE_MODIFIER, HIVE_SYMBOL );
            } );
          }
        }

        modify( wso, [&]( witness_schedule_object& wso )
        {
          wso.median_props.account_creation_fee = asset( wso.median_props.account_creation_fee.amount * HIVE_CREATE_ACCOUNT_WITH_HIVE_MODIFIER, HIVE_SYMBOL );
        } );

        // Initialize RC:

        // Initial values are located at `libraries/jsonball/data/resource_parameters.json`
        std::string resource_params_json = hive::jsonball::get_resource_parameters();
        fc::variant resource_params_var = fc::json::from_string( resource_params_json, fc::json::format_validation_mode::full, fc::json::strict_parser );
        std::vector< std::pair< fc::variant, std::pair< fc::variant_object, fc::variant_object > > > resource_params_pairs;
        fc::from_variant( resource_params_var, resource_params_pairs );
        fc::time_point_sec now = dgpo.time;

        const auto& rc_params = create< rc_resource_param_object >( [&]( rc_resource_param_object& params_obj )
        {
          for( auto& kv : resource_params_pairs )
          {
            auto k = kv.first.as< rc_resource_types >();
            fc::variant_object& vo = kv.second.first;
            fc::mutable_variant_object mvo( vo );
            fc::from_variant( fc::variant( mvo ), params_obj.resource_param_array[k] );
          }
        } );
        // override value for new account tokens using parameters provided by witnesses
        rc().set_pool_params( wso );
        dlog( "Initial RC params: ${o}", ( "o", rc_params ) );

        // create usage statistics buckets (empty, but with proper timestamps, last bucket has current timestamp)
        time_point_sec timestamp = now - fc::seconds( HIVE_RC_BUCKET_TIME_LENGTH * ( HIVE_RC_WINDOW_BUCKET_COUNT - 1 ) );
        for( int i = 0; i < HIVE_RC_WINDOW_BUCKET_COUNT; ++i )
        {
          create< rc_usage_bucket_object >( timestamp );
          timestamp += fc::seconds( HIVE_RC_BUCKET_TIME_LENGTH );
        }

        const auto& rc_pool = create< rc_pool_object >( rc_params, resource_count_type() );
        ilog( "Initial RC pools: ${o}", ( "o", rc_pool.get_pool() ) );

        create< rc_stats_object >( RC_PENDING_STATS_ID.get_value() );
        create< rc_stats_object >( RC_ARCHIVE_STATS_ID.get_value() );

        const auto& idx = get_index< account_index, by_id >();
        for( auto it = idx.begin(); it != idx.end(); ++it )
        {
          modify( *it, [&]( account_object& account )
          {
            account.rc_adjustment = HIVE_RC_HISTORICAL_ACCOUNT_CREATION_ADJUSTMENT;
            account.rc_manabar.last_update_time = now.sec_since_epoch();
            auto max_rc = account.get_maximum_rc().value;
            account.rc_manabar.current_mana = max_rc;
            account.last_max_rc = max_rc;
          } );
        }

      }
      break;
    case HIVE_HARDFORK_0_21:
    {
      modify( get_dynamic_global_properties(), [&]( dynamic_global_property_object& gpo )
      {
        gpo.proposal_fund_percent = HIVE_PROPOSAL_FUND_PERCENT_HF21;
        gpo.content_reward_percent = HIVE_CONTENT_REWARD_PERCENT_HF21;
        gpo.downvote_pool_percent = HIVE_DOWNVOTE_POOL_PERCENT_HF21;
        gpo.reverse_auction_seconds = HIVE_REVERSE_AUCTION_WINDOW_SECONDS_HF21;
      });

      const auto treasury_name = get_treasury_name();

      // Create the treasury account if it does not exist
      // This may sometimes happen in the mirrornet, when we do not have the account created upon the HF 21 application or any dependent operation
      if( find_account(treasury_name) == nullptr ) {
          create<account_object>(treasury_name, head_block_time());
          push_virtual_operation(
            account_created_operation( treasury_name, treasury_name, asset(0, VESTS_SYMBOL), asset(0, VESTS_SYMBOL) ) );
      }

      lock_account( get_treasury() );

      modify( get< reward_fund_object, by_name >( HIVE_POST_REWARD_FUND_NAME ), [&]( reward_fund_object& rfo )
      {
        rfo.percent_curation_rewards = 50 * HIVE_1_PERCENT;
        rfo.author_reward_curve = convergent_linear;
        rfo.curation_reward_curve = convergent_square_root;
        rfo.content_constant = HIVE_CONTENT_CONSTANT_HF21;
#ifndef  IS_TEST_NET
        rfo.recent_claims = HIVE_HF21_CONVERGENT_LINEAR_RECENT_CLAIMS;
#endif
      });
    }
    break;
    case HIVE_HARDFORK_0_22:
      break;
    case HIVE_HARDFORK_0_23:
    {
      clear_accounts( hardforkprotect::get_steemit_accounts() );

      /** Reset TAPOS buffer to avoid replay attack - do it only for mainnet, since is far away from such blocks.
          Skip it now since it is ineffective, but i.e. breaks live 5M mirrornet instance having applied all HFs.
      */
      //auto empty_block_id = block_id_type();
      //const auto& bs_idx = get_index< block_summary_index, by_id >();
      //for( auto itr = bs_idx.begin(); itr != bs_idx.end(); ++itr )
      //{
      //  modify( *itr, [&](block_summary_object& p) {
      //    p.block_id = empty_block_id;
      //  });
      //}
      break;
    }
    case HIVE_HARDFORK_1_24:
    {
      restore_accounts( hardforkprotect::get_restored_accounts() );
#ifdef USE_ALTERNATE_CHAIN_ID
      /// Don't change chain_id in testnet build.
#else
      set_chain_id(HIVE_CHAIN_ID);
#endif /// IS_TEST_NET
      break;
    }
    case HIVE_HARDFORK_1_25:
    {
      modify( get< reward_fund_object, by_name >( HIVE_POST_REWARD_FUND_NAME ), [&]( reward_fund_object& rfo )
      {
        rfo.curation_reward_curve = linear;
        rfo.author_reward_curve   = linear;
      });
      modify( get_dynamic_global_properties(), [&]( dynamic_global_property_object& gpo )
      {
        gpo.reverse_auction_seconds = HIVE_REVERSE_AUCTION_WINDOW_SECONDS_HF25;
        gpo.early_voting_seconds    = HIVE_EARLY_VOTING_SECONDS_HF25;
        gpo.mid_voting_seconds      = HIVE_MID_VOTING_SECONDS_HF25;
      });
      break;
    }
    case HIVE_HARDFORK_1_26:
    {
      modify( get_dynamic_global_properties(), [&]( dynamic_global_property_object& gpo )
      {
        gpo.hbd_stop_percent = HIVE_HBD_STOP_PERCENT_HF26;
        gpo.hbd_start_percent = HIVE_HBD_START_PERCENT_HF26;
      } );
      const auto& fwso = create<witness_schedule_object>( [&]( witness_schedule_object& future_witness_schedule )
      {
        future_witness_schedule.copy_values_from( get_witness_schedule_object() );
      } );
      FC_ASSERT( fwso.get_id() == 1, "Unexpected id allocated to future witness schedule object" );
      break;
    }
    case HIVE_HARDFORK_1_28:
    {
      remove_proposal_votes_for_accounts_without_voting_rights();
      break;
    }
    case HIVE_SMT_HARDFORK:
    {
#ifdef HIVE_ENABLE_SMT
      replenish_nai_pool( *this );
#endif
      break;
    }
    default:
      break;
  }

  modify( get_hardfork_property_object(), [&]( hardfork_property_object& hfp )
  {
    FC_ASSERT( hardfork == hfp.last_hardfork + 1, "Hardfork being applied out of order", ("hardfork",hardfork)("hfp.last_hardfork",hfp.last_hardfork) );
    FC_ASSERT( hfp.processed_hardforks.size() == hardfork, "Hardfork being applied out of order" );
    hfp.processed_hardforks.push_back( _hardfork_versions.times[ hardfork ] );
    hfp.last_hardfork = hardfork;
    hfp.current_hardfork_version = _hardfork_versions.versions[ hardfork ];
    FC_ASSERT( hfp.processed_hardforks[ hfp.last_hardfork ] == _hardfork_versions.times[ hfp.last_hardfork ], "Hardfork processing failed sanity check..." );
  } );

  if( hardfork == HIVE_HARDFORK_1_24_TREASURY_RENAME )
  {
    const auto treasury_name = get_treasury_name();

    if( find_account(treasury_name) == nullptr ) {
        create<account_object>(treasury_name, head_block_time());
        push_virtual_operation(
          account_created_operation( treasury_name, treasury_name, asset(0, VESTS_SYMBOL), asset(0, VESTS_SYMBOL) ) );
    }

    lock_account( get_treasury() );
    //the following routine can only be called effectively after hardfork was marked as applied
    //we could wait for regular call in _apply_block(), however it could hinder future changes, most notably use of treasury in future
    //hardfork code with assumption of nonzero balance
    consolidate_treasury_balance();
  }
  // HF 24 updates blockchain configuration.
  if (hardfork == HIVE_HARDFORK_1_24)
  {
    const auto current_blockchain_config = protocol::get_config(get_treasury_name(), get_chain_id());
    fc::variant current_blockchain_config_as_variant;
    fc::to_variant(current_blockchain_config, current_blockchain_config_as_variant);
    set_blockchain_config(fc::json::to_string(current_blockchain_config_as_variant));
  }

  post_push_virtual_operation( hardfork_vop, _op_in_trx );
}

const hardfork_property_object& database::get_hardfork_property_object()const
{ try {
  return get< hardfork_property_object >();
} FC_CAPTURE_AND_RETHROW() }

void database::gather_balance( const std::string& name, const asset& balance, const asset& hbd_balance )
{
  modify( get_hardfork_property_object(), [&]( hardfork_property_object& hfp )
  {
    hfp.h23_balances.emplace( std::make_pair( name, hf23_item{ balance, hbd_balance } ) );
  } );
}

} } // hive::chain
