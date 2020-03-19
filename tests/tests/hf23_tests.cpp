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
      ACTORS( (alice)(bob) )
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
      tx.operations.clear();
      tx.signatures.clear();
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

BOOST_AUTO_TEST_SUITE_END()
