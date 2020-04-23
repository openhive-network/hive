#ifdef IS_TEST_NET

#include <boost/test/unit_test.hpp>

#include <steem/chain/steem_fwd.hpp>

#include <steem/protocol/exceptions.hpp>
#include <steem/protocol/hardfork.hpp>
#include <steem/protocol/sps_operations.hpp>

#include <steem/chain/database.hpp>
#include <steem/chain/database_exceptions.hpp>
#include <steem/chain/steem_objects.hpp>

#include <steem/chain/util/reward.hpp>
#include <steem/chain/util/hf23_helper.hpp>

#include <steem/plugins/rc/rc_objects.hpp>
#include <steem/plugins/rc/resource_count.hpp>

#include <fc/macros.hpp>
#include <fc/crypto/digest.hpp>

#include "../db_fixture/database_fixture.hpp"

#include <cmath>
#include <iostream>
#include <stdexcept>

using namespace steem;
using namespace steem::chain;
using namespace steem::protocol;
using fc::string;


BOOST_FIXTURE_TEST_SUITE( hf24_tests, hf24_database_fixture )

BOOST_AUTO_TEST_CASE( blocked_operations )
{
   try
   {
      BOOST_TEST_MESSAGE( "Even after HF24 steem.dao is considered a treasury account" );

      ACTORS( ( alice ) )
      generate_block();
      fund( "alice", ASSET( "10.000 TESTS" ) );
      fund( "alice", ASSET( "10.000 TBD" ) );
      generate_block();

      signed_transaction tx;
      tx.set_expiration( db->head_block_time() + STEEM_MAX_TIME_UNTIL_EXPIRATION );

      //check transfer of non-HBD
      {
         transfer_operation op;
         op.from = "alice";
         op.to = OLD_STEEM_TREASURY_ACCOUNT;
         op.amount = ASSET( "1.000 TESTS" );
         tx.operations.push_back( op );
         sign( tx, alice_private_key );
         BOOST_REQUIRE_THROW( db->push_transaction( tx, 0 ), fc::assert_exception ); //still blocked even though old is no longer active treasury

         tx.clear();
         op.to = NEW_HIVE_TREASURY_ACCOUNT;
         tx.operations.push_back( op );
         sign( tx, alice_private_key );
         BOOST_REQUIRE_THROW( db->push_transaction( tx, 0 ), fc::assert_exception ); //blocked for new treasury as well
      }
      tx.clear();

      //check vesting of non-HBD
      {
         transfer_to_vesting_operation op;
         op.from = "alice";
         op.to = OLD_STEEM_TREASURY_ACCOUNT;
         op.amount = ASSET( "1.000 TESTS" );
         tx.operations.push_back( op );
         sign( tx, alice_private_key );
         BOOST_REQUIRE_THROW( db->push_transaction( tx, 0 ), fc::assert_exception );

         tx.clear();
         op.to = NEW_HIVE_TREASURY_ACCOUNT;
         tx.operations.push_back( op );
         sign( tx, alice_private_key );
         BOOST_REQUIRE_THROW( db->push_transaction( tx, 0 ), fc::assert_exception );
      }
      tx.clear();

      //check withdraw vesting route
      {
         set_withdraw_vesting_route_operation op;
         op.from_account = "alice";
         op.to_account = OLD_STEEM_TREASURY_ACCOUNT;
         op.percent = 50 * STEEM_1_PERCENT;
         tx.operations.push_back( op );
         sign( tx, alice_private_key );
         BOOST_REQUIRE_THROW( db->push_transaction( tx, 0 ), fc::assert_exception );

         tx.clear();
         op.to_account = NEW_HIVE_TREASURY_ACCOUNT;
         tx.operations.push_back( op );
         sign( tx, alice_private_key );
         BOOST_REQUIRE_THROW( db->push_transaction( tx, 0 ), fc::assert_exception );
      }
      tx.clear();

      //check transfer to savings
      {
         transfer_to_savings_operation op;
         op.from = "alice";
         op.to = OLD_STEEM_TREASURY_ACCOUNT;
         op.amount = ASSET( "1.000 TESTS" );
         tx.operations.push_back( op );
         sign( tx, alice_private_key );
         BOOST_REQUIRE_THROW( db->push_transaction( tx, 0 ), fc::assert_exception );

         tx.clear();
         op.amount = ASSET( "1.000 TBD" );
         tx.operations.push_back( op );
         sign( tx, alice_private_key );
         BOOST_REQUIRE_THROW( db->push_transaction( tx, 0 ), fc::assert_exception );

         tx.clear();
         op.to = NEW_HIVE_TREASURY_ACCOUNT;
         tx.operations.push_back( op );
         sign( tx, alice_private_key );
         BOOST_REQUIRE_THROW( db->push_transaction( tx, 0 ), fc::assert_exception );

         tx.clear();
         op.amount = ASSET( "1.000 TESTS" );
         tx.operations.push_back( op );
         sign( tx, alice_private_key );
         BOOST_REQUIRE_THROW( db->push_transaction( tx, 0 ), fc::assert_exception );

         tx.clear();
         op.to = "alice";
         tx.operations.push_back( op );
         sign( tx, alice_private_key );
         db->push_transaction( tx, 0 );
         BOOST_REQUIRE( db->get_account( "alice" ).savings_balance == ASSET( "1.000 TESTS" ) );
      }
      tx.clear();

      //check transfer from savings of non-HBD
      {
         transfer_from_savings_operation op;
         op.from = "alice";
         op.to = OLD_STEEM_TREASURY_ACCOUNT;
         op.amount = ASSET( "1.000 TESTS" );
         tx.operations.push_back( op );
         sign( tx, alice_private_key );
         BOOST_REQUIRE_THROW( db->push_transaction( tx, 0 ), fc::assert_exception );

         tx.clear();
         op.to = NEW_HIVE_TREASURY_ACCOUNT;
         tx.operations.push_back( op );
         sign( tx, alice_private_key );
         BOOST_REQUIRE_THROW( db->push_transaction( tx, 0 ), fc::assert_exception );
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
            gpo.sps_fund_percent = 0;
         } );
      } );
      fund( "alice", ASSET( "10.000 TESTS" ) );
      fund( "alice", ASSET( "10.000 TBD" ) );
      generate_block();

      signed_transaction tx;
      tx.set_expiration( db->head_block_time() + STEEM_MAX_TIME_UNTIL_EXPIRATION );

      {
         comment_operation comment;
         comment.author = "alice";
         comment.permlink = "test";
         comment.parent_permlink = "test";
         comment.title = "test";
         comment.body = "Hello world";
         tx.operations.push_back( comment );
         sign( tx, alice_private_key );
         db->push_transaction( tx, 0 );
      }
      tx.clear();

      {
         comment_options_operation op;
         comment_payout_beneficiaries b;
         b.beneficiaries.push_back( beneficiary_route_type( OLD_STEEM_TREASURY_ACCOUNT, STEEM_100_PERCENT ) );
         op.author = "alice";
         op.permlink = "test";
         op.allow_curation_rewards = false;
         op.extensions.insert( b );
         tx.operations.push_back( op );
         sign( tx, alice_private_key );
         db->push_transaction( tx, 0 );
      }
      tx.clear();

      {
         vote_operation vote;
         vote.author = "alice";
         vote.permlink = "test";
         vote.voter = "alice";
         vote.weight = STEEM_100_PERCENT;
         tx.operations.push_back( vote );
         sign( tx, alice_private_key );
         db->push_transaction( tx, 0 );
      }
      tx.clear();

      asset initial_treasury_balance = db->get_treasury().sbd_balance;
      generate_blocks( db->get_comment( "alice", string( "test" ) ).cashout_time );
      BOOST_REQUIRE_EQUAL( db->get_account( OLD_STEEM_TREASURY_ACCOUNT ).sbd_balance.amount.value, 0 );
      BOOST_REQUIRE_EQUAL( db->get_treasury().sbd_balance.amount.value, 1150 + initial_treasury_balance.amount.value );

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
      asset vested_3, vested_7;
      db_plugin->debug_update( [&]( database& db )
      {
         auto& dgpo = db.get_dynamic_global_properties();
         db.adjust_supply( ASSET( "20.000 TESTS" ) );
         db.adjust_supply( ASSET( "10.000 TBD" ) );
         vested_3 = ASSET( "3.000 TESTS" ) * dgpo.get_vesting_share_price();
         vested_7 = ASSET( "7.000 TESTS" ) * dgpo.get_vesting_share_price();
         db.modify( dgpo, []( dynamic_global_property_object& gpo )
         {
            gpo.sps_fund_percent = 0;
         } );
         auto& old_treasury = db.get_account( OLD_STEEM_TREASURY_ACCOUNT );
         db.create_vesting( old_treasury, ASSET( "7.000 TESTS" ) );
         db.create_vesting( old_treasury, ASSET( "3.000 TESTS" ), true );
         db.modify( old_treasury, [&]( account_object& t )
         {
            t.balance = ASSET( "5.000 TESTS" );
            t.savings_balance = ASSET( "3.000 TESTS" );
            t.reward_steem_balance = ASSET( "2.000 TESTS" );
            t.sbd_balance = ASSET( "5.000 TBD" );
            t.savings_sbd_balance = ASSET( "3.000 TBD" );
            t.reward_sbd_balance = ASSET( "2.000 TBD" );
         } );
      } );
      database_fixture::validate_database();
      {
         auto& old_treasury = db->get_account( OLD_STEEM_TREASURY_ACCOUNT );
         BOOST_REQUIRE_EQUAL( old_treasury.balance.amount.value, 5000 );
         BOOST_REQUIRE_EQUAL( old_treasury.savings_balance.amount.value, 3000 );
         BOOST_REQUIRE_EQUAL( old_treasury.reward_steem_balance.amount.value, 2000 );
         BOOST_REQUIRE_EQUAL( old_treasury.sbd_balance.amount.value, 5000 );
         BOOST_REQUIRE_EQUAL( old_treasury.savings_sbd_balance.amount.value, 3000 );
         BOOST_REQUIRE_EQUAL( old_treasury.reward_sbd_balance.amount.value, 2000 );
         BOOST_REQUIRE_EQUAL( old_treasury.vesting_shares.amount.value, vested_7.amount.value );
         BOOST_REQUIRE_EQUAL( old_treasury.reward_vesting_balance.amount.value, vested_3.amount.value );
      }

      asset initial_treasury_balance = db->get_treasury().sbd_balance;
      generate_block();
      database_fixture::validate_database();

      {
         auto& old_treasury = db->get_account( OLD_STEEM_TREASURY_ACCOUNT );
         BOOST_REQUIRE_EQUAL( old_treasury.balance.amount.value, 0 );
         BOOST_REQUIRE_EQUAL( old_treasury.savings_balance.amount.value, 0 );
         BOOST_REQUIRE_EQUAL( old_treasury.reward_steem_balance.amount.value, 0 );
         BOOST_REQUIRE_EQUAL( old_treasury.sbd_balance.amount.value, 0 );
         BOOST_REQUIRE_EQUAL( old_treasury.savings_sbd_balance.amount.value, 0 );
         BOOST_REQUIRE_EQUAL( old_treasury.reward_sbd_balance.amount.value, 0 );
         BOOST_REQUIRE_EQUAL( old_treasury.vesting_shares.amount.value, 0 );
         BOOST_REQUIRE_EQUAL( old_treasury.reward_vesting_balance.amount.value, 0 );
      }
      {
         auto& treasury = db->get_account( NEW_HIVE_TREASURY_ACCOUNT );
         BOOST_REQUIRE_EQUAL( treasury.balance.amount.value, 14999 ); //rounding during vest->hive conversion
         BOOST_REQUIRE_EQUAL( treasury.savings_balance.amount.value, 3000 );
         BOOST_REQUIRE_EQUAL( treasury.reward_steem_balance.amount.value, 2000 );
         BOOST_REQUIRE_EQUAL( treasury.sbd_balance.amount.value, 5000 + initial_treasury_balance.amount.value );
         BOOST_REQUIRE_EQUAL( treasury.savings_sbd_balance.amount.value, 3000 );
         BOOST_REQUIRE_EQUAL( treasury.reward_sbd_balance.amount.value, 2000 );
         BOOST_REQUIRE_EQUAL( treasury.vesting_shares.amount.value, 0 );
         BOOST_REQUIRE_EQUAL( treasury.reward_vesting_balance.amount.value, 0 );
      }
   }
   FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_SUITE_END()

#endif
