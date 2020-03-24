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

BOOST_FIXTURE_TEST_SUITE( hf23_tests, hf23_database_fixture )

BOOST_AUTO_TEST_CASE( dump_test_03 )
{
   try
   {
      BOOST_TEST_MESSAGE( "Creating incorrect json file" );

      std::string content_00 = R"(
                                 [{
                                    "name": "alice",
                                    "balance": {
                                       "amount": "1000000",
                                       "precision": 3,
                                       "nai": "@@000000021"
                                    }
                                 }]
                                 )";

      std::string content_01 = R"([{
                                    "name": "alice",
                                    "balance": {
                                       "amount": "1000000",
                                       "precision": 3,
                                       "nai": "@@000000021"
                                    },
                                    "savings_balancexxx": {
                                       "amount": "0",
                                       "precision": 3,
                                       "nai": "@@000000021"
                                    },
                                    "sbd_balance": {
                                       "amount": "20000",
                                       "precision": 3,
                                       "nai": "@@000000013"
                                    },
                                    "savings_sbd_balance": {
                                       "amount": "0",
                                       "precision": 3,
                                       "nai": "@@000000013"
                                    },
                                    "reward_sbd_balance": {
                                       "amount": "0",
                                       "precision": 3,
                                       "nai": "@@000000013"
                                    },
                                    "reward_steem_balance": {
                                       "amount": "0",
                                       "precision": 3,
                                       "nai": "@@000000021"
                                    }
                                 }])";

      std::string content_02 = R"([{
                                    "reward_sbd_balance": {
                                       "amount": "0",
                                       "precision": 3,
                                       "nai": "@@000000013"
                                    },
                                    "reward_steem_balance": {
                                       "amount": "0",
                                       "precision": 3,
                                       "nai": "@@000000021"
                                    }
                                 }])";

      std::string content_03 = R"({ "test":{
                                    "name": "alice",
                                    "balance": {
                                       "amount": "1000000",
                                       "precision": 3,
                                       "nai": "@@000000021"
                                    },
                                    "savings_balance": {
                                       "amount": "0",
                                       "precision": 3,
                                       "nai": "@@000000021"
                                    },
                                    "sbd_balance": {
                                       "amount": "20000",
                                       "precision": 3,
                                       "nai": "@@000000013"
                                    },
                                    "savings_sbd_balance": {
                                       "amount": "0",
                                       "precision": 3,
                                       "nai": "@@000000013"
                                    },
                                    "reward_sbd_balance": {
                                       "amount": "0",
                                       "precision": 3,
                                       "nai": "@@000000013"
                                    },
                                    "reward_steem_balance": {
                                       "amount": "0",
                                       "precision": 3,
                                       "nai": "@@000000021"
                                    }
                                 }
                                 })";

      std::string content_04 = R"([ ])";

      std::string content_05 = R"([ {} ])";

      {

         std::vector< std::string > contents{ content_00, content_01, content_02, content_03, content_04, content_05 };

         const std::string path ="balances03.json";
         fc::path _path( path.c_str() );

         auto save_content = [ &_path ]( const std::string& content )
         {
            if( fc::exists( _path ) )
               fc::remove( _path );

            std::ofstream file( _path.string() );
            file << content;
            file.close();
         };

         for( auto& item : contents )
         {
            save_content( item );

            hf23_helper::hf23_items balances = hf23_helper::restore_balances( path );
            BOOST_REQUIRE_EQUAL( balances.size(), 0u );
         }
      }
   }
   FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( dump_test_02 )
{
   try
   {
      BOOST_TEST_MESSAGE( "Dumping and restoring one account balances" );

      ACTORS( (alice) )
      generate_block();

      set_price_feed( price( ASSET( "1.000 TBD" ), ASSET( "1.000 TESTS" ) ) );
      generate_block();

      FUND( "alice", ASSET( "1000.000 TESTS" ) );
      FUND( "alice", ASSET( "20.000 TBD" ) );

      const std::string path ="balances02.json";

      fc::path _path( path.c_str() );
      if( fc::exists( _path ) )
         fc::remove( _path );

      const std::set< std::string > accounts{ "alice", "bob" };

      {
         hf23_helper::dump_balances( *db, path, std::set< std::string >() );
         hf23_helper::hf23_items balances = hf23_helper::restore_balances( path );
         BOOST_REQUIRE_EQUAL( balances.size(), 0u );
      }
      {
         hf23_helper::dump_balances( *db, path, accounts );
      }
      {
         hf23_helper::hf23_items balances = hf23_helper::restore_balances( path );
         BOOST_REQUIRE_EQUAL( balances.size(), 1u );

         auto alice_balances = hf23_helper::hf23_item::get_balances( db->get_account( "alice" ) );

         hf23_helper::hf23_items cmp_balances = { alice_balances };

         BOOST_REQUIRE_EQUAL( balances.size(), cmp_balances.size() );
         BOOST_REQUIRE( balances[0] == cmp_balances[0] );
      }

      database_fixture::validate_database();
   }
   FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( dump_test_01 )
{
   try
   {
      BOOST_TEST_MESSAGE( "Dumping and restoring accounts balances" );

      ACTORS( (alice)(bob) )
      generate_block();

      set_price_feed( price( ASSET( "1.000 TBD" ), ASSET( "1.000 TESTS" ) ) );
      generate_block();

      FUND( "alice", ASSET( "1000.000 TESTS" ) );
      FUND( "bob", ASSET( "2000.000 TESTS" ) );

      FUND( "alice", ASSET( "20.000 TBD" ) );

      const std::string path ="balances01.json";
      const std::set< std::string > accounts{ "alice", "bob" };

      {
         vest( "alice", "alice", ASSET( "10.000 TESTS" ), alice_private_key );
         vest( "bob", "bob", ASSET( "10.000 TESTS" ), bob_private_key );

         hf23_helper::dump_balances( *db, path, accounts );
      }
      {
         hf23_helper::hf23_items balances = hf23_helper::restore_balances( path );

         auto alice_balances = hf23_helper::hf23_item::get_balances( db->get_account( "alice" ) );
         auto bob_balances = hf23_helper::hf23_item::get_balances( db->get_account( "bob" ) );

         hf23_helper::hf23_items cmp_balances = { alice_balances, bob_balances };

         BOOST_REQUIRE_EQUAL( balances.size(), cmp_balances.size() );

         for( uint32_t i = 0; i < cmp_balances.size(); ++i )
         {
            BOOST_REQUIRE( balances[i] == cmp_balances[i] );
         }
      }

      database_fixture::validate_database();
   }
   FC_LOG_AND_RETHROW()
}

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

BOOST_AUTO_TEST_SUITE_END()
