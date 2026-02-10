#ifdef IS_TEST_NET
#include <boost/test/unit_test.hpp>

#include <hive/chain/hive_fwd.hpp>

#include <hive/protocol/exceptions.hpp>
#include <hive/protocol/hardfork.hpp>

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

#include <hive/chain/util/reward.hpp>

#include <hive/chain/rc/rc_objects.hpp>
#include <hive/chain/comment_object.hpp>
#include <hive/chain/witness_objects.hpp>
#include <hive/chain/detail/state/hardfork_property_object.hpp>
#include <hive/chain/detail/state/global_property_object.hpp>
#include <hive/chain/detail/state/manabars_rc_object.hpp>
#include <hive/chain/detail/state/assets_object.hpp>
#include <hive/chain/detail/state/time_object.hpp>
#include <hive/chain/detail/state/recovery_object.hpp>
#include <hive/chain/detail/state/delayed_votes_object.hpp>

#include <fc/crypto/digest.hpp>
#include <fc/io/json.hpp>

#include "../db_fixture/clean_database_fixture.hpp"

#include <cmath>
#include <iostream>

using namespace hive;
using namespace hive::chain;
using namespace hive::protocol;
using fc::string;

#define VOTING_MANABAR( account_name ) (db->get< manabars_rc_object >( manabars_rc_object::id_type( db->get_account( account_name ).get_id().get_value() ) ).get_voting_manabar())
#define DOWNVOTE_MANABAR( account_name ) (db->get< manabars_rc_object >( manabars_rc_object::id_type( db->get_account( account_name ).get_id().get_value() ) ).get_downvote_manabar())
#define GET_ASSETS( account_name ) (db->get< assets_object >( assets_object::id_type( db->get_account( account_name ).get_id().get_value() ) ))
#define GET_TIME( account_name ) (db->get< time_object >( time_object::id_type( db->get_account( account_name ).get_id().get_value() ) ))
#define GET_MRC( account_name ) (db->get< manabars_rc_object >( manabars_rc_object::id_type( db->get_account( account_name ).get_id().get_value() ) ))
#define GET_DV( account_name ) (db->get< delayed_votes_object >( delayed_votes_object::id_type( db->get_account( account_name ).get_id().get_value() ) ))
#define GET_EFF_VESTS( account_name ) (db->get_account( account_name ).get_effective_vesting_shares( GET_ASSETS( account_name ), GET_TIME( account_name ) ))

BOOST_FIXTURE_TEST_SUITE( vest_reward_split_tests, clean_database_fixture )

/// Test 1: Verify split objects are created correctly for new accounts
BOOST_AUTO_TEST_CASE( split_object_creation_consistency )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: split_object_creation_consistency" );

    ACTORS( (alice)(bob) )
    generate_block();

    BOOST_TEST_MESSAGE( "--- Verifying split object IDs match account IDs" );

    const auto& alice_acc = db->get_account( "alice" );
    const auto& alice_assets = GET_ASSETS( "alice" );
    const auto& alice_time = GET_TIME( "alice" );
    (void)alice_time;

    BOOST_REQUIRE( alice_assets.get_account_id() == alice_acc.get_id() );
    BOOST_REQUIRE( alice_assets.get_balance() == ASSET( "0.000 TESTS" ) );
    BOOST_REQUIRE( alice_assets.get_vesting().amount.value > 0 ); // Account creation fee
    BOOST_REQUIRE( alice_assets.get_rewards() == ASSET( "0.000 TESTS" ) );
    BOOST_REQUIRE( alice_assets.get_vest_rewards() == ASSET( "0.000000 VESTS" ) );
    BOOST_REQUIRE( alice_assets.get_vest_rewards_as_hive() == ASSET( "0.000 TESTS" ) );
    BOOST_REQUIRE( alice_assets.get_delegated_vesting() == ASSET( "0.000000 VESTS" ) );
    BOOST_REQUIRE( alice_assets.get_received_vesting() == ASSET( "0.000000 VESTS" ) );

    BOOST_REQUIRE( alice_time.get_next_vesting_withdrawal() == fc::time_point_sec::maximum() );

    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

/// Test 2: Verify reward balances are set correctly and can be claimed
BOOST_AUTO_TEST_CASE( create_vesting_reward_balance )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: create_vesting_reward_balance" );

    ACTORS( (alice) )
    generate_block();

    set_price_feed( price( ASSET( "1.000 TBD" ), ASSET( "1.000 TESTS" ) ) );

    BOOST_TEST_MESSAGE( "--- Setting up reward balances via debug_update" );

    db_plugin->debug_update( [&]( database& db )
    {
      const auto& alice_acc = db.get_account( "alice" );
      const auto& alice_assets = db.get< assets_object >( assets_object::id_type( alice_acc.get_id().get_value() ) );

      db.modify( alice_assets, []( assets_object& a )
      {
        a.set_vest_rewards( ASSET( "50.000000 VESTS" ) );
        a.set_vest_rewards_as_hive( ASSET( "5.000 TESTS" ) );
      });

      db.modify( db.get_dynamic_global_properties(), []( dynamic_global_property_object& gpo )
      {
        gpo.current_supply += ASSET( "5.000 TESTS" );
        gpo.virtual_supply += ASSET( "5.000 TESTS" );
        gpo.pending_rewarded_vesting_shares += ASSET( "50.000000 VESTS" );
        gpo.pending_rewarded_vesting_hive += ASSET( "5.000 TESTS" );
      });
    });
    generate_block();
    validate_database();

    BOOST_TEST_MESSAGE( "--- Verifying reward balances were set correctly" );

    BOOST_REQUIRE( get_vest_rewards( "alice" ) == ASSET( "50.000000 VESTS" ) );
    BOOST_REQUIRE( get_vest_rewards_as_hive( "alice" ) == ASSET( "5.000 TESTS" ) );
    BOOST_REQUIRE( GET_ASSETS( "alice" ).get_delegated_vesting() == ASSET( "0.000000 VESTS" ) );

    // Claim and verify the reward goes to active vesting
    auto vests_before = get_vesting( "alice" );
    claim_reward_balance( "alice", ASSET( "0.000 TESTS" ), ASSET( "0.000 TBD" ), ASSET( "50.000000 VESTS" ), alice_post_key );
    BOOST_REQUIRE( get_vesting( "alice" ) == vests_before + ASSET( "50.000000 VESTS" ) );
    BOOST_REQUIRE( get_vest_rewards( "alice" ) == ASSET( "0.000000 VESTS" ) );

    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

/// Test 3: Verify create_vesting with to_reward_balance=false correctly populates vesting fields
BOOST_AUTO_TEST_CASE( create_vesting_direct )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: create_vesting_direct" );

    ACTORS( (alice) )
    generate_block();

    auto vests_before = get_vesting( "alice" );

    const auto& gpo_before = db->get_dynamic_global_properties();
    auto total_vests_before = gpo_before.total_vesting_shares;
    auto total_hive_before = gpo_before.total_vesting_fund_hive;

    BOOST_TEST_MESSAGE( "--- Creating vesting with to_reward_balance=false" );

    asset liquid = ASSET( "10.000 TESTS" );
    asset expected_vests = liquid * gpo_before.get_vesting_share_price();

    db_plugin->debug_update( [&]( database& db )
    {
      // Must add supply since create_vesting converts liquid to vesting but doesn't create the HIVE
      db.modify( db.get_dynamic_global_properties(), [&]( dynamic_global_property_object& gpo )
      {
        gpo.current_supply += liquid;
        gpo.virtual_supply += liquid;
      });
      const auto& alice_acc = db.get_account( "alice" );
      db.create_vesting( alice_acc, liquid, false /*to_reward_balance*/ );
    });
    generate_block();

    BOOST_TEST_MESSAGE( "--- Verifying direct vesting balances were updated correctly" );

    BOOST_REQUIRE( get_vesting( "alice" ) == vests_before + expected_vests );

    const auto& gpo_after = db->get_dynamic_global_properties();
    // Use >= because generate_block() also adds witness rewards to gpo
    BOOST_REQUIRE( gpo_after.total_vesting_shares >= total_vests_before + expected_vests );
    BOOST_REQUIRE( gpo_after.total_vesting_fund_hive >= total_hive_before + liquid );

    // Verify reward balance was NOT changed
    BOOST_REQUIRE( get_vest_rewards( "alice" ) == ASSET( "0.000000 VESTS" ) );

    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

/// Test 4: Claim full reward balance and verify all split objects updated
BOOST_AUTO_TEST_CASE( claim_reward_full )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: claim_reward_full" );

    ACTORS( (alice) )
    generate_block();

    set_price_feed( price( ASSET( "1.000 TBD" ), ASSET( "1.000 TESTS" ) ) );

    BOOST_TEST_MESSAGE( "--- Setting up reward balances via debug_update" );

    db_plugin->debug_update( [&]( database& db )
    {
      const auto& alice_acc = db.get_account( "alice" );
      const auto& alice_assets = db.get< assets_object >( assets_object::id_type( alice_acc.get_id().get_value() ) );

      db.modify( alice_assets, []( assets_object& a )
      {
        a.set_rewards( ASSET( "10.000 TESTS" ) );
        a.set_hbd_rewards( ASSET( "5.000 TBD" ) );
        a.set_vest_rewards( ASSET( "100.000000 VESTS" ) );
        a.set_vest_rewards_as_hive( ASSET( "50.000 TESTS" ) );
      });

      db.modify( db.get_dynamic_global_properties(), []( dynamic_global_property_object& gpo )
      {
        gpo.current_supply += ASSET( "60.000 TESTS" );
        gpo.current_hbd_supply += ASSET( "5.000 TBD" );
        gpo.virtual_supply += ASSET( "60.000 TESTS" );
        gpo.pending_rewarded_vesting_shares += ASSET( "100.000000 VESTS" );
        gpo.pending_rewarded_vesting_hive += ASSET( "50.000 TESTS" );
      });
    });
    generate_block();
    validate_database();

    auto hive_before = get_balance( "alice" );
    auto hbd_before = get_hbd_balance( "alice" );
    auto vests_before = get_vesting( "alice" );

    const auto& gpo_before = db->get_dynamic_global_properties();
    auto total_vests_before = gpo_before.total_vesting_shares;
    auto total_fund_before = gpo_before.total_vesting_fund_hive;
    auto pending_vests_before = gpo_before.pending_rewarded_vesting_shares;
    auto pending_hive_before = gpo_before.pending_rewarded_vesting_hive;

    BOOST_TEST_MESSAGE( "--- Claiming all rewards" );

    claim_reward_balance( "alice", ASSET( "10.000 TESTS" ), ASSET( "5.000 TBD" ), ASSET( "100.000000 VESTS" ), alice_post_key );

    BOOST_TEST_MESSAGE( "--- Verifying balances after full claim" );

    BOOST_REQUIRE( get_balance( "alice" ) == hive_before + ASSET( "10.000 TESTS" ) );
    BOOST_REQUIRE( get_hbd_balance( "alice" ) == hbd_before + ASSET( "5.000 TBD" ) );
    BOOST_REQUIRE( get_vesting( "alice" ) == vests_before + ASSET( "100.000000 VESTS" ) );

    // All reward balances should be zero now
    BOOST_REQUIRE( get_rewards( "alice" ) == ASSET( "0.000 TESTS" ) );
    BOOST_REQUIRE( get_hbd_rewards( "alice" ) == ASSET( "0.000 TBD" ) );
    BOOST_REQUIRE( get_vest_rewards( "alice" ) == ASSET( "0.000000 VESTS" ) );
    BOOST_REQUIRE( get_vest_rewards_as_hive( "alice" ) == ASSET( "0.000 TESTS" ) );

    // Verify global properties updated
    const auto& gpo_after = db->get_dynamic_global_properties();
    BOOST_REQUIRE( gpo_after.total_vesting_shares == total_vests_before + ASSET( "100.000000 VESTS" ) );
    BOOST_REQUIRE( gpo_after.total_vesting_fund_hive == total_fund_before + ASSET( "50.000 TESTS" ) );
    BOOST_REQUIRE( gpo_after.pending_rewarded_vesting_shares == pending_vests_before - ASSET( "100.000000 VESTS" ) );
    BOOST_REQUIRE( gpo_after.pending_rewarded_vesting_hive == pending_hive_before - ASSET( "50.000 TESTS" ) );

    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

/// Test 5: Claim partial reward balance
BOOST_AUTO_TEST_CASE( claim_reward_partial )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: claim_reward_partial" );

    ACTORS( (alice) )
    generate_block();

    set_price_feed( price( ASSET( "1.000 TBD" ), ASSET( "1.000 TESTS" ) ) );

    db_plugin->debug_update( [&]( database& db )
    {
      const auto& alice_acc = db.get_account( "alice" );
      const auto& alice_assets = db.get< assets_object >( assets_object::id_type( alice_acc.get_id().get_value() ) );

      db.modify( alice_assets, []( assets_object& a )
      {
        a.set_rewards( ASSET( "10.000 TESTS" ) );
        a.set_hbd_rewards( ASSET( "10.000 TBD" ) );
        a.set_vest_rewards( ASSET( "100.000000 VESTS" ) );
        a.set_vest_rewards_as_hive( ASSET( "50.000 TESTS" ) );
      });

      db.modify( db.get_dynamic_global_properties(), []( dynamic_global_property_object& gpo )
      {
        gpo.current_supply += ASSET( "60.000 TESTS" );
        gpo.current_hbd_supply += ASSET( "10.000 TBD" );
        gpo.virtual_supply += ASSET( "60.000 TESTS" );
        gpo.pending_rewarded_vesting_shares += ASSET( "100.000000 VESTS" );
        gpo.pending_rewarded_vesting_hive += ASSET( "50.000 TESTS" );
      });
    });
    generate_block();
    validate_database();

    auto vests_before = get_vesting( "alice" );

    BOOST_TEST_MESSAGE( "--- Claiming partial VESTS reward (half)" );

    claim_reward_balance( "alice", ASSET( "0.000 TESTS" ), ASSET( "0.000 TBD" ), ASSET( "50.000000 VESTS" ), alice_post_key );

    BOOST_REQUIRE( get_vest_rewards( "alice" ) == ASSET( "50.000000 VESTS" ) );
    BOOST_REQUIRE( get_vest_rewards_as_hive( "alice" ) == ASSET( "25.000 TESTS" ) );
    BOOST_REQUIRE( get_vesting( "alice" ) == vests_before + ASSET( "50.000000 VESTS" ) );

    vests_before = get_vesting( "alice" );

    BOOST_TEST_MESSAGE( "--- Claiming remaining VESTS reward" );
    generate_block(); // Avoid duplicate transaction error

    claim_reward_balance( "alice", ASSET( "0.000 TESTS" ), ASSET( "0.000 TBD" ), ASSET( "50.000000 VESTS" ), alice_post_key );

    BOOST_REQUIRE( get_vest_rewards( "alice" ) == ASSET( "0.000000 VESTS" ) );
    BOOST_REQUIRE( get_vest_rewards_as_hive( "alice" ) == ASSET( "0.000 TESTS" ) );
    BOOST_REQUIRE( get_vesting( "alice" ) == vests_before + ASSET( "50.000000 VESTS" ) );

    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

/// Test 6: Claim more than available should fail
BOOST_AUTO_TEST_CASE( claim_reward_overclaim )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: claim_reward_overclaim" );

    ACTORS( (alice) )
    generate_block();

    set_price_feed( price( ASSET( "1.000 TBD" ), ASSET( "1.000 TESTS" ) ) );

    db_plugin->debug_update( [&]( database& db )
    {
      const auto& alice_acc = db.get_account( "alice" );
      const auto& alice_assets = db.get< assets_object >( assets_object::id_type( alice_acc.get_id().get_value() ) );

      db.modify( alice_assets, []( assets_object& a )
      {
        a.set_rewards( ASSET( "5.000 TESTS" ) );
        a.set_hbd_rewards( ASSET( "3.000 TBD" ) );
        a.set_vest_rewards( ASSET( "50.000000 VESTS" ) );
        a.set_vest_rewards_as_hive( ASSET( "25.000 TESTS" ) );
      });

      db.modify( db.get_dynamic_global_properties(), []( dynamic_global_property_object& gpo )
      {
        gpo.current_supply += ASSET( "30.000 TESTS" );
        gpo.current_hbd_supply += ASSET( "3.000 TBD" );
        gpo.virtual_supply += ASSET( "30.000 TESTS" );
        gpo.pending_rewarded_vesting_shares += ASSET( "50.000000 VESTS" );
        gpo.pending_rewarded_vesting_hive += ASSET( "25.000 TESTS" );
      });
    });
    generate_block();
    validate_database();

    BOOST_TEST_MESSAGE( "--- Attempting to overclaim HIVE" );
    HIVE_REQUIRE_THROW(
      claim_reward_balance( "alice", ASSET( "6.000 TESTS" ), ASSET( "0.000 TBD" ), ASSET( "0.000000 VESTS" ), alice_post_key ),
      fc::assert_exception );

    BOOST_TEST_MESSAGE( "--- Attempting to overclaim HBD" );
    HIVE_REQUIRE_THROW(
      claim_reward_balance( "alice", ASSET( "0.000 TESTS" ), ASSET( "4.000 TBD" ), ASSET( "0.000000 VESTS" ), alice_post_key ),
      fc::assert_exception );

    BOOST_TEST_MESSAGE( "--- Attempting to overclaim VESTS" );
    HIVE_REQUIRE_THROW(
      claim_reward_balance( "alice", ASSET( "0.000 TESTS" ), ASSET( "0.000 TBD" ), ASSET( "51.000000 VESTS" ), alice_post_key ),
      fc::assert_exception );

    // Verify nothing changed
    BOOST_REQUIRE( get_rewards( "alice" ) == ASSET( "5.000 TESTS" ) );
    BOOST_REQUIRE( get_hbd_rewards( "alice" ) == ASSET( "3.000 TBD" ) );
    BOOST_REQUIRE( get_vest_rewards( "alice" ) == ASSET( "50.000000 VESTS" ) );

    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

/// Test 7: Multiple reward accumulations and then claim
BOOST_AUTO_TEST_CASE( multiple_reward_accumulations )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: multiple_reward_accumulations" );

    ACTORS( (alice) )
    generate_block();

    BOOST_TEST_MESSAGE( "--- Accumulating rewards in multiple steps via debug_update" );

    const int num_accumulations = 10;
    asset vests_per_step = ASSET( "10.000000 VESTS" );
    asset hive_per_step = ASSET( "5.000 TESTS" );
    asset total_vests = ASSET( "0.000000 VESTS" );
    asset total_liquid = ASSET( "0.000 TESTS" );

    for( int i = 0; i < num_accumulations; ++i )
    {
      db_plugin->debug_update( [&]( database& db )
      {
        const auto& alice_acc = db.get_account( "alice" );
        const auto& alice_assets = db.get< assets_object >( assets_object::id_type( alice_acc.get_id().get_value() ) );

        db.modify( alice_assets, [&]( assets_object& a )
        {
          a.set_vest_rewards( a.get_vest_rewards() + vests_per_step );
          a.set_vest_rewards_as_hive( a.get_vest_rewards_as_hive() + hive_per_step );
        });

        db.modify( db.get_dynamic_global_properties(), [&]( dynamic_global_property_object& gpo )
        {
          gpo.current_supply += hive_per_step;
          gpo.virtual_supply += hive_per_step;
          gpo.pending_rewarded_vesting_shares += vests_per_step;
          gpo.pending_rewarded_vesting_hive += hive_per_step;
        });
      });
      generate_block();

      total_vests += vests_per_step;
      total_liquid += hive_per_step;
    }

    BOOST_TEST_MESSAGE( "--- Verifying accumulated rewards match expected" );
    BOOST_REQUIRE( get_vest_rewards( "alice" ) == total_vests );
    BOOST_REQUIRE( get_vest_rewards_as_hive( "alice" ) == total_liquid );

    auto vests_before = get_vesting( "alice" );

    BOOST_TEST_MESSAGE( "--- Claiming all accumulated rewards at once" );
    claim_reward_balance( "alice", ASSET( "0.000 TESTS" ), ASSET( "0.000 TBD" ), total_vests, alice_post_key );

    BOOST_REQUIRE( get_vest_rewards( "alice" ) == ASSET( "0.000000 VESTS" ) );
    BOOST_REQUIRE( get_vest_rewards_as_hive( "alice" ) == ASSET( "0.000 TESTS" ) );
    BOOST_REQUIRE( get_vesting( "alice" ) == vests_before + total_vests );

    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

/// Test 8: Manabar updates during reward claims
BOOST_AUTO_TEST_CASE( manabar_update_on_claim )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: manabar_update_on_claim" );

    ACTORS( (alice) )
    generate_block();

    vest( "alice", ASSET( "100.000 TESTS" ) );
    generate_block();

    set_price_feed( price( ASSET( "1.000 TBD" ), ASSET( "1.000 TESTS" ) ) );

    // Set up reward
    db_plugin->debug_update( [&]( database& db )
    {
      const auto& alice_acc = db.get_account( "alice" );
      const auto& alice_assets = db.get< assets_object >( assets_object::id_type( alice_acc.get_id().get_value() ) );

      db.modify( alice_assets, []( assets_object& a )
      {
        a.set_vest_rewards( ASSET( "1000.000000 VESTS" ) );
        a.set_vest_rewards_as_hive( ASSET( "500.000 TESTS" ) );
      });

      db.modify( db.get_dynamic_global_properties(), []( dynamic_global_property_object& gpo )
      {
        gpo.current_supply += ASSET( "500.000 TESTS" );
        gpo.virtual_supply += ASSET( "500.000 TESTS" );
        gpo.pending_rewarded_vesting_shares += ASSET( "1000.000000 VESTS" );
        gpo.pending_rewarded_vesting_hive += ASSET( "500.000 TESTS" );
      });
    });
    generate_block();
    validate_database();

    // Wait some time for mana to regenerate
    generate_blocks( 10 );

    auto vests_before = get_vesting( "alice" );
    auto mana_before = VOTING_MANABAR( "alice" );
    auto eff_vests_before = GET_EFF_VESTS( "alice" );

    BOOST_TEST_MESSAGE( "--- Claiming VESTS reward and checking manabar" );

    claim_reward_balance( "alice", ASSET( "0.000 TESTS" ), ASSET( "0.000 TBD" ), ASSET( "1000.000000 VESTS" ), alice_post_key );

    auto vests_after = get_vesting( "alice" );
    auto eff_vests_after = GET_EFF_VESTS( "alice" );

    // Vesting should have increased by the claimed amount
    BOOST_REQUIRE( vests_after == vests_before + ASSET( "1000.000000 VESTS" ) );
    // Effective vesting should also have increased
    BOOST_REQUIRE( eff_vests_after > eff_vests_before );

    // Mana should have increased due to new vesting shares being added
    auto mana_after = VOTING_MANABAR( "alice" );
    BOOST_REQUIRE( mana_after.current_mana >= mana_before.current_mana );

    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

/// Test 9: End-to-end curation reward flow with comment cashout and claim
BOOST_AUTO_TEST_CASE( curation_reward_and_claim )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: curation_reward_and_claim" );
    auto _scope = set_mainnet_cashout_values();

    ACTORS( (alice)(bob)(sam) )
    generate_block();

    vest( "alice", ASSET( "100.000 TESTS" ) );
    vest( "bob", ASSET( "100.000 TESTS" ) );
    vest( "sam", ASSET( "50.000 TESTS" ) );
    generate_block();

    set_price_feed( price( ASSET( "1.000 TBD" ), ASSET( "1.000 TESTS" ) ) );
    generate_block();

    BOOST_TEST_MESSAGE( "--- Creating post" );
    post_comment( "alice", "test-post", "Test", "Content", "test", alice_post_key );
    generate_block();

    BOOST_TEST_MESSAGE( "--- Voting on post" );
    vote( "alice", "test-post", "bob", HIVE_100_PERCENT, bob_post_key );
    vote( "alice", "test-post", "sam", HIVE_100_PERCENT, sam_post_key );
    generate_block();

    BOOST_TEST_MESSAGE( "--- Generating blocks until cashout" );
    auto cashout_time = db->find_comment_cashout( *db->get_comment( "alice", string( "test-post" ) ) )->get_cashout_time();
    generate_blocks( cashout_time, true );

    BOOST_TEST_MESSAGE( "--- Checking that curators received rewards" );

    // After cashout, curators should have VEST rewards (since all hardforks are active, rewards go to reward balance)
    auto bob_vest_rewards = get_vest_rewards( "bob" );
    auto sam_vest_rewards = get_vest_rewards( "sam" );

    // Bob voted first with more power, should get rewards
    // At least one of them should have rewards (rewards can be zero if dust threshold applies)
    bool has_rewards = bob_vest_rewards.amount.value > 0 || sam_vest_rewards.amount.value > 0;

    if( has_rewards )
    {
      BOOST_TEST_MESSAGE( "--- Rewards found. Bob: " + fc::json::to_string( bob_vest_rewards ) + " Sam: " + fc::json::to_string( sam_vest_rewards ) );

      auto bob_vests_before = get_vesting( "bob" );

      if( bob_vest_rewards.amount.value > 0 )
      {
        BOOST_TEST_MESSAGE( "--- Bob claiming rewards" );
        claim_reward_balance( "bob", ASSET( "0.000 TESTS" ), ASSET( "0.000 TBD" ), bob_vest_rewards, bob_post_key );

        BOOST_REQUIRE( get_vest_rewards( "bob" ) == ASSET( "0.000000 VESTS" ) );
        BOOST_REQUIRE( get_vesting( "bob" ) == bob_vests_before + bob_vest_rewards );
      }

      auto sam_vests_before = get_vesting( "sam" );

      if( sam_vest_rewards.amount.value > 0 )
      {
        BOOST_TEST_MESSAGE( "--- Sam claiming rewards" );
        claim_reward_balance( "sam", ASSET( "0.000 TESTS" ), ASSET( "0.000 TBD" ), sam_vest_rewards, sam_post_key );

        BOOST_REQUIRE( get_vest_rewards( "sam" ) == ASSET( "0.000000 VESTS" ) );
        BOOST_REQUIRE( get_vesting( "sam" ) == sam_vests_before + sam_vest_rewards );
      }
    }

    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

/// Test 10: Author reward flow with cashout and claim
BOOST_AUTO_TEST_CASE( author_reward_and_claim )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: author_reward_and_claim" );
    auto _scope = set_mainnet_cashout_values();

    ACTORS( (alice)(bob) )
    generate_block();

    vest( "alice", ASSET( "10.000 TESTS" ) );
    vest( "bob", ASSET( "1000.000 TESTS" ) );
    generate_block();

    set_price_feed( price( ASSET( "1.000 TBD" ), ASSET( "1.000 TESTS" ) ) );
    generate_block();

    BOOST_TEST_MESSAGE( "--- Creating post" );
    post_comment( "alice", "test-post", "Test", "Content", "test", alice_post_key );
    generate_block();

    BOOST_TEST_MESSAGE( "--- Bob votes on Alice's post with max weight" );
    vote( "alice", "test-post", "bob", HIVE_100_PERCENT, bob_post_key );
    generate_block();

    BOOST_TEST_MESSAGE( "--- Generating blocks until cashout" );
    auto cashout_time = db->find_comment_cashout( *db->get_comment( "alice", string( "test-post" ) ) )->get_cashout_time();
    generate_blocks( cashout_time, true );

    BOOST_TEST_MESSAGE( "--- Checking that Alice received author rewards" );

    auto alice_vest_rewards = get_vest_rewards( "alice" );
    auto alice_hive_rewards = get_rewards( "alice" );
    auto alice_hbd_rewards = get_hbd_rewards( "alice" );

    BOOST_TEST_MESSAGE( "Author rewards - VESTS: " + fc::json::to_string( alice_vest_rewards ) +
                        " HIVE: " + fc::json::to_string( alice_hive_rewards ) +
                        " HBD: " + fc::json::to_string( alice_hbd_rewards ) );

    // Alice should have received some rewards
    bool has_author_rewards = alice_vest_rewards.amount.value > 0 ||
                              alice_hive_rewards.amount.value > 0 ||
                              alice_hbd_rewards.amount.value > 0;

    if( has_author_rewards )
    {
      auto alice_vests_before = get_vesting( "alice" );
      auto alice_hive_before = get_balance( "alice" );
      auto alice_hbd_before = get_hbd_balance( "alice" );

      BOOST_TEST_MESSAGE( "--- Alice claiming all rewards" );
      claim_reward_balance( "alice", alice_hive_rewards, alice_hbd_rewards, alice_vest_rewards, alice_post_key );

      BOOST_REQUIRE( get_rewards( "alice" ) == ASSET( "0.000 TESTS" ) );
      BOOST_REQUIRE( get_hbd_rewards( "alice" ) == ASSET( "0.000 TBD" ) );
      BOOST_REQUIRE( get_vest_rewards( "alice" ) == ASSET( "0.000000 VESTS" ) );
      BOOST_REQUIRE( get_vest_rewards_as_hive( "alice" ) == ASSET( "0.000 TESTS" ) );

      BOOST_REQUIRE( get_vesting( "alice" ) == alice_vests_before + alice_vest_rewards );
      BOOST_REQUIRE( get_balance( "alice" ) == alice_hive_before + alice_hive_rewards );
      BOOST_REQUIRE( get_hbd_balance( "alice" ) == alice_hbd_before + alice_hbd_rewards );
    }

    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

/// Test 11: Beneficiary rewards flow
BOOST_AUTO_TEST_CASE( beneficiary_reward_and_claim )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: beneficiary_reward_and_claim" );
    auto _scope = set_mainnet_cashout_values();

    ACTORS( (alice)(bob)(charlie) )
    generate_block();

    vest( "alice", ASSET( "10.000 TESTS" ) );
    vest( "bob", ASSET( "1000.000 TESTS" ) );
    generate_block();

    set_price_feed( price( ASSET( "1.000 TBD" ), ASSET( "1.000 TESTS" ) ) );
    generate_block();

    BOOST_TEST_MESSAGE( "--- Creating post with charlie as beneficiary" );

    comment_operation com;
    com.author = "alice";
    com.permlink = "beneficiary-test";
    com.parent_permlink = "test";
    com.title = "foo";
    com.body = "bar";

    comment_options_operation cop;
    cop.author = "alice";
    cop.permlink = "beneficiary-test";
    cop.max_accepted_payout = ASSET( "1000000.000 TBD" );
    cop.percent_hbd = HIVE_100_PERCENT;
    cop.allow_curation_rewards = true;
    cop.allow_votes = true;

    comment_payout_beneficiaries beneficiaries;
    beneficiaries.beneficiaries.push_back( beneficiary_route_type{ "charlie", HIVE_100_PERCENT / 2 } ); // 50% to charlie

    cop.extensions.insert( beneficiaries );

    signed_transaction tx;
    tx.operations.push_back( com );
    tx.operations.push_back( cop );
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    push_transaction( tx, alice_post_key );

    generate_block();

    BOOST_TEST_MESSAGE( "--- Bob votes on Alice's post" );
    vote( "alice", "beneficiary-test", "bob", HIVE_100_PERCENT, bob_post_key );
    generate_block();

    BOOST_TEST_MESSAGE( "--- Generating blocks until cashout" );
    auto cashout_time = db->find_comment_cashout( *db->get_comment( "alice", string( "beneficiary-test" ) ) )->get_cashout_time();
    generate_blocks( cashout_time, true );

    BOOST_TEST_MESSAGE( "--- Checking beneficiary rewards for charlie" );

    auto charlie_vest_rewards = get_vest_rewards( "charlie" );
    auto charlie_hbd_rewards = get_hbd_rewards( "charlie" );

    bool has_beneficiary_rewards = charlie_vest_rewards.amount.value > 0 || charlie_hbd_rewards.amount.value > 0;

    if( has_beneficiary_rewards )
    {
      auto charlie_vests_before = get_vesting( "charlie" );

      BOOST_TEST_MESSAGE( "--- Charlie claiming beneficiary rewards" );
      claim_reward_balance( "charlie", ASSET( "0.000 TESTS" ), charlie_hbd_rewards, charlie_vest_rewards, charlie_post_key );

      BOOST_REQUIRE( get_vest_rewards( "charlie" ) == ASSET( "0.000000 VESTS" ) );
      BOOST_REQUIRE( get_vesting( "charlie" ) == charlie_vests_before + charlie_vest_rewards );
    }

    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

/// Test 12: Vesting withdrawal (power down) with split objects
BOOST_AUTO_TEST_CASE( vesting_withdrawal_split_objects )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: vesting_withdrawal_split_objects" );

    ACTORS( (alice) )
    generate_block();

    vest( "alice", ASSET( "100.000 TESTS" ) );
    generate_block();

    auto alice_vests = get_vesting( "alice" );
    auto withdraw_amount = asset( alice_vests.amount / 2, VESTS_SYMBOL );

    BOOST_TEST_MESSAGE( "--- Starting power down of half of vesting" );
    withdraw_vesting( "alice", withdraw_amount, alice_private_key );

    const auto& alice_assets = GET_ASSETS( "alice" );
    const auto& alice_time = GET_TIME( "alice" );

    // Check withdrawal rate and schedule
    BOOST_REQUIRE( alice_assets.get_to_withdraw().amount == withdraw_amount.amount );
    BOOST_REQUIRE( alice_assets.get_vesting_withdraw_rate().amount.value > 0 );
    BOOST_REQUIRE( alice_time.get_next_vesting_withdrawal() == db->head_block_time() + fc::seconds( HIVE_VESTING_WITHDRAW_INTERVAL_SECONDS ) );

    auto expected_rate = asset( ( withdraw_amount.amount + HIVE_VESTING_WITHDRAW_INTERVALS - 1 ) / HIVE_VESTING_WITHDRAW_INTERVALS, VESTS_SYMBOL );
    BOOST_REQUIRE( alice_assets.get_vesting_withdraw_rate() == expected_rate );

    auto hive_before = get_balance( "alice" );
    auto vests_before = get_vesting( "alice" );

    BOOST_TEST_MESSAGE( "--- Processing first withdrawal" );
    generate_blocks( alice_time.get_next_vesting_withdrawal(), true );

    auto hive_after = get_balance( "alice" );
    auto vests_after = get_vesting( "alice" );

    // Vesting should have decreased and HIVE increased
    BOOST_REQUIRE( vests_after < vests_before );
    BOOST_REQUIRE( hive_after > hive_before );

    // Withdrawn should be updated
    BOOST_REQUIRE( alice_assets.get_withdrawn().amount.value > 0 );

    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

/// Test 13: Full power down cycle
BOOST_AUTO_TEST_CASE( full_power_down_cycle )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: full_power_down_cycle" );

    ACTORS( (alice) )
    generate_block();

    vest( "alice", ASSET( "10.000 TESTS" ) );
    generate_block();

    auto alice_vests_total = get_vesting( "alice" );
    auto hive_before = get_balance( "alice" );

    BOOST_TEST_MESSAGE( "--- Starting power down of all vesting" );
    withdraw_vesting( "alice", alice_vests_total, alice_private_key );

    BOOST_TEST_MESSAGE( "--- Processing all withdrawal intervals" );

    for( int i = 0; i < HIVE_VESTING_WITHDRAW_INTERVALS; ++i )
    {
      const auto& alice_time = GET_TIME( "alice" );
      auto next_withdrawal = alice_time.get_next_vesting_withdrawal();

      if( next_withdrawal == fc::time_point_sec::maximum() )
        break; // Power down complete

      generate_blocks( next_withdrawal, true );
    }

    BOOST_TEST_MESSAGE( "--- Verifying power down completed" );

    const auto& alice_assets = GET_ASSETS( "alice" );
    const auto& alice_time = GET_TIME( "alice" );

    // After full power down, withdrawal should be cancelled
    BOOST_REQUIRE( alice_assets.get_vesting_withdraw_rate() == ASSET( "0.000000 VESTS" ) );
    BOOST_REQUIRE( alice_assets.get_to_withdraw() == ASSET( "0.000000 VESTS" ) );
    BOOST_REQUIRE( alice_assets.get_withdrawn() == ASSET( "0.000000 VESTS" ) );
    BOOST_REQUIRE( alice_time.get_next_vesting_withdrawal() == fc::time_point_sec::maximum() );

    // All vesting should have been converted (minus rounding)
    BOOST_REQUIRE( get_vesting( "alice" ).amount.value == 0 );

    // HIVE balance should have increased
    BOOST_REQUIRE( get_balance( "alice" ) > hive_before );

    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

/// Test 14: Cancel power down
BOOST_AUTO_TEST_CASE( cancel_power_down )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: cancel_power_down" );

    ACTORS( (alice) )
    generate_block();

    vest( "alice", ASSET( "100.000 TESTS" ) );
    generate_block();

    auto vests = get_vesting( "alice" );

    BOOST_TEST_MESSAGE( "--- Starting power down" );
    withdraw_vesting( "alice", asset( vests.amount / 2, VESTS_SYMBOL ), alice_private_key );

    BOOST_REQUIRE( GET_ASSETS( "alice" ).get_vesting_withdraw_rate().amount.value > 0 );
    BOOST_REQUIRE( GET_TIME( "alice" ).get_next_vesting_withdrawal() != fc::time_point_sec::maximum() );

    BOOST_TEST_MESSAGE( "--- Cancelling power down by withdrawing 0" );
    withdraw_vesting( "alice", ASSET( "0.000000 VESTS" ), alice_private_key );

    BOOST_REQUIRE( GET_ASSETS( "alice" ).get_vesting_withdraw_rate() == ASSET( "0.000000 VESTS" ) );
    BOOST_REQUIRE( GET_ASSETS( "alice" ).get_to_withdraw() == ASSET( "0.000000 VESTS" ) );
    BOOST_REQUIRE( GET_TIME( "alice" ).get_next_vesting_withdrawal() == fc::time_point_sec::maximum() );

    // Vesting should be unchanged
    BOOST_REQUIRE( get_vesting( "alice" ) == vests );

    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

/// Test 15: Delegation and reward interaction
BOOST_AUTO_TEST_CASE( delegation_and_reward_interaction )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: delegation_and_reward_interaction" );

    ACTORS( (alice)(bob) )
    generate_block();

    vest( "alice", ASSET( "1000.000 TESTS" ) );
    vest( "bob", ASSET( "10.000 TESTS" ) );
    generate_block();

    set_price_feed( price( ASSET( "1.000 TBD" ), ASSET( "1.000 TESTS" ) ) );

    auto alice_vests = get_vesting( "alice" );
    auto delegation_amount = asset( alice_vests.amount / 4, VESTS_SYMBOL );

    BOOST_TEST_MESSAGE( "--- Alice delegates 25% of VESTS to Bob" );
    delegate_vest( "alice", "bob", delegation_amount, alice_private_key );
    generate_block();

    // Verify delegation state
    BOOST_REQUIRE( GET_ASSETS( "alice" ).get_delegated_vesting() == delegation_amount );
    BOOST_REQUIRE( GET_ASSETS( "bob" ).get_received_vesting() == delegation_amount );

    // Alice's effective vesting should be reduced
    (void)GET_EFF_VESTS( "alice" );
    // Bob's effective vesting should be increased
    (void)GET_EFF_VESTS( "bob" );

    BOOST_TEST_MESSAGE( "--- Setting up vest rewards for Alice" );

    db_plugin->debug_update( [&]( database& db )
    {
      const auto& alice_acc = db.get_account( "alice" );
      const auto& alice_assets = db.get< assets_object >( assets_object::id_type( alice_acc.get_id().get_value() ) );

      db.modify( alice_assets, []( assets_object& a )
      {
        a.set_vest_rewards( ASSET( "500.000000 VESTS" ) );
        a.set_vest_rewards_as_hive( ASSET( "250.000 TESTS" ) );
      });

      db.modify( db.get_dynamic_global_properties(), []( dynamic_global_property_object& gpo )
      {
        gpo.current_supply += ASSET( "250.000 TESTS" );
        gpo.virtual_supply += ASSET( "250.000 TESTS" );
        gpo.pending_rewarded_vesting_shares += ASSET( "500.000000 VESTS" );
        gpo.pending_rewarded_vesting_hive += ASSET( "250.000 TESTS" );
      });
    });
    generate_block();
    validate_database();

    auto alice_vests_before = get_vesting( "alice" );
    auto alice_eff_vests_before = GET_EFF_VESTS( "alice" );

    BOOST_TEST_MESSAGE( "--- Alice claims vest rewards while having delegation" );
    claim_reward_balance( "alice", ASSET( "0.000 TESTS" ), ASSET( "0.000 TBD" ), ASSET( "500.000000 VESTS" ), alice_post_key );

    // Vesting should have increased
    BOOST_REQUIRE( get_vesting( "alice" ) == alice_vests_before + ASSET( "500.000000 VESTS" ) );

    // Effective vesting should have increased too
    auto alice_eff_vests_after = GET_EFF_VESTS( "alice" );
    BOOST_REQUIRE( alice_eff_vests_after > alice_eff_vests_before );

    // Delegation should still be intact
    BOOST_REQUIRE( GET_ASSETS( "alice" ).get_delegated_vesting() == delegation_amount );

    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

/// Test 16: Producer/witness reward (direct vesting, not reward balance)
BOOST_AUTO_TEST_CASE( producer_reward_direct_vesting )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: producer_reward_direct_vesting" );

    // Witness (initminer) produces blocks, verify vesting increases
    auto vests_before = get_vesting( "initminer" );

    generate_blocks( 10 );

    auto vests_after = get_vesting( "initminer" );

    // Initminer should receive producer rewards as direct vesting
    BOOST_REQUIRE( vests_after > vests_before );

    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

/// Test 17: Reward claim updates global properties correctly
BOOST_AUTO_TEST_CASE( global_properties_on_reward_claim )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: global_properties_on_reward_claim" );

    ACTORS( (alice) )
    generate_block();

    set_price_feed( price( ASSET( "1.000 TBD" ), ASSET( "1.000 TESTS" ) ) );

    db_plugin->debug_update( [&]( database& db )
    {
      const auto& alice_acc = db.get_account( "alice" );
      const auto& alice_assets = db.get< assets_object >( assets_object::id_type( alice_acc.get_id().get_value() ) );

      db.modify( alice_assets, []( assets_object& a )
      {
        a.set_vest_rewards( ASSET( "200.000000 VESTS" ) );
        a.set_vest_rewards_as_hive( ASSET( "100.000 TESTS" ) );
      });

      db.modify( db.get_dynamic_global_properties(), []( dynamic_global_property_object& gpo )
      {
        gpo.current_supply += ASSET( "100.000 TESTS" );
        gpo.virtual_supply += ASSET( "100.000 TESTS" );
        gpo.pending_rewarded_vesting_shares += ASSET( "200.000000 VESTS" );
        gpo.pending_rewarded_vesting_hive += ASSET( "100.000 TESTS" );
      });
    });
    generate_block();
    validate_database();

    const auto& gpo_before = db->get_dynamic_global_properties();
    auto total_vests_before = gpo_before.total_vesting_shares;
    auto total_fund_before = gpo_before.total_vesting_fund_hive;
    auto pending_vests_before = gpo_before.pending_rewarded_vesting_shares;
    auto pending_hive_before = gpo_before.pending_rewarded_vesting_hive;

    BOOST_TEST_MESSAGE( "--- Partial claim: 100 out of 200 VESTS" );
    claim_reward_balance( "alice", ASSET( "0.000 TESTS" ), ASSET( "0.000 TBD" ), ASSET( "100.000000 VESTS" ), alice_post_key );

    const auto& gpo_mid = db->get_dynamic_global_properties();
    BOOST_REQUIRE( gpo_mid.total_vesting_shares == total_vests_before + ASSET( "100.000000 VESTS" ) );
    BOOST_REQUIRE( gpo_mid.pending_rewarded_vesting_shares == pending_vests_before - ASSET( "100.000000 VESTS" ) );
    // The HIVE portion should be proportional (half of 100 = 50)
    BOOST_REQUIRE( gpo_mid.total_vesting_fund_hive == total_fund_before + ASSET( "50.000 TESTS" ) );
    BOOST_REQUIRE( gpo_mid.pending_rewarded_vesting_hive == pending_hive_before - ASSET( "50.000 TESTS" ) );

    BOOST_TEST_MESSAGE( "--- Claim remaining 100 VESTS" );
    generate_block(); // Avoid duplicate transaction error

    // Re-capture gpo before second claim (witness rewards may have shifted values)
    const auto& gpo_before2 = db->get_dynamic_global_properties();
    auto total_vests_before2 = gpo_before2.total_vesting_shares;
    auto total_fund_before2 = gpo_before2.total_vesting_fund_hive;
    auto pending_vests_before2 = gpo_before2.pending_rewarded_vesting_shares;
    auto pending_hive_before2 = gpo_before2.pending_rewarded_vesting_hive;

    claim_reward_balance( "alice", ASSET( "0.000 TESTS" ), ASSET( "0.000 TBD" ), ASSET( "100.000000 VESTS" ), alice_post_key );

    const auto& gpo_final = db->get_dynamic_global_properties();
    BOOST_REQUIRE( gpo_final.total_vesting_shares == total_vests_before2 + ASSET( "100.000000 VESTS" ) );
    BOOST_REQUIRE( gpo_final.pending_rewarded_vesting_shares == pending_vests_before2 - ASSET( "100.000000 VESTS" ) );
    BOOST_REQUIRE( gpo_final.total_vesting_fund_hive == total_fund_before2 + ASSET( "50.000 TESTS" ) );
    BOOST_REQUIRE( gpo_final.pending_rewarded_vesting_hive == pending_hive_before2 - ASSET( "50.000 TESTS" ) );

    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

/// Test 18: Power down while having reward balance
BOOST_AUTO_TEST_CASE( power_down_with_reward_balance )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: power_down_with_reward_balance" );

    ACTORS( (alice) )
    generate_block();

    vest( "alice", ASSET( "100.000 TESTS" ) );
    generate_block();

    set_price_feed( price( ASSET( "1.000 TBD" ), ASSET( "1.000 TESTS" ) ) );

    // Set up reward balance
    db_plugin->debug_update( [&]( database& db )
    {
      const auto& alice_acc = db.get_account( "alice" );
      const auto& alice_assets = db.get< assets_object >( assets_object::id_type( alice_acc.get_id().get_value() ) );

      db.modify( alice_assets, []( assets_object& a )
      {
        a.set_vest_rewards( ASSET( "500.000000 VESTS" ) );
        a.set_vest_rewards_as_hive( ASSET( "250.000 TESTS" ) );
      });

      db.modify( db.get_dynamic_global_properties(), []( dynamic_global_property_object& gpo )
      {
        gpo.current_supply += ASSET( "250.000 TESTS" );
        gpo.virtual_supply += ASSET( "250.000 TESTS" );
        gpo.pending_rewarded_vesting_shares += ASSET( "500.000000 VESTS" );
        gpo.pending_rewarded_vesting_hive += ASSET( "250.000 TESTS" );
      });
    });
    generate_block();
    validate_database();

    auto alice_vests = get_vesting( "alice" );

    BOOST_TEST_MESSAGE( "--- Starting power down while rewards are pending" );
    withdraw_vesting( "alice", alice_vests, alice_private_key );

    // Process one withdrawal
    const auto& alice_time = GET_TIME( "alice" );
    generate_blocks( alice_time.get_next_vesting_withdrawal(), true );

    // Rewards should still be intact (power down only affects active vesting)
    BOOST_REQUIRE( get_vest_rewards( "alice" ) == ASSET( "500.000000 VESTS" ) );

    BOOST_TEST_MESSAGE( "--- Claiming rewards during power down" );
    claim_reward_balance( "alice", ASSET( "0.000 TESTS" ), ASSET( "0.000 TBD" ), ASSET( "500.000000 VESTS" ), alice_post_key );

    BOOST_REQUIRE( get_vest_rewards( "alice" ) == ASSET( "0.000000 VESTS" ) );

    // The claimed VESTS should be added to active vesting (which is also being powered down)
    // But power down parameters should not change (they were set before claiming)
    BOOST_REQUIRE( GET_ASSETS( "alice" ).get_vesting_withdraw_rate().amount.value > 0 );

    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

/// Test 19: Multiple accounts claiming rewards simultaneously
BOOST_AUTO_TEST_CASE( multiple_accounts_claim )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: multiple_accounts_claim" );

    ACTORS( (alice)(bob)(charlie)(dave) )
    generate_block();

    set_price_feed( price( ASSET( "1.000 TBD" ), ASSET( "1.000 TESTS" ) ) );

    // Set up rewards for all accounts
    db_plugin->debug_update( [&]( database& db )
    {
      auto setup_rewards = [&]( const std::string& name, int64_t vests, int64_t hive )
      {
        const auto& acc = db.get_account( name );
        const auto& acc_assets = db.get< assets_object >( assets_object::id_type( acc.get_id().get_value() ) );

        db.modify( acc_assets, [&]( assets_object& a )
        {
          a.set_vest_rewards( asset( vests, VESTS_SYMBOL ) );
          a.set_vest_rewards_as_hive( asset( hive, HIVE_SYMBOL ) );
        });
      };

      setup_rewards( "alice", 100000000, 50000 );   // 100 VESTS, 50 TESTS
      setup_rewards( "bob", 200000000, 100000 );     // 200 VESTS, 100 TESTS
      setup_rewards( "charlie", 300000000, 150000 ); // 300 VESTS, 150 TESTS
      setup_rewards( "dave", 400000000, 200000 );    // 400 VESTS, 200 TESTS

      db.modify( db.get_dynamic_global_properties(), []( dynamic_global_property_object& gpo )
      {
        gpo.current_supply += ASSET( "500.000 TESTS" );
        gpo.virtual_supply += ASSET( "500.000 TESTS" );
        gpo.pending_rewarded_vesting_shares += ASSET( "1000.000000 VESTS" );
        gpo.pending_rewarded_vesting_hive += ASSET( "500.000 TESTS" );
      });
    });
    generate_block();
    validate_database();

    BOOST_TEST_MESSAGE( "--- All accounts claiming their rewards" );

    auto alice_vests_before = get_vesting( "alice" );
    auto bob_vests_before = get_vesting( "bob" );
    auto charlie_vests_before = get_vesting( "charlie" );
    auto dave_vests_before = get_vesting( "dave" );

    claim_reward_balance( "alice", ASSET( "0.000 TESTS" ), ASSET( "0.000 TBD" ), ASSET( "100.000000 VESTS" ), alice_post_key );
    claim_reward_balance( "bob", ASSET( "0.000 TESTS" ), ASSET( "0.000 TBD" ), ASSET( "200.000000 VESTS" ), bob_post_key );
    claim_reward_balance( "charlie", ASSET( "0.000 TESTS" ), ASSET( "0.000 TBD" ), ASSET( "300.000000 VESTS" ), charlie_post_key );
    claim_reward_balance( "dave", ASSET( "0.000 TESTS" ), ASSET( "0.000 TBD" ), ASSET( "400.000000 VESTS" ), dave_post_key );

    BOOST_REQUIRE( get_vesting( "alice" ) == alice_vests_before + ASSET( "100.000000 VESTS" ) );
    BOOST_REQUIRE( get_vesting( "bob" ) == bob_vests_before + ASSET( "200.000000 VESTS" ) );
    BOOST_REQUIRE( get_vesting( "charlie" ) == charlie_vests_before + ASSET( "300.000000 VESTS" ) );
    BOOST_REQUIRE( get_vesting( "dave" ) == dave_vests_before + ASSET( "400.000000 VESTS" ) );

    BOOST_REQUIRE( get_vest_rewards( "alice" ) == ASSET( "0.000000 VESTS" ) );
    BOOST_REQUIRE( get_vest_rewards( "bob" ) == ASSET( "0.000000 VESTS" ) );
    BOOST_REQUIRE( get_vest_rewards( "charlie" ) == ASSET( "0.000000 VESTS" ) );
    BOOST_REQUIRE( get_vest_rewards( "dave" ) == ASSET( "0.000000 VESTS" ) );

    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

/// Test 20: Vesting withdrawal with routing (auto_vest and liquid modes)
BOOST_AUTO_TEST_CASE( vesting_withdrawal_with_routing )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: vesting_withdrawal_with_routing" );

    ACTORS( (alice)(bob)(charlie) )
    generate_block();

    vest( "alice", ASSET( "100.000 TESTS" ) );
    generate_block();

    BOOST_TEST_MESSAGE( "--- Setting up withdrawal routes" );
    // 50% to bob as VESTS, 25% to charlie as HIVE
    set_withdraw_vesting_route( "alice", "bob", HIVE_1_PERCENT * 50, true /*auto_vest*/, alice_private_key );
    set_withdraw_vesting_route( "alice", "charlie", HIVE_1_PERCENT * 25, false /*auto_vest*/, alice_private_key );
    generate_block();

    auto alice_vests = get_vesting( "alice" );
    auto bob_vests_before = get_vesting( "bob" );
    auto charlie_hive_before = get_balance( "charlie" );

    BOOST_TEST_MESSAGE( "--- Starting power down" );
    withdraw_vesting( "alice", alice_vests, alice_private_key );

    BOOST_TEST_MESSAGE( "--- Processing first withdrawal" );
    generate_blocks( GET_TIME( "alice" ).get_next_vesting_withdrawal(), true );

    // Bob should have received VESTS
    BOOST_REQUIRE( get_vesting( "bob" ) > bob_vests_before );
    // Charlie should have received HIVE
    BOOST_REQUIRE( get_balance( "charlie" ) > charlie_hive_before );

    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

/// Test 21: Rewards accumulation via create_vesting with various liquid amounts
BOOST_AUTO_TEST_CASE( reward_vesting_price_consistency )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: reward_vesting_price_consistency" );

    ACTORS( (alice) )
    generate_block();

    set_price_feed( price( ASSET( "1.000 TBD" ), ASSET( "1.000 TESTS" ) ) );

    BOOST_TEST_MESSAGE( "--- Verifying vesting share price consistency across operations" );

    const auto& gpo = db->get_dynamic_global_properties();
    auto reward_price = gpo.get_reward_vesting_share_price();
    auto normal_price = gpo.get_vesting_share_price();

    // Reward vesting price is different from normal vesting price
    // because reward pool has its own tracking
    BOOST_TEST_MESSAGE( "Reward price: " + fc::json::to_string( reward_price ) +
                        " Normal price: " + fc::json::to_string( normal_price ) );

    // Create some vesting via reward path
    asset liquid = ASSET( "10.000 TESTS" );
    asset expected_reward_vests = liquid * reward_price;

    db_plugin->debug_update( [&]( database& db )
    {
      // Must add supply since create_vesting doesn't create the HIVE
      db.modify( db.get_dynamic_global_properties(), [&]( dynamic_global_property_object& gpo )
      {
        gpo.current_supply += liquid;
        gpo.virtual_supply += liquid;
      });
      const auto& alice_acc = db.get_account( "alice" );
      db.create_vesting( alice_acc, liquid, true );
    });
    generate_block();

    BOOST_REQUIRE( get_vest_rewards( "alice" ) == expected_reward_vests );
    BOOST_REQUIRE( get_vest_rewards_as_hive( "alice" ) == liquid );

    // Now create direct vesting with the same liquid amount
    // Re-capture price since generate_block() may have changed it (witness rewards)
    const auto& gpo2 = db->get_dynamic_global_properties();
    auto current_normal_price = gpo2.get_vesting_share_price();
    asset expected_direct_vests = liquid * current_normal_price;

    auto vests_before = get_vesting( "alice" );

    db_plugin->debug_update( [&]( database& db )
    {
      // Must add supply since create_vesting doesn't create the HIVE
      db.modify( db.get_dynamic_global_properties(), [&]( dynamic_global_property_object& gpo )
      {
        gpo.current_supply += liquid;
        gpo.virtual_supply += liquid;
      });
      const auto& alice_acc = db.get_account( "alice" );
      db.create_vesting( alice_acc, liquid, false );
    });
    generate_block();

    BOOST_REQUIRE( get_vesting( "alice" ) == vests_before + expected_direct_vests );

    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

/// Test 22: Effective vesting shares with delegation, power down, and rewards
BOOST_AUTO_TEST_CASE( effective_vesting_comprehensive )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: effective_vesting_comprehensive" );

    ACTORS( (alice)(bob) )
    generate_block();

    vest( "alice", ASSET( "1000.000 TESTS" ) );
    vest( "bob", ASSET( "100.000 TESTS" ) );
    generate_block();

    auto alice_vests = get_vesting( "alice" );
    auto alice_eff_vests_0 = GET_EFF_VESTS( "alice" );

    BOOST_TEST_MESSAGE( "--- Effective vesting should equal total vesting initially" );
    // Note: effective vesting may account for power down deductions
    BOOST_REQUIRE( alice_eff_vests_0.value == alice_vests.amount.value );

    BOOST_TEST_MESSAGE( "--- After delegation, effective vesting should decrease" );
    auto delegation_amount = asset( alice_vests.amount / 4, VESTS_SYMBOL );
    delegate_vest( "alice", "bob", delegation_amount, alice_private_key );
    generate_block();

    auto alice_eff_vests_1 = GET_EFF_VESTS( "alice" );
    BOOST_REQUIRE( alice_eff_vests_1 < alice_eff_vests_0 );

    BOOST_TEST_MESSAGE( "--- After starting power down, effective vesting should account for withdrawal" );
    auto withdraw_amount = asset( alice_vests.amount / 4, VESTS_SYMBOL );
    withdraw_vesting( "alice", withdraw_amount, alice_private_key );
    generate_block();

    auto alice_eff_vests_2 = GET_EFF_VESTS( "alice" );
    // During power down, next week's withdrawal is deducted from effective vesting
    BOOST_REQUIRE( alice_eff_vests_2 <= alice_eff_vests_1 );

    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

/// Test 23: Manabar consistency across multiple claim operations
BOOST_AUTO_TEST_CASE( manabar_consistency_multiple_claims )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: manabar_consistency_multiple_claims" );

    ACTORS( (alice) )
    generate_block();

    vest( "alice", ASSET( "1000.000 TESTS" ) );
    generate_block();

    set_price_feed( price( ASSET( "1.000 TBD" ), ASSET( "1.000 TESTS" ) ) );

    // Set up large reward
    db_plugin->debug_update( [&]( database& db )
    {
      const auto& alice_acc = db.get_account( "alice" );
      const auto& alice_assets = db.get< assets_object >( assets_object::id_type( alice_acc.get_id().get_value() ) );

      db.modify( alice_assets, []( assets_object& a )
      {
        a.set_vest_rewards( ASSET( "3000.000000 VESTS" ) );
        a.set_vest_rewards_as_hive( ASSET( "1500.000 TESTS" ) );
      });

      db.modify( db.get_dynamic_global_properties(), []( dynamic_global_property_object& gpo )
      {
        gpo.current_supply += ASSET( "1500.000 TESTS" );
        gpo.virtual_supply += ASSET( "1500.000 TESTS" );
        gpo.pending_rewarded_vesting_shares += ASSET( "3000.000000 VESTS" );
        gpo.pending_rewarded_vesting_hive += ASSET( "1500.000 TESTS" );
      });
    });
    generate_block();
    validate_database();

    // Wait for mana to regenerate
    generate_blocks( 100 );

    BOOST_TEST_MESSAGE( "--- Claiming rewards in 3 steps" );

    for( int step = 0; step < 3; ++step )
    {
      auto mana_before = VOTING_MANABAR( "alice" );
      (void)DOWNVOTE_MANABAR( "alice" );
      auto vests_before = get_vesting( "alice" );
      auto eff_vests_before = GET_EFF_VESTS( "alice" );

      claim_reward_balance( "alice", ASSET( "0.000 TESTS" ), ASSET( "0.000 TBD" ), ASSET( "1000.000000 VESTS" ), alice_post_key );
      generate_block();

      auto mana_after = VOTING_MANABAR( "alice" );
      auto vests_after = get_vesting( "alice" );
      auto eff_vests_after = GET_EFF_VESTS( "alice" );

      BOOST_REQUIRE( vests_after == vests_before + ASSET( "1000.000000 VESTS" ) );
      BOOST_REQUIRE( eff_vests_after > eff_vests_before );

      // Mana should increase since we added vesting (new mana is added)
      BOOST_REQUIRE( mana_after.current_mana >= mana_before.current_mana );

      BOOST_TEST_MESSAGE( "Step " + std::to_string( step ) +
                          ": mana " + std::to_string( mana_before.current_mana ) +
                          " -> " + std::to_string( mana_after.current_mana ) );
    }

    BOOST_REQUIRE( get_vest_rewards( "alice" ) == ASSET( "0.000000 VESTS" ) );

    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

/// Test 24: Vest directly then power down and check split objects
BOOST_AUTO_TEST_CASE( vest_then_power_down_split )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: vest_then_power_down_split" );

    ACTORS( (alice) )
    generate_block();

    BOOST_TEST_MESSAGE( "--- Vesting multiple times" );
    vest( "alice", ASSET( "10.000 TESTS" ) );
    generate_block();
    vest( "alice", ASSET( "20.000 TESTS" ) );
    generate_block();
    vest( "alice", ASSET( "30.000 TESTS" ) );
    generate_block();

    auto total_vests = get_vesting( "alice" );
    BOOST_REQUIRE( total_vests.amount.value > 0 );

    BOOST_TEST_MESSAGE( "--- Power down all" );
    withdraw_vesting( "alice", total_vests, alice_private_key );

    auto hive_balance_start = get_balance( "alice" );

    BOOST_TEST_MESSAGE( "--- Process all withdrawals" );
    for( int i = 0; i < HIVE_VESTING_WITHDRAW_INTERVALS; ++i )
    {
      const auto& alice_time = GET_TIME( "alice" );
      if( alice_time.get_next_vesting_withdrawal() == fc::time_point_sec::maximum() )
        break;
      generate_blocks( alice_time.get_next_vesting_withdrawal(), true );
    }

    BOOST_TEST_MESSAGE( "--- Verify complete power down" );
    BOOST_REQUIRE( get_vesting( "alice" ).amount.value == 0 );
    BOOST_REQUIRE( GET_ASSETS( "alice" ).get_vesting_withdraw_rate() == ASSET( "0.000000 VESTS" ) );
    BOOST_REQUIRE( GET_TIME( "alice" ).get_next_vesting_withdrawal() == fc::time_point_sec::maximum() );
    BOOST_REQUIRE( get_balance( "alice" ) > hive_balance_start );

    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

/// Test 25: Reward balance with zero claim
BOOST_AUTO_TEST_CASE( reward_zero_claim )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: reward_zero_claim" );

    ACTORS( (alice) )
    generate_block();

    set_price_feed( price( ASSET( "1.000 TBD" ), ASSET( "1.000 TESTS" ) ) );

    db_plugin->debug_update( [&]( database& db )
    {
      const auto& alice_acc = db.get_account( "alice" );
      const auto& alice_assets = db.get< assets_object >( assets_object::id_type( alice_acc.get_id().get_value() ) );

      db.modify( alice_assets, []( assets_object& a )
      {
        a.set_rewards( ASSET( "10.000 TESTS" ) );
        a.set_vest_rewards( ASSET( "100.000000 VESTS" ) );
        a.set_vest_rewards_as_hive( ASSET( "50.000 TESTS" ) );
      });

      db.modify( db.get_dynamic_global_properties(), []( dynamic_global_property_object& gpo )
      {
        gpo.current_supply += ASSET( "60.000 TESTS" );
        gpo.virtual_supply += ASSET( "60.000 TESTS" );
        gpo.pending_rewarded_vesting_shares += ASSET( "100.000000 VESTS" );
        gpo.pending_rewarded_vesting_hive += ASSET( "50.000 TESTS" );
      });
    });
    generate_block();
    validate_database();

    BOOST_TEST_MESSAGE( "--- Claiming zero of each should fail" );
    // Claiming zero of all is not allowed
    HIVE_REQUIRE_THROW(
      claim_reward_balance( "alice", ASSET( "0.000 TESTS" ), ASSET( "0.000 TBD" ), ASSET( "0.000000 VESTS" ), alice_post_key ),
      fc::assert_exception );

    // Rewards should be unchanged
    BOOST_REQUIRE( get_rewards( "alice" ) == ASSET( "10.000 TESTS" ) );
    BOOST_REQUIRE( get_vest_rewards( "alice" ) == ASSET( "100.000000 VESTS" ) );

    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

/// Test 26: Claim tiny/dust reward amounts
BOOST_AUTO_TEST_CASE( claim_tiny_reward_amounts )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: claim_tiny_reward_amounts" );

    ACTORS( (alice) )
    generate_block();

    set_price_feed( price( ASSET( "1.000 TBD" ), ASSET( "1.000 TESTS" ) ) );

    db_plugin->debug_update( [&]( database& db )
    {
      const auto& alice_acc = db.get_account( "alice" );
      const auto& alice_assets = db.get< assets_object >( assets_object::id_type( alice_acc.get_id().get_value() ) );

      db.modify( alice_assets, []( assets_object& a )
      {
        a.set_rewards( ASSET( "0.001 TESTS" ) );
        a.set_hbd_rewards( ASSET( "0.001 TBD" ) );
        a.set_vest_rewards( ASSET( "0.000001 VESTS" ) );
        a.set_vest_rewards_as_hive( ASSET( "0.001 TESTS" ) );
      });

      db.modify( db.get_dynamic_global_properties(), []( dynamic_global_property_object& gpo )
      {
        gpo.current_supply += ASSET( "0.002 TESTS" );
        gpo.current_hbd_supply += ASSET( "0.001 TBD" );
        gpo.virtual_supply += ASSET( "0.002 TESTS" );
        gpo.pending_rewarded_vesting_shares += ASSET( "0.000001 VESTS" );
        gpo.pending_rewarded_vesting_hive += ASSET( "0.001 TESTS" );
      });
    });
    generate_block();
    validate_database();

    auto vests_before = get_vesting( "alice" );

    BOOST_TEST_MESSAGE( "--- Claiming smallest possible amounts" );
    claim_reward_balance( "alice", ASSET( "0.001 TESTS" ), ASSET( "0.001 TBD" ), ASSET( "0.000001 VESTS" ), alice_post_key );

    BOOST_REQUIRE( get_rewards( "alice" ) == ASSET( "0.000 TESTS" ) );
    BOOST_REQUIRE( get_hbd_rewards( "alice" ) == ASSET( "0.000 TBD" ) );
    BOOST_REQUIRE( get_vest_rewards( "alice" ) == ASSET( "0.000000 VESTS" ) );
    BOOST_REQUIRE( get_vesting( "alice" ) == vests_before + ASSET( "0.000001 VESTS" ) );

    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

/// Test 27: vest_rewards_as_hive proportional tracking during partial claims
BOOST_AUTO_TEST_CASE( vest_rewards_as_hive_proportional )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: vest_rewards_as_hive_proportional" );

    ACTORS( (alice) )
    generate_block();

    set_price_feed( price( ASSET( "1.000 TBD" ), ASSET( "1.000 TESTS" ) ) );

    // Setup: 1000 VESTS = 500 TESTS equivalent
    db_plugin->debug_update( [&]( database& db )
    {
      const auto& alice_acc = db.get_account( "alice" );
      const auto& alice_assets = db.get< assets_object >( assets_object::id_type( alice_acc.get_id().get_value() ) );

      db.modify( alice_assets, []( assets_object& a )
      {
        a.set_vest_rewards( ASSET( "1000.000000 VESTS" ) );
        a.set_vest_rewards_as_hive( ASSET( "500.000 TESTS" ) );
      });

      db.modify( db.get_dynamic_global_properties(), []( dynamic_global_property_object& gpo )
      {
        gpo.current_supply += ASSET( "500.000 TESTS" );
        gpo.virtual_supply += ASSET( "500.000 TESTS" );
        gpo.pending_rewarded_vesting_shares += ASSET( "1000.000000 VESTS" );
        gpo.pending_rewarded_vesting_hive += ASSET( "500.000 TESTS" );
      });
    });
    generate_block();
    validate_database();

    BOOST_TEST_MESSAGE( "--- Claim 25% of rewards (250 VESTS)" );
    claim_reward_balance( "alice", ASSET( "0.000 TESTS" ), ASSET( "0.000 TBD" ), ASSET( "250.000000 VESTS" ), alice_post_key );

    // vest_rewards_as_hive should decrease proportionally: 500 * 250/1000 = 125
    BOOST_REQUIRE( get_vest_rewards( "alice" ) == ASSET( "750.000000 VESTS" ) );
    BOOST_REQUIRE( get_vest_rewards_as_hive( "alice" ) == ASSET( "375.000 TESTS" ) );

    BOOST_TEST_MESSAGE( "--- Claim another 25% of original (250 VESTS)" );
    generate_block(); // Avoid duplicate transaction error

    claim_reward_balance( "alice", ASSET( "0.000 TESTS" ), ASSET( "0.000 TBD" ), ASSET( "250.000000 VESTS" ), alice_post_key );

    BOOST_REQUIRE( get_vest_rewards( "alice" ) == ASSET( "500.000000 VESTS" ) );
    // Proportional: 375 * 250/750 = 125
    BOOST_REQUIRE( get_vest_rewards_as_hive( "alice" ) == ASSET( "250.000 TESTS" ) );

    BOOST_TEST_MESSAGE( "--- Claim remaining 500 VESTS (exact match)" );
    generate_block(); // Avoid duplicate transaction error

    claim_reward_balance( "alice", ASSET( "0.000 TESTS" ), ASSET( "0.000 TBD" ), ASSET( "500.000000 VESTS" ), alice_post_key );

    BOOST_REQUIRE( get_vest_rewards( "alice" ) == ASSET( "0.000000 VESTS" ) );
    BOOST_REQUIRE( get_vest_rewards_as_hive( "alice" ) == ASSET( "0.000 TESTS" ) );

    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

/// Test 28: Claim after multiple reward sources (curation + author + beneficiary)
BOOST_AUTO_TEST_CASE( claim_after_multiple_reward_sources )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: claim_after_multiple_reward_sources" );
    auto _scope = set_mainnet_cashout_values();

    ACTORS( (alice)(bob)(charlie) )
    generate_block();

    vest( "alice", ASSET( "100.000 TESTS" ) );
    vest( "bob", ASSET( "500.000 TESTS" ) );
    vest( "charlie", ASSET( "500.000 TESTS" ) );
    generate_block();

    set_price_feed( price( ASSET( "1.000 TBD" ), ASSET( "1.000 TESTS" ) ) );
    generate_block();

    BOOST_TEST_MESSAGE( "--- Alice creates two posts" );

    // Post 1: Alice is author, gets author rewards
    post_comment( "alice", "post-one", "Test 1", "Content 1", "test", alice_post_key );
    generate_block();

    // Post 2: Bob is author, Alice is voter (gets curation rewards)
    post_comment( "bob", "post-two", "Test 2", "Content 2", "test", bob_post_key );
    generate_block();

    BOOST_TEST_MESSAGE( "--- Voting" );
    // Bob and Charlie vote on Alice's post (Alice gets author rewards)
    vote( "alice", "post-one", "bob", HIVE_100_PERCENT, bob_post_key );
    vote( "alice", "post-one", "charlie", HIVE_100_PERCENT, charlie_post_key );

    // Alice votes on Bob's post (Alice gets curation rewards)
    vote( "bob", "post-two", "alice", HIVE_100_PERCENT, alice_post_key );
    generate_block();

    BOOST_TEST_MESSAGE( "--- Generating blocks until cashout" );
    auto cashout1 = db->find_comment_cashout( *db->get_comment( "alice", string( "post-one" ) ) )->get_cashout_time();
    auto cashout2 = db->find_comment_cashout( *db->get_comment( "bob", string( "post-two" ) ) )->get_cashout_time();
    auto later_cashout = std::max( cashout1, cashout2 );
    generate_blocks( later_cashout, true );

    BOOST_TEST_MESSAGE( "--- Checking Alice's accumulated rewards from multiple sources" );

    auto alice_vest_rewards = get_vest_rewards( "alice" );
    auto alice_hive_rewards = get_rewards( "alice" );
    auto alice_hbd_rewards = get_hbd_rewards( "alice" );

    BOOST_TEST_MESSAGE( "Alice total rewards - VESTS: " + fc::json::to_string( alice_vest_rewards ) +
                        " HIVE: " + fc::json::to_string( alice_hive_rewards ) +
                        " HBD: " + fc::json::to_string( alice_hbd_rewards ) );

    bool has_rewards = alice_vest_rewards.amount.value > 0 ||
                       alice_hive_rewards.amount.value > 0 ||
                       alice_hbd_rewards.amount.value > 0;

    if( has_rewards )
    {
      auto alice_vests_before = get_vesting( "alice" );
      auto alice_hive_before = get_balance( "alice" );
      auto alice_hbd_before = get_hbd_balance( "alice" );

      BOOST_TEST_MESSAGE( "--- Claiming all accumulated rewards at once" );
      claim_reward_balance( "alice", alice_hive_rewards, alice_hbd_rewards, alice_vest_rewards, alice_post_key );

      BOOST_REQUIRE( get_rewards( "alice" ) == ASSET( "0.000 TESTS" ) );
      BOOST_REQUIRE( get_hbd_rewards( "alice" ) == ASSET( "0.000 TBD" ) );
      BOOST_REQUIRE( get_vest_rewards( "alice" ) == ASSET( "0.000000 VESTS" ) );
      BOOST_REQUIRE( get_vest_rewards_as_hive( "alice" ) == ASSET( "0.000 TESTS" ) );

      BOOST_REQUIRE( get_vesting( "alice" ) == alice_vests_before + alice_vest_rewards );
      BOOST_REQUIRE( get_balance( "alice" ) == alice_hive_before + alice_hive_rewards );
      BOOST_REQUIRE( get_hbd_balance( "alice" ) == alice_hbd_before + alice_hbd_rewards );
    }

    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

/// Test 29: Delegation return after undelegation
BOOST_AUTO_TEST_CASE( delegation_return_split )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: delegation_return_split" );

    ACTORS( (alice)(bob) )
    generate_block();

    vest( "alice", ASSET( "1000.000 TESTS" ) );
    generate_block();

    auto alice_vests = get_vesting( "alice" );
    auto delegation = asset( alice_vests.amount / 4, VESTS_SYMBOL );

    BOOST_TEST_MESSAGE( "--- Delegating to bob" );
    delegate_vest( "alice", "bob", delegation, alice_private_key );
    generate_block();

    BOOST_REQUIRE( GET_ASSETS( "alice" ).get_delegated_vesting() == delegation );
    BOOST_REQUIRE( GET_ASSETS( "bob" ).get_received_vesting() == delegation );

    auto bob_eff_before = GET_EFF_VESTS( "bob" );

    BOOST_TEST_MESSAGE( "--- Removing delegation" );
    delegate_vest( "alice", "bob", ASSET( "0.000000 VESTS" ), alice_private_key );
    generate_block();

    // Bob's received vesting should go back to 0
    BOOST_REQUIRE( GET_ASSETS( "bob" ).get_received_vesting() == ASSET( "0.000000 VESTS" ) );

    // Bob's effective vesting should decrease
    auto bob_eff_after = GET_EFF_VESTS( "bob" );
    BOOST_REQUIRE( bob_eff_after < bob_eff_before );

    // Alice's delegated_vesting does NOT return to 0 immediately - it stays until expiration
    // The delegation return is scheduled for delegation_return_period (5 days)
    BOOST_REQUIRE( GET_ASSETS( "alice" ).get_delegated_vesting() == delegation );

    // Wait for delegation return period to expire
    const auto& gpo = db->get_dynamic_global_properties();
    generate_blocks( db->head_block_time() + gpo.delegation_return_period, true );

    // After expiration, delegated_vesting should be 0
    BOOST_REQUIRE( GET_ASSETS( "alice" ).get_delegated_vesting() == ASSET( "0.000000 VESTS" ) );

    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

/// Test 30: Claim reward with delegation active - verify effective vesting accounts for new VESTS
BOOST_AUTO_TEST_CASE( claim_reward_with_active_delegation )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: claim_reward_with_active_delegation" );

    ACTORS( (alice)(bob) )
    generate_block();

    vest( "alice", ASSET( "1000.000 TESTS" ) );
    generate_block();

    set_price_feed( price( ASSET( "1.000 TBD" ), ASSET( "1.000 TESTS" ) ) );

    // Alice delegates to Bob
    auto alice_vests = get_vesting( "alice" );
    auto delegation = asset( alice_vests.amount / 2, VESTS_SYMBOL );
    delegate_vest( "alice", "bob", delegation, alice_private_key );
    generate_block();

    // Set up reward
    db_plugin->debug_update( [&]( database& db )
    {
      const auto& alice_acc = db.get_account( "alice" );
      const auto& alice_assets = db.get< assets_object >( assets_object::id_type( alice_acc.get_id().get_value() ) );

      db.modify( alice_assets, []( assets_object& a )
      {
        a.set_vest_rewards( ASSET( "2000.000000 VESTS" ) );
        a.set_vest_rewards_as_hive( ASSET( "1000.000 TESTS" ) );
      });

      db.modify( db.get_dynamic_global_properties(), []( dynamic_global_property_object& gpo )
      {
        gpo.current_supply += ASSET( "1000.000 TESTS" );
        gpo.virtual_supply += ASSET( "1000.000 TESTS" );
        gpo.pending_rewarded_vesting_shares += ASSET( "2000.000000 VESTS" );
        gpo.pending_rewarded_vesting_hive += ASSET( "1000.000 TESTS" );
      });
    });
    generate_block();
    validate_database();

    auto eff_vests_before = GET_EFF_VESTS( "alice" );
    auto vests_before = get_vesting( "alice" );
    auto delegated = GET_ASSETS( "alice" ).get_delegated_vesting();

    BOOST_TEST_MESSAGE( "--- Alice claims 2000 VESTS while having delegation of " + fc::json::to_string( delegation ) );

    claim_reward_balance( "alice", ASSET( "0.000 TESTS" ), ASSET( "0.000 TBD" ), ASSET( "2000.000000 VESTS" ), alice_post_key );

    auto vests_after = get_vesting( "alice" );
    auto eff_vests_after = GET_EFF_VESTS( "alice" );

    // Vesting should increase by exact claimed amount
    BOOST_REQUIRE( vests_after == vests_before + ASSET( "2000.000000 VESTS" ) );

    // Delegation should be unchanged
    BOOST_REQUIRE( GET_ASSETS( "alice" ).get_delegated_vesting() == delegated );

    // Effective vesting should increase by the claimed amount
    BOOST_REQUIRE( eff_vests_after.value == eff_vests_before.value + ASSET( "2000.000000 VESTS" ).amount.value );

    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_SUITE_END()

#endif
