#if defined(IS_TEST_NET)

#include <boost/test/unit_test.hpp>

#include <steem/chain/steem_fwd.hpp>

#include <steem/protocol/exceptions.hpp>
#include <steem/protocol/hardfork.hpp>

#include <steem/chain/database.hpp>
#include <steem/chain/database_exceptions.hpp>
#include <steem/chain/steem_objects.hpp>

#include <steem/plugins/rc/rc_objects.hpp>
#include <steem/plugins/rc/resource_count.hpp>

#include <steem/chain/util/delayed_voting_processor.hpp>
#include <steem/chain/util/delayed_voting.hpp>

#include <fc/macros.hpp>
#include <fc/crypto/digest.hpp>

#include "../db_fixture/database_fixture.hpp"

#include <cmath>
#include <iostream>
#include <stdexcept>
#include <deque>
#include <array>

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

BOOST_FIXTURE_TEST_SUITE( delayed_voting_tests, delayed_vote_database_fixture )

BOOST_AUTO_TEST_CASE( delayed_voting_01 )
{
   try
   {
      BOOST_TEST_MESSAGE( "Testing: delaying voting" );

      ACTORS( (alice)(bob)(witness) )
      generate_block();

      set_price_feed( price( ASSET( "1.000 TBD" ), ASSET( "1.000 TESTS" ) ) );
      generate_block();

      //auto start_time = db->head_block_time();

      FUND( "alice", ASSET( "10000.000 TESTS" ) );
      FUND( "bob", ASSET( "10000.000 TESTS" ) );
      FUND( "witness", ASSET( "10000.000 TESTS" ) );
      //Prepare witnesses
      witness_create( "witness", witness_private_key, "url.witness", witness_private_key.get_public_key(), STEEM_MIN_PRODUCER_REWARD.amount );
      generate_block();

      auto start_time = db->head_block_time();
      witness_vote( "alice", "witness", true/*approve*/, alice_private_key );
      generate_block();

      int64_t basic_votes = get_votes( "witness" );

      //Make some vests
      vest( "bob", "alice", ASSET( "1000.000 TESTS" ), bob_private_key );
      generate_block();

      int64_t votes_01 = get_votes( "witness" );
      BOOST_REQUIRE_EQUAL( votes_01, basic_votes );

      generate_blocks( start_time + STEEM_DELAYED_VOTING_TOTAL_INTERVAL_SECONDS , true );
      generate_block();

      auto votes_power = db->get_account( "alice" ).vesting_shares;
      int64_t votes_02 = get_votes( "witness" );
      BOOST_REQUIRE_EQUAL( votes_02, basic_votes + votes_power.amount.value );

      validate_database();
   }
   FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( delayed_voting_04 )
{
   try
   {
      BOOST_TEST_MESSAGE( "Testing: delaying voting v4" );

      ACTORS( (bob)(witness) )
      generate_block();

      set_price_feed( price( ASSET( "1.000 TBD" ), ASSET( "1.000 TESTS" ) ) );
      generate_block();

      //auto start_time = db->head_block_time();

      FUND( "bob", ASSET( "10000.000 TESTS" ) );
      FUND( "witness", ASSET( "10000.000 TESTS" ) );
      
      //Prepare witnesses
      witness_create( "witness", witness_private_key, "url.witness", witness_private_key.get_public_key(), STEEM_MIN_PRODUCER_REWARD.amount );
      generate_block();

      auto start_time = db->head_block_time();
      const int64_t basic_votes = get_votes( "witness" );

      //Make some vests
      vest( "bob", "bob", ASSET( "1000.000 TESTS" ), bob_private_key );
      generate_block();

      witness_vote( "bob", "witness", true/*approve*/, bob_private_key );
      generate_block();

      BOOST_REQUIRE_EQUAL( basic_votes, get_votes( "witness" ) );
      for(int i = 1; i < (STEEM_DELAYED_VOTING_TOTAL_INTERVAL_SECONDS / STEEM_DELAYED_VOTING_INTERVAL_SECONDS) - 1; i++)
      {
         generate_blocks( start_time + (i * STEEM_DELAYED_VOTING_INTERVAL_SECONDS) , true );
         BOOST_REQUIRE_EQUAL( get_votes( "witness" ), basic_votes );
      }
      generate_blocks( start_time + STEEM_DELAYED_VOTING_TOTAL_INTERVAL_SECONDS , true );
      generate_block();

      auto votes_power = db->get_account( "bob" ).vesting_shares;
      BOOST_REQUIRE_EQUAL( get_votes( "witness" ), basic_votes + votes_power.amount.value );

      validate_database();
   }
   FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( delayed_voting_05 )
{
   try
   {
      BOOST_TEST_MESSAGE( "Testing: delaying voting v5" );

      ACTORS( (bob)(witness1)(witness2) )
      generate_block();

      set_price_feed( price( ASSET( "1.000 TBD" ), ASSET( "1.000 TESTS" ) ) );
      generate_block();

      //auto start_time = db->head_block_time();

      FUND( "bob", ASSET( "10000.000 TESTS" ) );
      FUND( "witness1", ASSET( "10000.000 TESTS" ) );
      FUND( "witness2", ASSET( "10000.000 TESTS" ) );
      
      //Prepare witnesses
      witness_create( "witness1", witness1_private_key, "url.witness1", witness1_private_key.get_public_key(), STEEM_MIN_PRODUCER_REWARD.amount );
      witness_create( "witness2", witness2_private_key, "url.witness2", witness2_private_key.get_public_key(), STEEM_MIN_PRODUCER_REWARD.amount );
      generate_block();

      auto start_time = db->head_block_time();
      int64_t basic_votes_1 = get_votes( "witness1" );
      int64_t basic_votes_2 = get_votes( "witness2" );

      //Make some vests
      vest( "bob", "bob", ASSET( "1000.000 TESTS" ), bob_private_key );
      generate_block();

      witness_vote( "bob", "witness1", true/*approve*/, bob_private_key );
      generate_block();

      int64_t votes_01_1 = get_votes( "witness1" );
      int64_t votes_01_2 = get_votes( "witness2" );
      BOOST_REQUIRE_EQUAL( basic_votes_1, votes_01_1 );
      BOOST_REQUIRE_EQUAL( basic_votes_2, votes_01_2 );

      constexpr int DAYS{ (STEEM_DELAYED_VOTING_TOTAL_INTERVAL_SECONDS / STEEM_DELAYED_VOTING_INTERVAL_SECONDS) };
      for(int i = 1; i < DAYS - 1; i++)
      {
         generate_blocks( start_time + (i * STEEM_DELAYED_VOTING_INTERVAL_SECONDS) , true );
         if( i == static_cast<int>( DAYS/2 )) witness_vote( "bob", "witness2", true/*approve*/, bob_private_key );
         BOOST_REQUIRE_EQUAL( get_votes( "witness1" ), votes_01_1 );
         BOOST_REQUIRE_EQUAL( get_votes( "witness2" ), votes_01_2 );
      }
      generate_blocks( start_time + STEEM_DELAYED_VOTING_TOTAL_INTERVAL_SECONDS , true );
      generate_block();

      auto votes_power = db->get_account( "bob" ).vesting_shares;
      BOOST_REQUIRE_EQUAL( get_votes( "witness1" ), basic_votes_1 + votes_power.amount.value );
      BOOST_REQUIRE_EQUAL( get_votes( "witness2" ), basic_votes_2 + votes_power.amount.value );

      validate_database();
   }
   FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( delayed_voting_06 )
{
   try
   {
      BOOST_TEST_MESSAGE( "Testing: delaying voting v6" );

      ACTORS( (bob)(witness) )
      generate_block();

      set_price_feed( price( ASSET( "1.000 TBD" ), ASSET( "1.000 TESTS" ) ) );
      generate_block();

      //auto start_time = db->head_block_time();

      FUND( "bob", ASSET( "10000.000 TESTS" ) );
      FUND( "witness", ASSET( "10000.000 TESTS" ) );
      
      //Prepare witnesses
      witness_create( "witness", witness_private_key, "url.witness", witness_private_key.get_public_key(), STEEM_MIN_PRODUCER_REWARD.amount );
      generate_block();

      auto start_time = db->head_block_time();
      const int64_t basic_votes = get_votes( "witness" );

      //Make some vests
      vest( "bob", "bob", ASSET( "1000.000 TESTS" ), bob_private_key );
      generate_block();

      STEEM_REQUIRE_THROW( witness_vote( "bob", "witness", false/*approve*/, bob_private_key ), fc::assert_exception) ;
      generate_block();
      BOOST_REQUIRE_EQUAL( get_votes( "witness" ), basic_votes );

      witness_vote( "bob", "witness", true/*approve*/, bob_private_key );
      generate_block();
      BOOST_REQUIRE_EQUAL( get_votes( "witness" ), basic_votes );

      witness_vote( "bob", "witness", false/*approve*/, bob_private_key );
      generate_block();
      BOOST_REQUIRE_EQUAL( get_votes( "witness" ), basic_votes );

      generate_blocks( start_time + STEEM_DELAYED_VOTING_TOTAL_INTERVAL_SECONDS );
      BOOST_REQUIRE_EQUAL( get_votes( "witness" ), basic_votes );

      validate_database();
   }
   FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( delayed_voting_basic_03 )
{
   BOOST_TEST_MESSAGE( "Testing: `delayed_voting::run` method" );
   //TODO
}

BOOST_AUTO_TEST_CASE( delayed_voting_basic_02 )
{
   auto vcmp = []( const std::deque< delayed_votes_data >& a, const account_object::t_delayed_votes& b )
   {
      return std::equal( a.begin(), a.end(), b.begin() );
   };

   BOOST_TEST_MESSAGE( "Testing: `delayed_voting::update_votes` method" );

   ACTORS( (alice)(bob) )
   generate_block();

   set_price_feed( price( ASSET( "1.000 TBD" ), ASSET( "1.000 TESTS" ) ) );
   generate_block();

   FUND( "alice", ASSET( "10000.000 TESTS" ) );
   FUND( "bob", ASSET( "10000.000 TESTS" ) );

   generate_block();

   /*
      The collection `items` stores `votes_update_data` structure. Below is the definition.
         struct votes_update_data
         {
            bool                    withdraw_executor;
            mutable int64_t         val;;
            const account_object*   account;
         };
   */
   delayed_voting::opt_votes_update_data_items items = delayed_voting::votes_update_data_items();

   delayed_voting dv = delayed_voting( *db );

   fc::time_point_sec time = db->head_block_time() + fc::hours( 5 );

   auto& __alice = db->get_account( "alice" );
   auto alice_dv = __alice.delayed_votes;
   auto alice_sum = __alice.sum_delayed_votes;

   auto& __bob = db->get_account( "bob" );
   auto bob_dv = __bob.delayed_votes;
   auto bob_sum = __bob.sum_delayed_votes;

   {
      delayed_voting::opt_votes_update_data_items _items;
      dv.update_votes( _items, time );
      auto& _alice = db->get_account( "alice" );
      BOOST_REQUIRE( vcmp( { { alice_dv[0].time, alice_dv[0].val } }, _alice.delayed_votes ) );
      BOOST_REQUIRE( alice_sum == _alice.sum_delayed_votes );
   }
   {
      dv.update_votes( items, time );
      auto& _alice = db->get_account( "alice" );
      BOOST_REQUIRE( vcmp( { { alice_dv[0].time, alice_dv[0].val } }, _alice.delayed_votes ) );
      BOOST_REQUIRE( alice_sum == _alice.sum_delayed_votes );
   }
   {
      dv.add_votes( items, false/*withdraw_executor*/, 0/*val*/, db->get_account( "alice" ) );
      dv.update_votes( items, time );
      auto& _alice = db->get_account( "alice" );
      BOOST_REQUIRE( vcmp( { { alice_dv[0].time, alice_dv[0].val } }, _alice.delayed_votes ) );
      BOOST_REQUIRE( alice_sum == _alice.sum_delayed_votes );
   }
   {
      dv.add_votes( items, false/*withdraw_executor*/, -5/*val*/, db->get_account( "alice" ) );
      //delayed_voting_messages::incorrect_votes_update
      STEEM_REQUIRE_THROW( dv.update_votes( items, time ), fc::exception );
      items->clear();

      dv.add_votes( items, true/*withdraw_executor*/, 1/*val*/, db->get_account( "alice" ) );
      //delayed_voting_messages::incorrect_votes_update
      STEEM_REQUIRE_THROW( dv.update_votes( items, time ), fc::exception );
      items->clear();
   }
   {
      auto& alice = db->get_account( "alice" );
      auto& bob = db->get_account( "bob" );

      dv.add_delayed_value( alice, time, 70 );
      dv.add_delayed_value( bob, time, 88 );

      dv.add_votes( items, false/*withdraw_executor*/, 7/*val*/, db->get_account( "alice" ) );
      dv.add_votes( items, true/*withdraw_executor*/, -8/*val*/, db->get_account( "bob" ) );
      dv.update_votes( items, time );

      auto& alice2 = db->get_account( "alice" );
      BOOST_REQUIRE( vcmp( { { alice_dv[0].time, alice_dv[0].val + 70 + 7 } }, alice2.delayed_votes ) );
      BOOST_REQUIRE( alice_sum + 70 + 7 == alice2.sum_delayed_votes );

      auto& bob2 = db->get_account( "bob" );
      BOOST_REQUIRE( vcmp( { { bob_dv[0].time, bob_dv[0].val + 88 - 8 } }, bob2.delayed_votes ) );
      BOOST_REQUIRE( bob_sum + 88 - 8 == bob2.sum_delayed_votes );
   }

}

BOOST_AUTO_TEST_CASE( delayed_voting_basic_01 )
{
   /*
      The collection `items` stores `votes_update_data` structure. Below is the definition.
         struct votes_update_data
         {
            bool                    withdraw_executor;
            mutable int64_t         val;;
            const account_object*   account;
         };
   */
   delayed_voting::opt_votes_update_data_items items = delayed_voting::votes_update_data_items();

   auto cmp = [ &items, this ]( bool withdraw_executor, int64_t val, const account_object& obj )
   {
      return check_collection( items, withdraw_executor, val, obj );
   };

   BOOST_TEST_MESSAGE( "Testing: `delayed_voting::add_votes` method" );

   ACTORS( (alice)(bob)(celine) )

   std::array< const account_object*, 3 > accs{ &db->get_account( "alice" ),
                                                &db->get_account( "bob" ),
                                                &db->get_account( "celine" )
                                              };

   delayed_voting dv = delayed_voting( *db );

   {
      delayed_voting::opt_votes_update_data_items _items;
      dv.add_votes( _items, true/*withdraw_executor*/, 1/*val*/, *accs[0] );
      BOOST_REQUIRE( !_items.valid() );
   }
   {
      dv.add_votes( items, true/*withdraw_executor*/, 0/*val*/, *accs[0] );
      BOOST_REQUIRE( items->size() == 0 );
   }
   {
      dv.add_votes( items, true/*withdraw_executor*/, 1/*val*/, *accs[0] );
      BOOST_REQUIRE( items->size() == 1 );
      BOOST_REQUIRE( cmp( true, 1, *accs[0] ) );
   }
   {
      //delayed_voting_messages::incorrect_withdraw_data
      STEEM_REQUIRE_THROW( dv.add_votes( items, false/*withdraw_executor*/, 88/*val*/, *accs[0] ), fc::exception );
   }
   {
      dv.add_votes( items, true/*withdraw_executor*/, 2/*val*/, *accs[1] );
      BOOST_REQUIRE( items->size() == 2 );
      BOOST_REQUIRE( cmp( true, 1, *accs[0] ) );
      BOOST_REQUIRE( cmp( true, 2, *accs[1] ) );
   }
   {
      dv.add_votes( items, true/*withdraw_executor*/, 3/*val*/, *accs[1] );
      BOOST_REQUIRE( items->size() == 2 );
      BOOST_REQUIRE( cmp( true, 1, *accs[0] ) );
      BOOST_REQUIRE( cmp( true, 5, *accs[1] ) );
   }
   {
      dv.add_votes( items, true/*withdraw_executor*/, 4/*val*/, *accs[2] );
      BOOST_REQUIRE( items->size() == 3 );
      BOOST_REQUIRE( cmp( true, 1, *accs[0] ) );
      BOOST_REQUIRE( cmp( true, 5, *accs[1] ) );
      BOOST_REQUIRE( cmp( true, 4, *accs[2] ) );
   }
   {
      //delayed_voting_messages::incorrect_withdraw_data
      STEEM_REQUIRE_THROW( dv.add_votes( items, false/*withdraw_executor*/, 4/*val*/, *accs[1] ), fc::exception );
   }
}

BOOST_AUTO_TEST_CASE( delayed_voting_processor_03 )
{
   std::deque< delayed_votes_data > dq;

   uint64_t sum = 0;

   fc::time_point_sec time = fc::variant( "2020-02-02T03:04:05" ).as< fc::time_point_sec >();

   {
      //nothing to do
      delayed_voting_processor::erase( dq, sum, 0 );
      delayed_voting_processor::add( dq, sum, time/*time*/, 0/*val*/ );
      delayed_voting_processor::erase_front( dq, sum );
   }

   {
      BOOST_REQUIRE( dq.size() == 0 );
      //delayed_voting_messages::incorrect_erased_votes
      STEEM_REQUIRE_THROW( delayed_voting_processor::erase( dq, sum, 1 ), fc::exception );
   }

   {
      delayed_voting_processor::add( dq, sum, time/*time*/, 666/*val*/ );
      delayed_voting_processor::erase( dq, sum, 666 );
      BOOST_REQUIRE( dq.size() == 0 );
   }

   {
      delayed_voting_processor::add( dq, sum, time + fc::days( 1 )/*time*/, 22/*val*/ );
      delayed_voting_processor::add( dq, sum, time + fc::days( 2 )/*time*/, 33/*val*/ );
      delayed_voting_processor::erase( dq, sum, 55 );
      BOOST_REQUIRE( dq.size() == 0 );
   }
}

BOOST_AUTO_TEST_CASE( delayed_voting_processor_02 )
{
   std::deque< delayed_votes_data > dq;

   auto cmp = [ &dq, this ]( size_t idx, const fc::time_point_sec& time, uint64_t val )
   {
      return check_collection( dq, idx, time, val );
   };

   uint64_t sum = 0;

   fc::time_point_sec time = fc::variant( "2020-01-02T03:04:05" ).as< fc::time_point_sec >();

   const uint32_t _max = 10;
   uint64_t calculated_sum = 0;

   //dq={1,2,3,4,5,6,7,8,9,10}
   for( uint32_t i = 0; i < _max; ++i )
   {
      calculated_sum += i + 1;
      delayed_voting_processor::add( dq, sum, time + fc::days( i + 1 )/*time*/, i + 1/*val*/ );
   }
   BOOST_REQUIRE( dq.size() == _max );
   BOOST_REQUIRE( sum == calculated_sum );

   for( uint32_t i = 0; i < _max; ++i )
      BOOST_REQUIRE( cmp( i/*idx*/, time + fc::days( i + 1 )/*time*/, i + 1/*val*/ ) );

   //Decrease gradually last element by `1` in `dq`.
   for( uint32_t cnt = 0; cnt < _max - 1; ++cnt )
   {
      idump( (cnt) );

      delayed_voting_processor::erase( dq, sum, 1 );
      BOOST_REQUIRE( dq.size() == _max );
      BOOST_REQUIRE( sum == calculated_sum - ( cnt + 1 ) );

      for( uint32_t i = 0; i < _max; ++i )
      {
         if( i == _max - 1 )
            BOOST_REQUIRE( cmp( i/*idx*/, time + fc::days( i + 1 )/*time*/, i - cnt/*val*/ ) );
         else
            BOOST_REQUIRE( cmp( i/*idx*/, time + fc::days( i + 1 )/*time*/, i + 1/*val*/ ) );
      }
   }

   //dq={1,2,3,4,5,6,7,8,9,1}
   //Last element has only `1`, so after below operation last element should be removed
   delayed_voting_processor::erase( dq, sum, 1 );
   BOOST_REQUIRE( dq.size() == _max - 1 );
   BOOST_REQUIRE( sum == calculated_sum - 10 );

   //dq={1,2,3,4,5,6,7,8,9}
   //Two last elements will disappear.
   delayed_voting_processor::erase( dq, sum, 17 );
   BOOST_REQUIRE( dq.size() == _max - 3 );
   BOOST_REQUIRE( sum == calculated_sum - 27 );

   //dq={1,2,3,4,5,6,7}
   delayed_voting_processor::erase( dq, sum, 7 );
   BOOST_REQUIRE( dq.size() == _max - 4 );
   BOOST_REQUIRE( sum == calculated_sum - 34 );

   //dq={1,2,3,4,5,6}
   delayed_voting_processor::erase( dq, sum, 18 );
   BOOST_REQUIRE( dq.size() == _max - 8 );
   BOOST_REQUIRE( sum == calculated_sum - 52 );

   //delayed_voting_messages::incorrect_erased_votes
   //dq={1,2}
   STEEM_REQUIRE_THROW( delayed_voting_processor::erase( dq, sum, 10 ), fc::exception );

   //dq={1,2}
   delayed_voting_processor::erase( dq, sum, 3 );
   BOOST_REQUIRE( dq.size() == 0 );
   BOOST_REQUIRE( sum == 0 );
}

BOOST_AUTO_TEST_CASE( delayed_voting_processor_01 )
{
   std::deque< delayed_votes_data > dq;

   auto cmp = [ &dq, this ]( size_t idx, const fc::time_point_sec& time, uint64_t val )
   {
      return check_collection( dq, idx, time, val );
   };

   uint64_t sum = 0;

   fc::time_point_sec time = fc::variant( "2000-01-01T00:00:00" ).as< fc::time_point_sec >();

   {
      delayed_voting_processor::add( dq, sum, time + fc::minutes( 1 )/*time*/, 1/*val*/ );
      BOOST_REQUIRE( dq.size() == 1 );
      BOOST_REQUIRE( sum == 1 );
      BOOST_REQUIRE( cmp( 0/*idx*/, time + fc::minutes( 1 )/*time*/, 1/*val*/ ) );
   }
   {
      delayed_voting_processor::add( dq, sum, time + fc::minutes( 1 ) + fc::hours( 1 )/*time*/, 2/*val*/ );
      BOOST_REQUIRE( dq.size() == 1 );
      BOOST_REQUIRE( sum == 3 );
      BOOST_REQUIRE( cmp( 0/*idx*/, time + fc::minutes( 1 )/*time*/, 3/*val*/ ) );
   }
   {
      delayed_voting_processor::add( dq, sum, time + fc::minutes( 1 ) + fc::hours( 24 ) - fc::seconds( 1 )/*time*/, 3/*val*/ );
      BOOST_REQUIRE( dq.size() == 1 );
      BOOST_REQUIRE( sum == 6 );
      BOOST_REQUIRE( cmp( 0/*idx*/, time + fc::minutes( 1 )/*time*/, 6/*val*/ ) );
   }
   {
      delayed_voting_processor::add( dq, sum, time + fc::minutes( 1 ) + fc::hours( 24 )/*time*/, 4/*val*/ );
      BOOST_REQUIRE( dq.size() == 2 );
      BOOST_REQUIRE( sum == 10 );
      BOOST_REQUIRE( cmp( 0/*idx*/, time + fc::minutes( 1 )/*time*/, 6/*val*/ ) );
      BOOST_REQUIRE( cmp( 1/*idx*/, time + fc::minutes( 1 ) + fc::hours( 24 )/*time*/, 4/*val*/ ) );
   }
   {
      delayed_voting_processor::add( dq, sum, time + fc::minutes( 1 ) + fc::hours( 2*24 )/*time*/, 5/*val*/ );
      BOOST_REQUIRE( dq.size() == 3 );
      BOOST_REQUIRE( sum == 15 );
      BOOST_REQUIRE( cmp( 0/*idx*/, time + fc::minutes( 1 )/*time*/, 6/*val*/ ) );
      BOOST_REQUIRE( cmp( 1/*idx*/, time + fc::minutes( 1 ) + fc::hours( 24 )/*time*/, 4/*val*/ ) );
      BOOST_REQUIRE( cmp( 2/*idx*/, time + fc::minutes( 1 ) + fc::hours( 2*24 )/*time*/, /*val*/5 ) );
   }
   {
      delayed_voting_processor::erase_front( dq, sum );
      BOOST_REQUIRE( dq.size() == 2 );
      BOOST_REQUIRE( sum == 9 );
      BOOST_REQUIRE( cmp( 0/*idx*/, time + fc::minutes( 1 ) + fc::hours( 24 )/*time*/, 4/*val*/ ) );
      BOOST_REQUIRE( cmp( 1/*idx*/, time + fc::minutes( 1 ) + fc::hours( 2*24 )/*time*/, 5/*val*/ ) );
   }
   {
      delayed_voting_processor::erase_front( dq, sum );
      BOOST_REQUIRE( dq.size() == 1 );
      BOOST_REQUIRE( sum == 5 );
      BOOST_REQUIRE( cmp( 0/*idx*/, time + fc::minutes( 1 ) + fc::hours( 2*24 )/*time*/, 5/*val*/ ) );
   }
   {
      delayed_voting_processor::add( dq, sum, time + fc::minutes( 2 ) + fc::hours( 2*24 )/*time*/, 6/*val*/ );
      BOOST_REQUIRE( dq.size() == 1 );
      BOOST_REQUIRE( sum == 11 );
      BOOST_REQUIRE( cmp( 0/*idx*/, time + fc::minutes( 1 ) + fc::hours( 2*24 )/*time*/, 11/*val*/ ) );
   }
   {
      delayed_voting_processor::add( dq, sum, time + fc::minutes( 1 ) + fc::hours( 3*24 )/*time*/, 7/*val*/ );
      BOOST_REQUIRE( dq.size() == 2 );
      BOOST_REQUIRE( sum == 18 );
      BOOST_REQUIRE( cmp( 0/*idx*/, time + fc::minutes( 1 ) + fc::hours( 2*24 )/*time*/, 11/*val*/ ) );
      BOOST_REQUIRE( cmp( 1/*idx*/, time + fc::minutes( 1 ) + fc::hours( 3*24 )/*time*/, 7/*val*/ ) );
   }
   {
      delayed_voting_processor::add( dq, sum, time + fc::minutes( 1 ) + fc::hours( 3*24 ) + fc::seconds( 3 )/*time*/, 8/*val*/ );
      BOOST_REQUIRE( dq.size() == 2 );
      BOOST_REQUIRE( sum == 26 );
      BOOST_REQUIRE( cmp( 0/*idx*/, time + fc::minutes( 1 ) + fc::hours( 2*24 )/*time*/, 11/*val*/ ) );
      BOOST_REQUIRE( cmp( 1/*idx*/, time + fc::minutes( 1 ) + fc::hours( 3*24 )/*time*/, 15/*val*/ ) );
   }
   {
      delayed_voting_processor::erase_front( dq, sum );
      BOOST_REQUIRE( dq.size() == 1 );
      BOOST_REQUIRE( sum == 15 );
      BOOST_REQUIRE( cmp( 0/*idx*/, time + fc::minutes( 1 ) + fc::hours( 3*24 )/*time*/, 15/*val*/ ) );
   }
   {
      delayed_voting_processor::erase_front( dq, sum );
      BOOST_REQUIRE( dq.size() == 0 );
      BOOST_REQUIRE( sum == 0 );

      delayed_voting_processor::erase_front( dq, sum );
      BOOST_REQUIRE( dq.size() == 0 );
      BOOST_REQUIRE( sum == 0 );

      delayed_voting_processor::erase_front( dq, sum );
      BOOST_REQUIRE( dq.size() == 0 );
      BOOST_REQUIRE( sum == 0 );
   }
   {
      //delayed_voting_messages::incorrect_sum_equal
      sum = 1;
      STEEM_REQUIRE_THROW( delayed_voting_processor::erase_front( dq, sum ), fc::exception );

      sum = 0;
      delayed_voting_processor::erase_front( dq, sum );
      BOOST_REQUIRE( dq.size() == 0 );
   }
   {
      delayed_voting_processor::add( dq, sum, time, 2/*val*/ );
      --sum;
      //delayed_voting_messages::incorrect_sum_greater_equal
      STEEM_REQUIRE_THROW( delayed_voting_processor::erase_front( dq, sum ), fc::exception );

      ++sum;
      delayed_voting_processor::erase_front( dq, sum );
      BOOST_REQUIRE( dq.size() == 0 );
      BOOST_REQUIRE( sum == 0 );
   }
   {
      delayed_voting_processor::add( dq, sum, time + fc::seconds( 30 )/*time*/, 1000/*val*/ );
      BOOST_REQUIRE( dq.size() == 1 );
      BOOST_REQUIRE( sum == 1000 );
      BOOST_REQUIRE( cmp( 0/*idx*/, time + fc::seconds( 30 )/*time*/, 1000/*val*/ ) );

      //delayed_voting_messages::incorrect_head_time
      STEEM_REQUIRE_THROW( delayed_voting_processor::add( dq, sum, time + fc::seconds( 29 )/*time*/, 1000/*val*/ ), fc::exception );

      delayed_voting_processor::erase_front( dq, sum );
      BOOST_REQUIRE( dq.size() == 0 );
   }
}

#define VOTING_POWER( account ) db->get_account( account ).witness_vote_weight().value
#define PROXIED_VSF( account ) db->get_account( account ).proxied_vsf_votes_total().value

#define ACCOUNT_REPORT( account ) \
   BOOST_TEST_MESSAGE( "[abw_scenario_01]: " << account << " has " << asset_to_string( db->get_account( account ).vesting_shares) ); \
   BOOST_TEST_MESSAGE( "[abw_scenario_01]: " << account << " has " << asset_to_string( db->get_account( account ).balance) ); \
   BOOST_TEST_MESSAGE( "[abw_scenario_01]: " << account << " has " << VOTING_POWER( account ) << " voting power" ); \
   BOOST_TEST_MESSAGE( "[abw_scenario_01]: " << account << " has " << PROXIED_VSF( account ) << " proxied vsf" )

#define WITNESS_VOTES( witness ) db->get_witness( witness ).votes.value

#define DELAYED_VOTES( account ) db->get_account( account ).sum_delayed_votes

#define DAY_REPORT( day ) \
   BOOST_TEST_MESSAGE( "[abw_scenario_01]: current day: " << day ); \
   ACCOUNT_REPORT( "alice" ); \
   ACCOUNT_REPORT( "alice0bp" ); \
   BOOST_TEST_MESSAGE( "[abw_scenario_01]: " << "alice0bp" << " has " << WITNESS_VOTES( "alice0bp" ) << " votes"); \
   BOOST_TEST_MESSAGE( "[abw_scenario_01]: " << "alice0bp" << " has " << DELAYED_VOTES( "alice0bp" ) << " delayed votes"); \
   ACCOUNT_REPORT( "bob" ); \
   ACCOUNT_REPORT( "bob0bp" ); \
   BOOST_TEST_MESSAGE( "[abw_scenario_01]: " << "bob0bp" << " has " << WITNESS_VOTES( "bob0bp" ) << " votes"); \
   BOOST_TEST_MESSAGE( "[abw_scenario_01]: " << "bob0bp" << " has " << DELAYED_VOTES( "bob0bp" ) << " delayed votes"); \
   ACCOUNT_REPORT( "carol" )

#define CHECK_ACCOUNT_VESTS( account ) \
   BOOST_REQUIRE( db->get_account( #account ).vesting_shares == expected_ ## account ## _vests )

#define CHECK_ACCOUNT_STEEM( account ) \
   BOOST_REQUIRE( db->get_account( #account ).balance == expected_ ## account ## _steem )

#define CHECK_ACCOUNT_VP( account ) \
   BOOST_REQUIRE( VOTING_POWER( #account ) == expected_ ## account ## _vp )

#define CHECK_WITNESS_VOTES( witness ) \
   BOOST_REQUIRE( WITNESS_VOTES( #witness ) == expected_ ## witness ## _votes )

// alice:     V + S
// alice0bp:  V + VP
// bob:       V
// bob0bp:    V + VP
// carol:     S
#define DAY_CHECK \
   /*CHECK_ACCOUNT_VESTS( alice ); \
   CHECK_ACCOUNT_STEEM( alice ); \
   CHECK_ACCOUNT_VESTS( alice0bp ); \
   CHECK_ACCOUNT_VP( alice0bp ); \
   CHECK_ACCOUNT_VESTS( bob ); \
   CHECK_ACCOUNT_VESTS( bob0bp ); \
   CHECK_ACCOUNT_VP( bob0bp ); \
   CHECK_ACCOUNT_STEEM( carol )*/

#define GOTO_DAY( day ) \
   generate_days_blocks( day - today ); \
   today = day

BOOST_AUTO_TEST_CASE( abw_scenario_01 )
{
   try {

   /*asset expected_alice_vests;
   asset expected_alice_steem;
   asset expected_alice0bp_vests;
   int64_t expected_alice0bp_vp;
   int64_t expected_alice0bp_votes;
   asset expected_bob_vests;
   asset expected_bob0bp_vests;
   int64_t expected_bob0bp_vp;
   int64_t expected_bob0bp_votes;
   asset expected_carol_steem;*/

/* https://gitlab.syncad.com/hive-group/steem/issues/5#note_24084

  actors: alice, alice.bp (witness of alice choice), bob, bob.bp (witness of bob choice), carol
*/
   ACTORS( (alice)(alice0bp)(bob)(bob0bp)(carol) );
   generate_block();

   BOOST_TEST_MESSAGE( "[abw_scenario_01]: after account creation" );
   BOOST_TEST_MESSAGE( "[abw_scenario_01]: alice has " << asset_to_string( db->get_account( "alice" ).vesting_shares) );
   BOOST_TEST_MESSAGE( "[abw_scenario_01]: alice has " << asset_to_string( db->get_account( "alice" ).balance) );

   //set_price_feed( price( ASSET( "1.000 TBD" ), ASSET( "1.000 TESTS" ) ) );
   //generate_block();

   witness_create( "alice0bp", alice0bp_private_key, "url.alice.bp", alice0bp_private_key.get_public_key(), STEEM_MIN_PRODUCER_REWARD.amount );
   witness_create( "bob0bp", bob0bp_private_key, "url.bob.bp", bob0bp_private_key.get_public_key(), STEEM_MIN_PRODUCER_REWARD.amount );
   generate_block();

/*
  For simplicity we are going to assume 1 vest == 1 STEEM at all times but in reality all conversions
  are done using rate from the time when action is performed.
  
  Before test starts alice has 1300 STEEM in liquid form. She sets up vest route in the following way:
  - to alice in vest form 25%
  - to bob in vest form 25%
  - to carol in liquid form 25%

  leaving remaining 25% for herself in liquid form.
*/
   signed_transaction tx;
   fund( "alice", ASSET( "1300.000 TESTS" ) );

   {
   BOOST_TEST_MESSAGE( "[abw_scenario_01]: Setting up alice destination" );
   set_withdraw_vesting_route_operation op;
   op.from_account = "alice";
   op.to_account = "alice";
   op.percent = STEEM_1_PERCENT * 25;
   op.auto_vest = true;
   tx.operations.push_back( op );
   }

   {
   BOOST_TEST_MESSAGE( "[abw_scenario_01]: Setting up bob destination" );
   set_withdraw_vesting_route_operation op;
   op.from_account = "alice";
   op.to_account = "bob";
   op.percent = STEEM_1_PERCENT * 25;
   op.auto_vest = true;
   tx.operations.push_back( op );
   }

   {
   BOOST_TEST_MESSAGE( "[abw_scenario_01]: Setting up carol destination" );
   set_withdraw_vesting_route_operation op;
   op.from_account = "alice";
   op.to_account = "carol";
   op.percent = STEEM_1_PERCENT * 25;
   op.auto_vest = false;
   tx.operations.push_back( op );
   }

   tx.set_expiration( db->head_block_time() + STEEM_MAX_TIME_UNTIL_EXPIRATION );
   sign( tx, alice_private_key );
   db->push_transaction( tx, 0 );
   validate_database();

   // set alice.bp as witness of alice choice
   witness_vote( "alice", "alice0bp", true/*approve*/, alice_private_key );
   // set bob.bp as witness of bob choice
   witness_vote( "bob", "bob0bp", true/*approve*/, bob_private_key );

   generate_block();

/*
  Day 0: alice powers up 1000 STEEM; she has 1000 vests, including delayed maturing on day 30 and 300 STEEM
*/
   uint32_t today = 0;
   asset initial_alice_vests = db->get_account( "alice" ).vesting_shares;
   BOOST_TEST_MESSAGE( "[abw_scenario_01]: day_zero = " << today );
   BOOST_TEST_MESSAGE( "[abw_scenario_01]: head_block_num = " << db->head_block_num() );

   BOOST_TEST_MESSAGE( "[abw_scenario_01]: alice powers up 1000" );
   vest( "alice", "alice", ASSET( "1000.000 TESTS" ), alice_private_key );
   validate_database();

   DAY_REPORT( today );
   //BOOST_REQUIRE( db->get_account( "alice" ).vesting_shares == ASSET ( "10000.000000 VESTS" ) );
   BOOST_REQUIRE( db->get_account( "alice" ).balance == ASSET( "300.000 TESTS" ) );

/*
  Day 5: alice powers up 300 STEEM; she has 1300 vests, including delayed 1000 maturing on day 30 and 300 maturing on day 35, 0 STEEM
*/
   BOOST_TEST_MESSAGE( "[abw_scenario_01]: before set_current_day( 5 ): " << get_current_time_iso_string() );
   GOTO_DAY( 5 );
   BOOST_TEST_MESSAGE( "[abw_scenario_01]: after set_current_day( 5 ): " << get_current_time_iso_string() );

   BOOST_TEST_MESSAGE( "[abw_scenario_01]: alice powers up 300" );
   vest( "alice", "alice", ASSET( "300.000 TESTS" ), alice_private_key );
   validate_database();

   DAY_REPORT( today );
   //BOOST_REQUIRE( db->get_account( "alice" ).vesting_shares == ASSET( "1300.000000 VESTS" ) );
   BOOST_REQUIRE( db->get_account( "alice" ).balance == ASSET( "0.000 TESTS" ) );

/*
  Day 10: alice powers down 1300 vests; this schedules virtual PD actions on days 17, 24, 31, 38, 45, 52, 59, 66, 73, 80, 87, 94 and 101
*/
   GOTO_DAY( 10 );

   asset new_vests = db->get_account( "alice" ).vesting_shares - initial_alice_vests;
   BOOST_TEST_MESSAGE( "[abw_scenario_01]: new alice vests = " << asset_to_string( new_vests ) );

   int64_t quarter = new_vests.amount.value / 4;
   std::array<asset, STEEM_VESTING_WITHDRAW_INTERVALS> vests_portions;
   std::array<asset, STEEM_VESTING_WITHDRAW_INTERVALS> tests_portions;

   for (int i = 0; i < STEEM_VESTING_WITHDRAW_INTERVALS; ++i)
   {
      vests_portions[i] = asset( (quarter * (i + 1)) / STEEM_VESTING_WITHDRAW_INTERVALS, VESTS_SYMBOL );
      BOOST_TEST_MESSAGE( "[abw_scenario_01]: vests_portions[" << i << "] = " << asset_to_string( vests_portions[i] ) );
      tests_portions[i] = asset( 25 * (i + 1), STEEM_SYMBOL );
   }

   BOOST_TEST_MESSAGE( "[abw_scenario_01]: alice powers down all new vests" );
   withdraw_vesting( "alice", new_vests, alice_private_key );
   validate_database();

   DAY_REPORT( today );
   // ? BOOST_REQUIRE( db->get_account( "alice" ).vesting_shares == ASSET( "1300.000000 VESTS" ) );
   BOOST_REQUIRE( db->get_account( "alice" ).balance == ASSET( "0.000 TESTS" ) );

/*
  Day 17: PD of 100 vests from alice gives 25 STEEM to carol, 25 STEEM to alice, powers up 25 vests to bob (maturing on day 47)
  and ignores 25 vests of power down (power down canceled in 25% by vest route to alice);
  since there is nonzero balance as delayed, 75 vests are subtracted from second record leaving 1000 (30day) + 225 (35day)
  alice 25S+1225v=0+1000(30d)+225(35d); bob 25v=0+25(47d); carol 25S
*/
   /*expected_alice_vests = db->get_account( "alice" ).vesting_shares;
   expected_alice_steem = db->get_account( "alice" ).balance;
   expected_alice0bp_vests = db->get_account( "alice0bp" ).vesting_shares;
   expected_alice0bp_vp = VOTING_POWER( "alice0bp" );
   expected_alice0bp_votes = WITNESS_VOTES( "alice0bp" );
   expected_bob_vests = db->get_account( "bob" ).vesting_shares;
   expected_bob0bp_vests = db->get_account( "bob0bp" ).vesting_shares;
   expected_bob0bp_vp = VOTING_POWER( "bob0bp");
   expected_bob0bp_votes = WITNESS_VOTES( "bob0bp" );
   expected_carol_steem = db->get_account( "carol" ).balance;*/

   GOTO_DAY( 17 );
   DAY_REPORT( today );
   DAY_CHECK;

/*
  Day 24: PD of 100 vests from alice split like above
  alice 50S+1150v=0+1000(30d)+150(35d); bob 50v=0+25(47d)+25(54d); carol 50S
*/
   GOTO_DAY( 24 );
   DAY_REPORT( today );
   DAY_CHECK;

/*
  Day 30: 1000 vests matures on alice, alice.bp receives 1000 vests of new voting power (1000v)
  alice 50S+1150v=1000+150(35d); bob 50v=0+25(47d)+25(54d); carol 50S
*/
   GOTO_DAY( 30 );
   DAY_REPORT( today );
   DAY_CHECK;
  
/*
  Day 31: PD of 100 vests from alice
  alice 75S+1075v=1000+75(35d); bob 75v=0+25(47d)+25(54d)+25(61d); carol 75S
*/
   GOTO_DAY( 31 );
   DAY_REPORT( today );
   DAY_CHECK;
  
/*
  Day 35: remaining 75 vests mature on alice, alice.bp receives 75 vests of new voting power (1075v)
  alice 75S+1075v=1075; bob 75v=0+25(47d)+25(54d)+25(61d); carol 75S
*/
   GOTO_DAY( 35 );
   DAY_REPORT( today );
   DAY_CHECK;
  
/*
  Day 38: PD of 100 vests from alice, alice.bp loses 75 vests of voting power (1000v)
  alice 100S+1000v=1000; bob 100v=0+25(47d)+25(54d)+25(61d)+25(68d); carol 100S
*/
   GOTO_DAY( 38 );
   DAY_REPORT( today );
   DAY_CHECK;
  
/*
  Day 45: PD of 100 vests from alice, alice.bp loses 75 vests of voting power (925v)
  alice 125S+925v=925; bob 125v=0+25(47d)+25(54d)+25(61d)+25(68d)+25(75d); carol 125S
*/
   GOTO_DAY( 45 );
   DAY_REPORT( today );
   DAY_CHECK;
  
/*
  Day 47: first 25 vests mature on bob, bob.bp receives 25 vests of new voting power (25v)
  alice 125S+925v=925; bob 125v=25+25(54d)+25(61d)+25(68d)+25(75d); carol 125S
*/
   GOTO_DAY( 47 );
   DAY_REPORT( today );
   DAY_CHECK;
  
/*
  Day 52: PD of 100 vests from alice, alice.bp loses 75 vests of voting power (850v)
  alice 150S+850v=850; bob 150v=25+25(54d)+25(61d)+25(68d)+25(75d)+25(82d); carol 150S
*/
   GOTO_DAY( 52 );
   DAY_REPORT( today );
   DAY_CHECK;
  
/*
  Day 54: 25 vests mature on bob, bob.bp receives 25 vests of new voting power (50v)
  alice 150S+850v=850; bob 150v=50+25(61d)+25(68d)+25(75d)+25(82d); carol 150S
*/
   GOTO_DAY( 54 );
   DAY_REPORT( today );
   DAY_CHECK;
  
/*
  Day 59: PD of 100 vests from alice, alice.bp loses 75 vests of voting power (775v)
  alice 175S+775v=775; bob 175v=50+25(61d)+25(68d)+25(75d)+25(82d)+25(89d); carol 175S
*/
   GOTO_DAY( 59 );
   DAY_REPORT( today );
   DAY_CHECK;
  
/*
  Day 61: 25 vests mature on bob, bob.bp receives 25 vests of new voting power (75v)
  alice 175S+775v=775; bob 175v=75+25(68d)+25(75d)+25(82d)+25(89d); carol 175S
*/
   GOTO_DAY( 61 );
   DAY_REPORT( today );
   DAY_CHECK;
  
/*
  Day 66: PD of 100 vests from alice, alice.bp loses 75 vests of voting power (700v)
  alice 200S+700v=700; bob 200v=75+25(68d)+25(75d)+25(82d)+25(89d)+25(96d); carol 200S
*/
   GOTO_DAY( 66 );
   DAY_REPORT( today );
   DAY_CHECK;
  
/*
  Day 68: 25 vests mature on bob, bob.bp receives 25 vests of new voting power (100v)
  alice 200S+700v=700; bob 200v=100+25(75d)+25(82d)+25(89d)+25(96d); carol 200S
*/
   GOTO_DAY( 68 );
   DAY_REPORT( today );
   DAY_CHECK;
  
/*
  Day 73: PD of 100 vests from alice, alice.bp loses 75 vests of voting power (625v)
  alice 225S+625v=625; bob 225v=100+25(75d)+25(82d)+25(89d)+25(96d)+25(103d); carol 225S
*/
   GOTO_DAY( 73 );
   DAY_REPORT( today );
   DAY_CHECK;
  
/*
  Day 75: 25 vests mature on bob, bob.bp receives 25 vests of new voting power (125v)
  alice 225S+625v=625; bob 225v=125+25(82d)+25(89d)+25(96d)+25(103d); carol 225S
*/
   GOTO_DAY( 75 );
   DAY_REPORT( today );
   DAY_CHECK;
  
/*
  Day 80: PD of 100 vests from alice, alice.bp loses 75 vests of voting power (550v)
  alice 250S+550v=550; bob 250v=125+25(82d)+25(89d)+25(96d)+25(103d)+25(110d); carol 250S
*/
   GOTO_DAY( 80 );
   DAY_REPORT( today );
   DAY_CHECK;
  
/*
  Day 82: 25 vests mature on bob, bob.bp receives 25 vests of new voting power (150v)
  alice 250S+550v=550; bob 250v=150+25(89d)+25(96d)+25(103d)+25(110d); carol 250S
*/
   GOTO_DAY( 82 );
   DAY_REPORT( today );
   DAY_CHECK;
  
/*
  Day 87: PD of 100 vests from alice, alice.bp loses 75 vests of voting power (475v)
  alice 275S+475v=475; bob 275v=150+25(89d)+25(96d)+25(103d)+25(110d)+25(117d); carol 275S
*/
   GOTO_DAY( 87 );
   DAY_REPORT( today );
   DAY_CHECK;
  
/*
  Day 89: 25 vests mature on bob, bob.bp receives 25 vests of new voting power (175v)
  alice 275S+475v=475; bob 275v=175+25(96d)+25(103d)+25(110d)+25(117d); carol 275S
*/
   GOTO_DAY( 89 );
   DAY_REPORT( today );
   DAY_CHECK;
  
/*
  Day 94: PD of 100 vests from alice, alice.bp loses 75 vests of voting power (400v)
  alice 300S+400v=400; bob 300v=175+25(96d)+25(103d)+25(110d)+25(117d)+25(124d); carol 300S
*/
   GOTO_DAY( 94 );
   DAY_REPORT( today );
   DAY_CHECK;
  
/*
  Day 96: 25 vests mature on bob, bob.bp receives 25 vests of new voting power (200v)
  alice 300S+400v=400; bob 300v=200+25(103d)+25(110d)+25(117d)+25(124d); carol 300S
*/
   GOTO_DAY( 96 );
   DAY_REPORT( today );
   DAY_CHECK;
  
/*
  Day 101: last scheduled PD of 100 vests from alice, alice.bp loses 75 vests of voting power (325v)
  alice 325S+325v=325; bob 325v=200+25(103d)+25(110d)+25(117d)+25(124d)+25(131d); carol 325S
*/
   GOTO_DAY( 101 );
   DAY_REPORT( today );
   DAY_CHECK;
  
/*
  Day 103: 25 vests mature on bob, bob.bp receives 25 vests of new voting power (225v)
  alice 325S+325v=325; bob 325v=225+25(110d)+25(117d)+25(124d)+25(131d); carol 325S
*/
   GOTO_DAY( 103 );
   DAY_REPORT( today );
   DAY_CHECK;
  
/*
  Day 110: 25 vests mature on bob, bob.bp receives 25 vests of new voting power (250v)
  alice 325S+325v=325; bob 325v=250+25(117d)+25(124d)+25(131d); carol 325S
*/
   GOTO_DAY( 110 );
   DAY_REPORT( today );
   DAY_CHECK;
  
/*
  Day 117: 25 vests mature on bob, bob.bp receives 25 vests of new voting power (275v)
  alice 325S+325v=325; bob 325v=275+25(124d)+25(131d); carol 325S
*/
   GOTO_DAY( 117 );
   DAY_REPORT( today );
   DAY_CHECK;
  
/*
  Day 124: 25 vests mature on bob, bob.bp receives 25 vests of new voting power (300v)
  alice 325S+325v=325; bob 325v=300+25(131d); carol 325S
*/
   GOTO_DAY( 124 );
   DAY_REPORT( today );
   DAY_CHECK;
  
/*
  Day 131: last 25 vests mature on bob, bob.bp receives 25 vests of new voting power (325v)
  alice 325S+325v=325; bob 325v=325; carol 325S
*/
   GOTO_DAY( 131 );
   DAY_REPORT( today );
   DAY_CHECK;

   }
   FC_LOG_AND_RETHROW()
}

#undef ACCOUNT_REPORT
#undef DAY_REPORT
#undef GOTO_DAY

BOOST_AUTO_TEST_SUITE_END()

#endif // #if defined(IS_TEST_NET)
