#ifdef IS_TEST_NET

#include <boost/test/unit_test.hpp>

#include <hive/chain/hive_fwd.hpp>

#include <hive/protocol/exceptions.hpp>
#include <hive/protocol/hardfork.hpp>
#include <hive/protocol/dhf_operations.hpp>

#include <hive/chain/database.hpp>
#include <hive/chain/database_exceptions.hpp>
#include <hive/chain/detail/state/convert_request_object.hpp>
#include <hive/chain/detail/state/collateralized_convert_request_object.hpp>
#include <hive/chain/detail/state/escrow_object.hpp>
#include <hive/chain/detail/state/savings_withdraw_object.hpp>
#include <hive/chain/detail/state/liquidity_reward_balance_object.hpp>
#include <hive/chain/detail/state/feed_history_object.hpp>
#include <hive/chain/detail/state/limit_order_object.hpp>
#include <hive/chain/detail/state/withdraw_vesting_route_object.hpp>
#include <hive/chain/detail/state/decline_voting_rights_request_object.hpp>
#include <hive/chain/detail/state/reward_fund_object.hpp>
#include <hive/chain/detail/state/recurrent_transfer_object.hpp>

#include <hive/chain/util/reward.hpp>
#include <hive/chain/comment_object.hpp>
#include <hive/chain/witness_objects.hpp>
#include <hive/chain/detail/state/hardfork_property_object.hpp>
#include <hive/chain/detail/state/global_property_object.hpp>
// Multiindex headers for index type definitions
#include <hive/chain/comment_object_multiindex.hpp>
#include <hive/chain/transaction_object_multiindex.hpp>
#include <hive/chain/dhf_objects_multiindex.hpp>
#include <hive/chain/block_summary_object_multiindex.hpp>
#include <hive/chain/hardfork_property_object_multiindex.hpp>
#include <hive/chain/witness_objects_multiindex.hpp>
#include <hive/chain/detail/state/global_property_object_multiindex.hpp>
#include <hive/chain/detail/state/feed_history_object_multiindex.hpp>
#include <hive/chain/detail/state/limit_order_object_multiindex.hpp>
#include <hive/chain/detail/state/decline_voting_rights_request_object_multiindex.hpp>
#include <hive/chain/detail/state/reward_fund_object_multiindex.hpp>
#include <hive/chain/detail/state/recurrent_transfer_object_multiindex.hpp>
#include <hive/chain/detail/state/convert_request_object_multiindex.hpp>
#include <hive/chain/detail/state/collateralized_convert_request_object_multiindex.hpp>
#include <hive/chain/detail/state/escrow_object_multiindex.hpp>
#include <hive/chain/detail/state/savings_withdraw_object_multiindex.hpp>
#include <hive/chain/detail/state/liquidity_reward_balance_object_multiindex.hpp>
#include <hive/chain/detail/state/withdraw_vesting_route_object_multiindex.hpp>

#include <fc/macros.hpp>
#include <fc/crypto/digest.hpp>

#include "../db_fixture/clean_database_fixture.hpp"

#include <cmath>
#include <iostream>
#include <stdexcept>

using namespace hive;
using namespace hive::chain;
using namespace hive::protocol;
using fc::string;


BOOST_FIXTURE_TEST_SUITE( hf24_tests, hf24_database_fixture )

BOOST_AUTO_TEST_CASE( blocked_operations )
{
  try
  {
    BOOST_TEST_MESSAGE( "Even after HF24 steem.dao is considered a treasury account" );

    ACTORS( ( alice ) )
    generate_block();
    issue_funds( "alice", HIVE_asset( 10'000 ) );
    issue_funds( "alice", HBD_asset( 10'000 ) );
    generate_block();

    signed_transaction tx;
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );

    //check vesting of non-HBD
    {
      transfer_to_vesting_operation op;
      op.from = "alice";
      op.to = OBSOLETE_TREASURY_ACCOUNT;
      op.amount = ASSET( "1.000 TESTS" );
      tx.operations.push_back( op );
      BOOST_REQUIRE_THROW( push_transaction( tx, alice_private_key ), fc::assert_exception );

      tx.clear();
      op.to = NEW_HIVE_TREASURY_ACCOUNT;
      tx.operations.push_back( op );
      BOOST_REQUIRE_THROW( push_transaction( tx, alice_private_key ), fc::assert_exception );
    }
    tx.clear();

    //check withdraw vesting route
    {
      set_withdraw_vesting_route_operation op;
      op.from_account = "alice";
      op.to_account = OBSOLETE_TREASURY_ACCOUNT;
      op.percent = 50 * HIVE_1_PERCENT;
      tx.operations.push_back( op );
      BOOST_REQUIRE_THROW( push_transaction( tx, alice_private_key ), fc::assert_exception );

      tx.clear();
      op.to_account = NEW_HIVE_TREASURY_ACCOUNT;
      tx.operations.push_back( op );
      BOOST_REQUIRE_THROW( push_transaction( tx, alice_private_key ), fc::assert_exception );
    }
    tx.clear();

    //check transfer to savings
    {
      transfer_to_savings_operation op;
      op.from = "alice";
      op.to = OBSOLETE_TREASURY_ACCOUNT;
      op.amount = ASSET( "1.000 TESTS" );
      tx.operations.push_back( op );
      BOOST_REQUIRE_THROW( push_transaction( tx, alice_private_key ), fc::assert_exception );

      tx.clear();
      op.amount = ASSET( "1.000 TBD" );
      tx.operations.push_back( op );
      BOOST_REQUIRE_THROW( push_transaction( tx, alice_private_key ), fc::assert_exception );

      tx.clear();
      op.to = NEW_HIVE_TREASURY_ACCOUNT;
      tx.operations.push_back( op );
      BOOST_REQUIRE_THROW( push_transaction( tx, alice_private_key ), fc::assert_exception );

      tx.clear();
      op.amount = ASSET( "1.000 TESTS" );
      tx.operations.push_back( op );
      BOOST_REQUIRE_THROW( push_transaction( tx, alice_private_key ), fc::assert_exception );

      tx.clear();
      op.to = "alice";
      tx.operations.push_back( op );
      push_transaction( tx, alice_private_key );
      BOOST_REQUIRE_EQUAL( get_hive_savings( "alice" ), HIVE_asset( 1'000 ) );
    }
    tx.clear();

    //check transfer from savings of non-HBD
    {
      transfer_from_savings_operation op;
      op.from = "alice";
      op.to = OBSOLETE_TREASURY_ACCOUNT;
      op.amount = ASSET( "1.000 TESTS" );
      tx.operations.push_back( op );
      BOOST_REQUIRE_THROW( push_transaction( tx, alice_private_key ), fc::assert_exception );

      tx.clear();
      op.to = NEW_HIVE_TREASURY_ACCOUNT;
      tx.operations.push_back( op );
      BOOST_REQUIRE_THROW( push_transaction( tx, alice_private_key ), fc::assert_exception );
    }
    tx.clear();

    database_fixture::validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( comment_beneficiary )
{
  try
  {
    BOOST_TEST_MESSAGE( "After HF24 steem.dao as comment beneficiary gives directly to new treasury account" );

    ACTORS( ( alice ) )
    generate_block();

    db_plugin->debug_update( []( database& db )
    {
      db.modify( db.get_dynamic_global_properties(), []( dynamic_global_property_object& gpo )
      {
        gpo.proposal_fund_percent = 0;
      } );
    } );
    issue_funds( "alice", HIVE_asset( 10'000 ) );
    issue_funds( "alice", HBD_asset( 10'000 ) );
    generate_block();

    signed_transaction tx;
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );

    {
      comment_operation comment;
      comment.author = "alice";
      comment.permlink = "test";
      comment.parent_permlink = "test";
      comment.title = "test";
      comment.body = "Hello world";
      tx.operations.push_back( comment );
      push_transaction( tx, alice_post_key );
    }
    tx.clear();

    {
      comment_options_operation op;
      comment_payout_beneficiaries b;
      b.beneficiaries.push_back( beneficiary_route_type( OBSOLETE_TREASURY_ACCOUNT, HIVE_100_PERCENT ) );
      op.author = "alice";
      op.permlink = "test";
      op.allow_curation_rewards = false;
      op.extensions.insert( b );
      tx.operations.push_back( op );
      push_transaction( tx, alice_post_key );
    }
    tx.clear();

    {
      vote_operation vote;
      vote.author = "alice";
      vote.permlink = "test";
      vote.voter = "alice";
      vote.weight = HIVE_100_PERCENT;
      tx.operations.push_back( vote );
      push_transaction( tx, alice_post_key );
    }
    tx.clear();

    const auto& treasury = db->get_treasury();
    const auto& treasury_assets = db->get_asset_account( treasury.get_id() );
    asset initial_treasury_balance = treasury_assets.get_hbd_balance();
    generate_blocks( db->find_comment_cashout( *db->get_comment( "alice", string( "test" ) ) )->get_cashout_time() );
    BOOST_REQUIRE_EQUAL( get_hbd_balance( OBSOLETE_TREASURY_ACCOUNT ).amount.value, 0 );
    BOOST_REQUIRE_EQUAL( treasury_assets.get_hbd_balance().amount.value, 1150 + initial_treasury_balance.amount.value );

    database_fixture::validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( consolidate_balance )
{
  try
  {
    BOOST_TEST_MESSAGE( "After HF24 even if steem.dao gets some funds they will be transfered to new treasury account" );
    generate_block();
    
    //instead of trying to find a way to fund various balances of steem.dao, just write to them directly
    VEST_asset vested_3, vested_7;
    db_plugin->debug_update( [&]( database& db )
    {
      auto& dgpo = db.get_dynamic_global_properties();
      db.adjust_supply( HIVE_asset( 20'000 ) );
      db.adjust_supply( HBD_asset( 10'000 ) );
      vested_3 = HIVE_asset( 3'000 ) * dgpo.get_vesting_share_price();
      vested_7 = HIVE_asset( 7'000 ) * dgpo.get_vesting_share_price();
      db.modify( dgpo, []( dynamic_global_property_object& gpo )
      {
        gpo.proposal_fund_percent = 0;
      } );
      const auto& old_treasury = db.get_account( OBSOLETE_TREASURY_ACCOUNT );
      db.create_vesting( old_treasury, ASSET( "7.000 TESTS" ) );
      db.create_vesting( old_treasury, ASSET( "3.000 TESTS" ), true );
      const auto& old_treasury_assets_init = db.get_asset_account( old_treasury.get_id() );
      db.modify( old_treasury_assets_init, [&]( assets_object& t )
      {
        t.set_balance( ASSET( "5.000 TESTS" ) );
        t.set_savings( ASSET( "3.000 TESTS" ) );
        t.set_rewards( ASSET( "2.000 TESTS" ) );
        t.set_hbd_balance( ASSET( "5.000 TBD" ) );
        t.set_hbd_savings( ASSET( "3.000 TBD" ) );
        t.set_hbd_rewards( ASSET( "2.000 TBD" ) );
      } );
    } );
    database_fixture::validate_database();
    {
      const auto& old_treasury = db->get_account( OBSOLETE_TREASURY_ACCOUNT );
      const auto& old_treasury_assets = db->get_asset_account( old_treasury.get_id() );
      BOOST_REQUIRE_EQUAL( old_treasury_assets.get_balance().amount.value, 5000 );
      BOOST_REQUIRE_EQUAL( old_treasury_assets.get_savings().amount.value, 3000 );
      BOOST_REQUIRE_EQUAL( old_treasury_assets.get_rewards().amount.value, 2000 );
      BOOST_REQUIRE_EQUAL( old_treasury_assets.get_hbd_balance().amount.value, 5000 );
      BOOST_REQUIRE_EQUAL( old_treasury_assets.get_hbd_savings().amount.value, 3000 );
      BOOST_REQUIRE_EQUAL( old_treasury_assets.get_hbd_rewards().amount.value, 2000 );
      BOOST_REQUIRE_EQUAL( old_treasury_assets.get_vesting().amount.value, vested_7.amount.value );
      BOOST_REQUIRE_EQUAL( old_treasury_assets.get_vest_rewards().amount.value, vested_3.amount.value );
    }

    const auto& treasury_acct = db->get_treasury();
    const auto& treasury_acct_assets = db->get_asset_account( treasury_acct.get_id() );
    asset initial_treasury_balance = treasury_acct_assets.get_hbd_balance();
    generate_block();
    database_fixture::validate_database();

    {
      const auto& old_treasury = db->get_account( OBSOLETE_TREASURY_ACCOUNT );
      const auto& old_treasury_assets = db->get_asset_account( old_treasury.get_id() );
      BOOST_REQUIRE_EQUAL( old_treasury_assets.get_balance().amount.value, 0 );
      BOOST_REQUIRE_EQUAL( old_treasury_assets.get_savings().amount.value, 0 );
      BOOST_REQUIRE_EQUAL( old_treasury_assets.get_rewards().amount.value, 0 );
      BOOST_REQUIRE_EQUAL( old_treasury_assets.get_hbd_balance().amount.value, 0 );
      BOOST_REQUIRE_EQUAL( old_treasury_assets.get_hbd_savings().amount.value, 0 );
      BOOST_REQUIRE_EQUAL( old_treasury_assets.get_hbd_rewards().amount.value, 0 );
      BOOST_REQUIRE_EQUAL( old_treasury_assets.get_vesting().amount.value, 0 );
      BOOST_REQUIRE_EQUAL( old_treasury_assets.get_vest_rewards().amount.value, 0 );
    }
    {
      const auto& treasury = db->get_account( NEW_HIVE_TREASURY_ACCOUNT );
      const auto& treasury_assets = db->get_asset_account( treasury.get_id() );
      BOOST_REQUIRE_EQUAL( treasury_assets.get_balance().amount.value, 14999 ); //rounding during vest->hive conversion
      BOOST_REQUIRE_EQUAL( treasury_assets.get_savings().amount.value, 3000 );
      BOOST_REQUIRE_EQUAL( treasury_assets.get_rewards().amount.value, 2000 );
      BOOST_REQUIRE_EQUAL( treasury_assets.get_hbd_balance().amount.value, 5000 + initial_treasury_balance.amount.value );
      BOOST_REQUIRE_EQUAL( treasury_assets.get_hbd_savings().amount.value, 3000 );
      BOOST_REQUIRE_EQUAL( treasury_assets.get_hbd_rewards().amount.value, 2000 );
      BOOST_REQUIRE_EQUAL( treasury_assets.get_vesting().amount.value, 0 );
      BOOST_REQUIRE_EQUAL( treasury_assets.get_vest_rewards().amount.value, 0 );
    }
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( treasury_debt_ratio )
{
  try
  {
    ACTORS((alice))
    BOOST_TEST_MESSAGE( "After HF24 funds in the treasury don't count towards the HBD debt ratio" );
    set_price_feed( HBD_price( 1000, 10000 ) );
    generate_block();
    auto& dgpo = db->get_dynamic_global_properties();
    const auto before_hbd_print_rate = dgpo.hbd_print_rate;

    ISSUE_FUNDS("alice", HBD_asset( 1'000'000'000 ));
    const auto during_hbd_print_rate = dgpo.hbd_print_rate;

    transfer( "alice", db->get_treasury_name(), asset( 1000000000, HBD_SYMBOL ), "", alice_private_key );
    generate_block();
    const auto after_hbd_print_rate = dgpo.hbd_print_rate;

    BOOST_REQUIRE_EQUAL( after_hbd_print_rate, before_hbd_print_rate );
    BOOST_REQUIRE_NE( after_hbd_print_rate, during_hbd_print_rate );
    database_fixture::validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_SUITE_END()

#endif
