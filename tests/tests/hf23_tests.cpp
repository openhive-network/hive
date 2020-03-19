#include <boost/test/unit_test.hpp>

#include <steem/chain/steem_fwd.hpp>

#include <steem/protocol/exceptions.hpp>
#include <steem/protocol/hardfork.hpp>
#include <steem/protocol/sps_operations.hpp>

#include <steem/chain/database.hpp>
#include <steem/chain/database_exceptions.hpp>
#include <steem/chain/steem_objects.hpp>

#include <steem/chain/util/reward.hpp>

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

namespace
{

std::string asset_to_string( const asset& a )
{
   return steem::plugins::condenser_api::legacy_asset::from_asset( a ).to_string();
}

} // namespace


BOOST_FIXTURE_TEST_SUITE( hf23_tests, hf23_database_fixture )

BOOST_AUTO_TEST_CASE( basic_test_06 )
{
   try
   {
      BOOST_TEST_MESSAGE( "VEST delegations - object of type `vesting_delegation_expiration_object` is generated" );

      ACTORS( (alice)(bob) )
      generate_block();

      set_price_feed( price( ASSET( "1.000 TBD" ), ASSET( "1.000 TESTS" ) ) );
      generate_block();

      auto _10 = ASSET( "10.000 TESTS" );
      auto _20 = ASSET( "20.000 TESTS" );
      auto _1v = ASSET( "1.000000 VESTS" );
      auto _2v = ASSET( "2.000000 VESTS" );
      auto _3v = ASSET( "3.000000 VESTS" );
      auto _1000 = ASSET( "1000.000 TESTS" );

      FUND( "alice", _1000 );
      FUND( "bob", _1000 );

      {
         vest( "alice", "alice", _10, alice_private_key );
         vest( "bob", "bob", _20, bob_private_key );
      }
      {
         delegate_vest( "alice", "bob", _3v, alice_private_key );
         generate_block();

         const auto& idx = db->get_index< vesting_delegation_expiration_index, by_account_expiration >();
         BOOST_REQUIRE( idx.lower_bound( "alice" ) == idx.end() );
         BOOST_REQUIRE( idx.lower_bound( "bob" ) == idx.end() );
      }
      {
         delegate_vest( "alice", "bob", _3v, alice_private_key );

         const auto& idx = db->get_index< vesting_delegation_expiration_index, by_account_expiration >();
         BOOST_REQUIRE( idx.lower_bound( "alice" ) == idx.end() );
         BOOST_REQUIRE( idx.lower_bound( "bob" ) == idx.end() );
      }
      {
         delegate_vest( "alice", "bob", _2v, alice_private_key );

         const auto& idx = db->get_index< vesting_delegation_expiration_index, by_account_expiration >();
         BOOST_REQUIRE( idx.lower_bound( "alice" ) != idx.end() );
         BOOST_REQUIRE( idx.lower_bound( "bob" ) == idx.end() );
      }
      {
         delegate_vest( "alice", "bob", _1v, alice_private_key );

         const auto& idx = db->get_index< vesting_delegation_expiration_index, by_account_expiration >();
         BOOST_REQUIRE( idx.lower_bound( "alice" ) != idx.end() );
         BOOST_REQUIRE( idx.lower_bound( "bob" ) == idx.end() );
      }
      {
         db->clear_account( db->get_account( "bob" ) );

         const auto& idx = db->get_index< vesting_delegation_expiration_index, by_account_expiration >();
         BOOST_REQUIRE( idx.lower_bound( "alice" ) != idx.end() );
         BOOST_REQUIRE( idx.lower_bound( "bob" ) == idx.end() );
      }
      {
         db->clear_account( db->get_account( "alice" ) );

         const auto& idx = db->get_index< vesting_delegation_expiration_index, by_account_expiration >();
         BOOST_REQUIRE( idx.lower_bound( "alice" ) == idx.end() );
         BOOST_REQUIRE( idx.lower_bound( "bob" ) == idx.end() );
      }

      database_fixture::validate_database();
   }
   FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( basic_test_05 )
{
   try
   {
      BOOST_TEST_MESSAGE( "VEST delegations - more complex scenarios" );

      ACTORS( (alice)(bob)(carol) )
      generate_block();

      set_price_feed( price( ASSET( "1.000 TBD" ), ASSET( "1.000 TESTS" ) ) );
      generate_block();

      auto _10 = ASSET( "10.000 TESTS" );
      auto _20 = ASSET( "20.000 TESTS" );
      auto _1v = ASSET( "1.000000 VESTS" );
      auto _2v = ASSET( "2.000000 VESTS" );
      auto _3v = ASSET( "3.000000 VESTS" );
      auto _1000 = ASSET( "1000.000 TESTS" );

      FUND( "alice", _1000 );
      FUND( "bob", _1000 );
      FUND( "carol", _1000 );

      {
         vest( "alice", "alice", _10, alice_private_key );
         vest( "bob", "bob", _20, bob_private_key );
         vest( "carol", "carol", _10, carol_private_key );
      }
      {
         delegate_vest( "alice", "bob", _1v, alice_private_key );
         delegate_vest( "alice", "bob", _2v, alice_private_key );

         BOOST_REQUIRE( db->get_account( "alice" ).delegated_vesting_shares.amount.value == _2v.amount.value );

         BOOST_REQUIRE( db->get_account( "bob" ).received_vesting_shares.amount.value == _2v.amount.value );
      }
      {
         delegate_vest( "alice", "carol", _1v, alice_private_key );

         BOOST_REQUIRE( db->get_account( "alice" ).delegated_vesting_shares.amount.value == _3v.amount.value );

         BOOST_REQUIRE( db->get_account( "bob" ).received_vesting_shares.amount.value == _2v.amount.value );
         BOOST_REQUIRE( db->get_account( "carol" ).received_vesting_shares.amount.value == _1v.amount.value );
      }
      {
         delegate_vest( "carol", "bob", _1v, carol_private_key );

         BOOST_REQUIRE( db->get_account( "alice" ).delegated_vesting_shares.amount.value == _3v.amount.value );
         BOOST_REQUIRE( db->get_account( "carol" ).delegated_vesting_shares.amount.value == _1v.amount.value );

         BOOST_REQUIRE( db->get_account( "bob" ).received_vesting_shares.amount.value == _3v.amount.value );
         BOOST_REQUIRE( db->get_account( "carol" ).received_vesting_shares.amount.value == _1v.amount.value );
      }
      {
         db->clear_account( db->get_account( "carol" ) );

         BOOST_REQUIRE( db->get_account( "alice" ).delegated_vesting_shares.amount.value == _3v.amount.value );
         BOOST_REQUIRE( db->get_account( "carol" ).delegated_vesting_shares.amount.value == 0l );

         BOOST_REQUIRE( db->get_account( "bob" ).received_vesting_shares.amount.value == _2v.amount.value );
         BOOST_REQUIRE( db->get_account( "carol" ).received_vesting_shares.amount.value == _1v.amount.value );
      }
      {
         db->clear_account( db->get_account( "alice" ) );

         BOOST_REQUIRE( db->get_account( "alice" ).delegated_vesting_shares.amount.value == 0l );
         BOOST_REQUIRE( db->get_account( "bob" ).delegated_vesting_shares.amount.value == 0l );
         BOOST_REQUIRE( db->get_account( "carol" ).delegated_vesting_shares.amount.value == 0l );

         BOOST_REQUIRE( db->get_account( "alice" ).received_vesting_shares.amount.value == 0l );
         BOOST_REQUIRE( db->get_account( "bob" ).received_vesting_shares.amount.value == 0l );
         BOOST_REQUIRE( db->get_account( "carol" ).received_vesting_shares.amount.value == 0l );
      }

      database_fixture::validate_database();
   }
   FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( basic_test_04 )
{
   try
   {
      BOOST_TEST_MESSAGE( "VEST delegations" );

      ACTORS( (alice)(bob)(carol) )
      generate_block();

      set_price_feed( price( ASSET( "1.000 TBD" ), ASSET( "1.000 TESTS" ) ) );
      generate_block();

      auto _10 = ASSET( "10.000 TESTS" );
      auto _20 = ASSET( "20.000 TESTS" );
      auto _1v = ASSET( "1.000000 VESTS" );
      auto _2v = ASSET( "2.000000 VESTS" );
      auto _1000 = ASSET( "1000.000 TESTS" );

      FUND( "alice", _1000 );
      FUND( "bob", _1000 );
      FUND( "carol", _1000 );

      {
         vest( "alice", "alice", _10, alice_private_key );
         vest( "alice", "alice", _20, alice_private_key );

         delegate_vest( "alice", "bob", _1v, alice_private_key );
         BOOST_REQUIRE( db->get_account( "alice" ).delegated_vesting_shares.amount.value != 0l );
         BOOST_REQUIRE( db->get_account( "bob" ).delegated_vesting_shares.amount.value == 0l );
         BOOST_REQUIRE( db->get_account( "carol" ).delegated_vesting_shares.amount.value == 0l );

         BOOST_REQUIRE( db->get_account( "alice" ).received_vesting_shares.amount.value == 0l );
         BOOST_REQUIRE( db->get_account( "bob" ).received_vesting_shares.amount.value != 0l );
         BOOST_REQUIRE( db->get_account( "carol" ).received_vesting_shares.amount.value == 0l );

         auto previous = db->get_account( "alice" ).delegated_vesting_shares.amount.value;

         delegate_vest( "alice", "carol", _2v, alice_private_key );

         auto next = db->get_account( "alice" ).delegated_vesting_shares.amount.value;
         BOOST_REQUIRE( next > previous );

         BOOST_REQUIRE( db->get_account( "alice" ).delegated_vesting_shares.amount.value != 0l );
         BOOST_REQUIRE( db->get_account( "bob" ).delegated_vesting_shares.amount.value == 0l );
         BOOST_REQUIRE( db->get_account( "carol" ).delegated_vesting_shares.amount.value == 0l );

         BOOST_REQUIRE( db->get_account( "alice" ).received_vesting_shares.amount.value == 0l );
         BOOST_REQUIRE( db->get_account( "bob" ).received_vesting_shares.amount.value != 0l );
         BOOST_REQUIRE( db->get_account( "carol" ).received_vesting_shares.amount.value != 0l );
      }
      {
         db->clear_account( db->get_account( "alice" ) );
         BOOST_REQUIRE( db->get_account( "alice" ).delegated_vesting_shares.amount.value == 0l );
         BOOST_REQUIRE( db->get_account( "bob" ).delegated_vesting_shares.amount.value == 0l );
         BOOST_REQUIRE( db->get_account( "carol" ).delegated_vesting_shares.amount.value == 0l );

         BOOST_REQUIRE( db->get_account( "alice" ).received_vesting_shares.amount.value == 0l );
         BOOST_REQUIRE( db->get_account( "bob" ).received_vesting_shares.amount.value == 0l );
         BOOST_REQUIRE( db->get_account( "carol" ).received_vesting_shares.amount.value == 0l );

         db->clear_account( db->get_account( "bob" ) );
         BOOST_REQUIRE( db->get_account( "alice" ).delegated_vesting_shares.amount.value == 0l );
         BOOST_REQUIRE( db->get_account( "bob" ).delegated_vesting_shares.amount.value == 0l );
         BOOST_REQUIRE( db->get_account( "carol" ).delegated_vesting_shares.amount.value == 0l );

         BOOST_REQUIRE( db->get_account( "alice" ).received_vesting_shares.amount.value == 0l );
         BOOST_REQUIRE( db->get_account( "bob" ).received_vesting_shares.amount.value == 0l );
         BOOST_REQUIRE( db->get_account( "carol" ).received_vesting_shares.amount.value == 0l );
      }

      database_fixture::validate_database();
   }
   FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( basic_test_03 )
{
   try
   {
      BOOST_TEST_MESSAGE( "Vesting every account to anothers accounts" );

      ACTORS( (alice)(bob)(carol) )
      generate_block();

      set_price_feed( price( ASSET( "1.000 TBD" ), ASSET( "1.000 TESTS" ) ) );
      generate_block();

      auto _1 = ASSET( "1.000 TESTS" );
      auto _2 = ASSET( "2.000 TESTS" );
      auto _3 = ASSET( "3.000 TESTS" );
      auto _1000 = ASSET( "1000.000 TESTS" );

      FUND( "alice", _1000 );
      FUND( "bob", _1000 );
      FUND( "carol", _1000 );

      BOOST_REQUIRE( db->get_account( "alice" ).vesting_shares == db->get_account( "bob" ).vesting_shares );
      BOOST_REQUIRE( db->get_account( "bob" ).vesting_shares == db->get_account( "carol" ).vesting_shares );

      {
         vest( "alice", "bob", _1, alice_private_key );
         vest( "bob", "carol", _2, bob_private_key );
         vest( "carol", "alice", _3, carol_private_key );
         BOOST_REQUIRE_GT( db->get_account( "alice" ).vesting_shares.amount.value, db->get_account( "carol" ).vesting_shares.amount.value );
         BOOST_REQUIRE_GT( db->get_account( "carol" ).vesting_shares.amount.value, db->get_account( "bob" ).vesting_shares.amount.value );
      }
      {
         auto vest_bob = db->get_account( "bob" ).vesting_shares.amount.value;
         auto vest_carol = db->get_account( "carol" ).vesting_shares.amount.value;

         db->clear_account( db->get_account( "alice" ) );
         BOOST_REQUIRE( db->get_account( "alice" ).vesting_shares.amount.value == 0l );
         BOOST_REQUIRE( db->get_account( "bob" ).vesting_shares.amount.value == vest_bob );
         BOOST_REQUIRE( db->get_account( "carol" ).vesting_shares.amount.value == vest_carol );

         db->clear_account( db->get_account( "bob" ) );
         BOOST_REQUIRE( db->get_account( "alice" ).vesting_shares.amount.value == 0l );
         BOOST_REQUIRE( db->get_account( "bob" ).vesting_shares.amount.value == 0l );
         BOOST_REQUIRE( db->get_account( "carol" ).vesting_shares.amount.value == vest_carol );

         db->clear_account( db->get_account( "carol" ) );
         BOOST_REQUIRE( db->get_account( "alice" ).vesting_shares.amount.value == 0l );
         BOOST_REQUIRE( db->get_account( "bob" ).vesting_shares.amount.value == 0l );
         BOOST_REQUIRE( db->get_account( "carol" ).vesting_shares.amount.value == 0l );
      }

      database_fixture::validate_database();
   }
   FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( basic_test_02 )
{
   try
   {
      BOOST_TEST_MESSAGE( "Vesting" );

      ACTORS( (alice)(bob) )
      generate_block();

      set_price_feed( price( ASSET( "1.000 TBD" ), ASSET( "1.000 TESTS" ) ) );
      generate_block();

      auto _1 = ASSET( "1.000 TESTS" );
      auto _1000 = ASSET( "1000.000 TESTS" );

      FUND( "alice", _1000 );
      FUND( "bob", _1000 );

      BOOST_REQUIRE( db->get_account( "alice" ).vesting_shares == db->get_account( "bob" ).vesting_shares );

      {
         vest( "alice", "alice", _1, alice_private_key );
         BOOST_REQUIRE_GT( db->get_account( "alice" ).vesting_shares.amount.value, db->get_account( "bob" ).vesting_shares.amount.value );
      }
      {
         auto vest_bob = db->get_account( "bob" ).vesting_shares.amount.value;

         db->clear_account( db->get_account( "alice" ) );
         BOOST_REQUIRE( db->get_account( "alice" ).vesting_shares.amount.value == 0l );
         BOOST_REQUIRE( db->get_account( "bob" ).vesting_shares.amount.value == vest_bob );

         db->clear_account( db->get_account( "bob" ) );
         BOOST_REQUIRE( db->get_account( "alice" ).vesting_shares.amount.value == 0l );
         BOOST_REQUIRE( db->get_account( "bob" ).vesting_shares.amount.value == 0l );
      }

      database_fixture::validate_database();
   }
   FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( basic_test_01 )
{
   try
   {
      BOOST_TEST_MESSAGE( "Calling clear_account" );

      ACTORS( (alice) )
      generate_block();

      auto& _alice = db->get_account( "alice" );
      db->clear_account( _alice );

      /*
         Original `clean_database_fixture::validate_database` checks `rc_plugin` as well.
         Is it needed?
      */
      database_fixture::validate_database();
   }
   FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( sbd_test_01 )
{
   try
   {
      ACTORS( (alice) )
      generate_block();

      set_price_feed( price( ASSET( "1.000 TBD" ), ASSET( "1.000 TESTS" ) ) );
      generate_block();

      BOOST_REQUIRE( db->get_account( "alice" ).sbd_balance == ASSET( "0.000 TBD" ) );
      db->clear_account( db->get_account( "alice" ) );
      BOOST_REQUIRE( db->get_account( "alice" ).sbd_balance == ASSET( "0.000 TBD" ) );
      database_fixture::validate_database();

      fund( "alice", ASSET( "1000.000 TBD" ) );
      auto alice_sbd = db->get_account( "alice" ).sbd_balance;
      BOOST_REQUIRE( alice_sbd == ASSET( "1000.000 TBD" ) );
      db->clear_account( db->get_account( "alice" ) );
      BOOST_REQUIRE( db->get_account( "alice" ).sbd_balance == ASSET( "0.000 TBD" ) );
      database_fixture::validate_database();
   }
   FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( sbd_test_02 )
{
   try
   {
      ACTORS( (alice)(bob) )
      generate_block();

      set_price_feed( price( ASSET( "1.000 TBD" ), ASSET( "1.000 TESTS" ) ) );
      generate_block();

      BOOST_REQUIRE( db->get_account( "alice" ).sbd_balance == ASSET( "0.000 TBD" ) );
      fund( "alice", ASSET( "1000.000 TBD" ) );
      auto start_time = db->get_account( "alice" ).sbd_seconds_last_update;
      auto alice_sbd = db->get_account( "alice" ).sbd_balance;
      BOOST_TEST_MESSAGE( "treasury_sbd = " << asset_to_string( db->get_account( STEEM_TREASURY_ACCOUNT ).sbd_balance ) );
      BOOST_TEST_MESSAGE( "alice_sbd = " << asset_to_string( alice_sbd ) );
      BOOST_REQUIRE( alice_sbd == ASSET( "1000.000 TBD" ) );

      generate_blocks( db->head_block_time() + fc::seconds( STEEM_SBD_INTEREST_COMPOUND_INTERVAL_SEC ), true );

      {
      signed_transaction tx;
      transfer_operation transfer;
      transfer.to = "bob";
      transfer.from = "alice";
      transfer.amount = ASSET( "1.000 TBD" );
      tx.set_expiration( db->head_block_time() + STEEM_MAX_TIME_UNTIL_EXPIRATION );
      tx.operations.push_back( transfer );
      sign( tx, alice_private_key );
      db->push_transaction( tx, 0 );
      }

      auto& gpo = db->get_dynamic_global_properties();
      auto interest_op = get_last_operations( 1 )[0].get< interest_operation >();

      BOOST_REQUIRE( gpo.sbd_interest_rate > 0 );
      BOOST_REQUIRE( static_cast<uint64_t>(db->get_account( "alice" ).sbd_balance.amount.value) ==
         alice_sbd.amount.value - ASSET( "1.000 TBD" ).amount.value +
         ( ( ( ( uint128_t( alice_sbd.amount.value ) * ( db->head_block_time() - start_time ).to_seconds() ) / STEEM_SECONDS_PER_YEAR ) *
            gpo.sbd_interest_rate ) / STEEM_100_PERCENT ).to_uint64() );
      BOOST_REQUIRE( interest_op.owner == "alice" );
      BOOST_REQUIRE( interest_op.interest.amount.value ==
         db->get_account( "alice" ).sbd_balance.amount.value - ( alice_sbd.amount.value - ASSET( "1.000 TBD" ).amount.value ) );
      database_fixture::validate_database();

      generate_blocks( db->head_block_time() + fc::seconds( STEEM_SBD_INTEREST_COMPOUND_INTERVAL_SEC ), true );

      alice_sbd = db->get_account( "alice" ).sbd_balance;
      BOOST_TEST_MESSAGE( "alice_sbd = " << asset_to_string( alice_sbd ) );
      BOOST_TEST_MESSAGE( "bob_sbd = " << asset_to_string( db->get_account( "bob" ).sbd_balance ) );

      db->clear_account( db->get_account( "alice" ) );
      BOOST_REQUIRE( db->get_account( "alice" ).sbd_balance == ASSET( "0.000 TBD" ) );
      database_fixture::validate_database();
   }
   FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( savings_test_01 )
{
   try
   {
      ACTORS( (alice) )
      generate_block();

      set_price_feed( price( ASSET( "1.000 TBD" ), ASSET( "1.000 TESTS" ) ) );
      generate_block();

      fund( "alice", ASSET( "1000.000 TBD" ) );
      
      signed_transaction tx;
      transfer_to_savings_operation op;
      op.from = "alice";
      op.to = "alice";
      op.amount = ASSET( "1000.000 TBD" );
      tx.set_expiration( db->head_block_time() + STEEM_MAX_TIME_UNTIL_EXPIRATION );
      tx.operations.push_back( op );
      sign( tx, alice_private_key );
      db->push_transaction( tx, 0 );

      BOOST_REQUIRE( db->get_account( "alice" ).savings_sbd_balance == ASSET( "1000.000 TBD" ) );
      db->clear_account( db->get_account( "alice" ) );
      BOOST_REQUIRE( db->get_account( "alice" ).savings_sbd_balance == ASSET( "0.000 TBD" ) );
      database_fixture::validate_database();
   }
   FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( savings_test_02 )
{
   try
   {
      ACTORS( (alice)(bob) )
      generate_block();

      set_price_feed( price( ASSET( "1.000 TBD" ), ASSET( "1.000 TESTS" ) ) );
      generate_block();

      fund( "alice", ASSET( "1000.000 TBD" ) );

      signed_transaction tx;
      transfer_to_savings_operation op;
      op.from = "alice";
      op.to = "alice";
      op.amount = ASSET( "1000.000 TBD" );
      tx.set_expiration( db->head_block_time() + STEEM_MAX_TIME_UNTIL_EXPIRATION );
      tx.operations.push_back( op );
      sign( tx, alice_private_key );
      db->push_transaction( tx, 0 );

      BOOST_REQUIRE( db->get_account( "alice" ).savings_sbd_balance == ASSET( "1000.000 TBD" ) );

      generate_blocks( db->head_block_time() + fc::seconds( STEEM_SBD_INTEREST_COMPOUND_INTERVAL_SEC ), true );

      BOOST_TEST_MESSAGE( "alice_savings_sbd before clear = " << asset_to_string( db->get_account( "alice" ).savings_sbd_balance ) ); // alice_savings_sbd before clear = 1000.000 TBD
      db->clear_account( db->get_account( "alice" ) );
      BOOST_TEST_MESSAGE( "alice_savings_sbd after clear = " << asset_to_string( db->get_account( "alice" ).savings_sbd_balance ) ); // alice_savings_sbd after clear = 8.219 TBD
      BOOST_REQUIRE( db->get_account( "alice" ).savings_sbd_balance == ASSET( "0.000 TBD" ) );
      database_fixture::validate_database();
   }
   FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_SUITE_END()
