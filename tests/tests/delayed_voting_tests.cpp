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

BOOST_FIXTURE_TEST_SUITE( delayed_voting_tests, clean_database_fixture )

BOOST_AUTO_TEST_CASE( delayed_voting_processor_01 )
{
   std::deque< delayed_votes_data > dq;

   auto cmp = [ &dq ]( size_t idx, const fc::time_point_sec& time, int64_t val )
   {
      if( idx >= dq.size() )
         return false;
      else
         return ( dq[idx].time == time ) && ( dq[idx].val == val );
   };

   int64_t sum = 0;

   fc::time_point_sec time = fc::variant( "2000-01-01T00:00:00" ).as< fc::time_point_sec >();

   {
      delayed_voting_processor::add( dq, sum, time + fc::minutes( 1 ), 1/*val*/ );
      BOOST_REQUIRE( dq.size() == 1 );
      BOOST_REQUIRE( sum == 1 );
      BOOST_REQUIRE( cmp( 0/*idx*/, time + fc::minutes( 1 ), 1 ) );
   }
   {
      delayed_voting_processor::add( dq, sum, time + fc::minutes( 1 ) + fc::hours( 1 ), 2/*val*/ );
      BOOST_REQUIRE( dq.size() == 1 );
      BOOST_REQUIRE( sum == 3 );
      BOOST_REQUIRE( cmp( 0/*idx*/, time + fc::minutes( 1 ), 3 ) );
   }
   {
      delayed_voting_processor::add( dq, sum, time + fc::minutes( 1 ) + fc::hours( 24 ) - fc::seconds( 1 ), 3/*val*/ );
      BOOST_REQUIRE( dq.size() == 1 );
      BOOST_REQUIRE( sum == 6 );
      BOOST_REQUIRE( cmp( 0/*idx*/, time + fc::minutes( 1 ), 6 ) );
   }
   {
      delayed_voting_processor::add( dq, sum, time + fc::minutes( 1 ) + fc::hours( 24 ), 4/*val*/ );
      BOOST_REQUIRE( dq.size() == 2 );
      BOOST_REQUIRE( sum == 10 );
      BOOST_REQUIRE( cmp( 0/*idx*/, time + fc::minutes( 1 ), 6 ) );
      BOOST_REQUIRE( cmp( 1/*idx*/, time + fc::minutes( 1 ) + fc::hours( 24 ), 4 ) );
   }
   {
      delayed_voting_processor::add( dq, sum, time + fc::minutes( 1 ) + fc::hours( 2*24 ), 5/*val*/ );
      BOOST_REQUIRE( dq.size() == 3 );
      BOOST_REQUIRE( sum == 15 );
      BOOST_REQUIRE( cmp( 0/*idx*/, time + fc::minutes( 1 ), 6 ) );
      BOOST_REQUIRE( cmp( 1/*idx*/, time + fc::minutes( 1 ) + fc::hours( 24 ), 4 ) );
      BOOST_REQUIRE( cmp( 2/*idx*/, time + fc::minutes( 1 ) + fc::hours( 2*24 ), 5 ) );
   }
   {
      delayed_voting_processor::erase_front( dq, sum );
      BOOST_REQUIRE( dq.size() == 2 );
      BOOST_REQUIRE( sum == 9 );
      BOOST_REQUIRE( cmp( 0/*idx*/, time + fc::minutes( 1 ) + fc::hours( 24 ), 4 ) );
      BOOST_REQUIRE( cmp( 1/*idx*/, time + fc::minutes( 1 ) + fc::hours( 2*24 ), 5 ) );
   }
   {
      delayed_voting_processor::erase_front( dq, sum );
      BOOST_REQUIRE( dq.size() == 1 );
      BOOST_REQUIRE( sum == 5 );
      BOOST_REQUIRE( cmp( 0/*idx*/, time + fc::minutes( 1 ) + fc::hours( 2*24 ), 5 ) );
   }
   {
      delayed_voting_processor::add( dq, sum, time + fc::minutes( 2 ) + fc::hours( 2*24 ), 6/*val*/ );
      BOOST_REQUIRE( dq.size() == 1 );
      BOOST_REQUIRE( sum == 11 );
      BOOST_REQUIRE( cmp( 0/*idx*/, time + fc::minutes( 1 ) + fc::hours( 2*24 ), 11 ) );
   }
   {
      delayed_voting_processor::add( dq, sum, time + fc::minutes( 1 ) + fc::hours( 3*24 ), 7/*val*/ );
      BOOST_REQUIRE( dq.size() == 2 );
      BOOST_REQUIRE( sum == 18 );
      BOOST_REQUIRE( cmp( 0/*idx*/, time + fc::minutes( 1 ) + fc::hours( 2*24 ), 11 ) );
      BOOST_REQUIRE( cmp( 1/*idx*/, time + fc::minutes( 1 ) + fc::hours( 3*24 ), 7 ) );
   }
   {
      delayed_voting_processor::add( dq, sum, time + fc::minutes( 1 ) + fc::hours( 3*24 ) + fc::seconds( 3 ), 8/*val*/ );
      BOOST_REQUIRE( dq.size() == 2 );
      BOOST_REQUIRE( sum == 26 );
      BOOST_REQUIRE( cmp( 0/*idx*/, time + fc::minutes( 1 ) + fc::hours( 2*24 ), 11 ) );
      BOOST_REQUIRE( cmp( 1/*idx*/, time + fc::minutes( 1 ) + fc::hours( 3*24 ), 15 ) );
   }
   {
      delayed_voting_processor::erase_front( dq, sum );
      BOOST_REQUIRE( dq.size() == 1 );
      BOOST_REQUIRE( sum == 15 );
      BOOST_REQUIRE( cmp( 0/*idx*/, time + fc::minutes( 1 ) + fc::hours( 3*24 ), 15 ) );
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
      delayed_voting_processor::add( dq, sum, time + fc::seconds( 30 ), 1000/*val*/ );
      BOOST_REQUIRE( dq.size() == 1 );
      BOOST_REQUIRE( sum == 1000 );
      BOOST_REQUIRE( cmp( 0/*idx*/, time + fc::seconds( 30 ), 1000 ) );

      //delayed_voting_messages::incorrect_head_time
      STEEM_REQUIRE_THROW( delayed_voting_processor::add( dq, sum, time + fc::seconds( 29 ), 1000/*val*/ ), fc::exception );

      delayed_voting_processor::erase_front( dq, sum );
      BOOST_REQUIRE( dq.size() == 0 );
   }
}

BOOST_AUTO_TEST_SUITE_END()
