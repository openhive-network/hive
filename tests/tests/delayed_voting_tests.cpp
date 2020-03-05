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
   fc::time_point_sec time = fc::variant( "2000-00-00T00:00:00" ).as< fc::time_point_sec >();

   {
      time = time + fc::seconds( 50 );

      delayed_voting_processor::add( dq, sum, time, 1/*val*/ );
      BOOST_REQUIRE( dq.size() == 1 );
      BOOST_REQUIRE( sum == 1 );
      BOOST_REQUIRE( cmp( 0/*idx*/, time, 1 ) );
   }
}

BOOST_AUTO_TEST_SUITE_END()
