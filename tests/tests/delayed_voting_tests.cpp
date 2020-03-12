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

using namespace steem;
using namespace steem::chain;
using namespace steem::protocol;
using fc::string;

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
      BOOST_REQUIRE_EQUAL( basic_votes, votes_01 );

      generate_blocks( start_time + STEEM_DELAYED_VOTING_TOTAL_INTERVAL_SECONDS , true );
      generate_block();

      int64_t votes_02 = get_votes( "witness" );
      BOOST_REQUIRE_GT( votes_02, basic_votes );

      validate_database();
   }
   FC_LOG_AND_RETHROW()
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

BOOST_AUTO_TEST_SUITE_END()
