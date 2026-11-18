#include <boost/test/unit_test.hpp>

#include <graphene/net/config.hpp>
#include <graphene/net/node.hpp>

#include <hive/protocol/config.hpp>

#include <cstddef>

using graphene::net::transaction_size_estimate;

namespace {

const size_t max_transactions = GRAPHENE_NET_MAX_TRANSACTIONS_PER_PEER_DURING_NORMAL_OPERATION;
const size_t max_bytes = GRAPHENE_NET_MAX_TRANSACTION_BYTES_PER_FETCH;

/// how many requests, each as large as allowed and filled with transactions of @p size, until a request may
/// ask for @p wanted transactions; 0 if it doesn't get there within 100 requests
size_t requests_until( transaction_size_estimate& estimate, size_t size, size_t wanted )
{
  for( size_t requests = 0; requests < 100; ++requests )
  {
    if( estimate.max_transactions_per_request() >= wanted )
      return requests;
    for( size_t i = estimate.max_transactions_per_request(); i > 0; --i )
      estimate.record( size );
  }
  return 0;
}

} // namespace

BOOST_AUTO_TEST_SUITE( p2p_transaction_request_size_tests )

/// Before the peer sent a transaction, a request assumes the largest ones: its reply stays within the byte limit.
BOOST_AUTO_TEST_CASE( initial_estimate_assumes_largest_transactions )
{
  transaction_size_estimate estimate;
  BOOST_CHECK_EQUAL( estimate.mean(), size_t( HIVE_MAX_TRANSACTION_SIZE ) );
  BOOST_CHECK_EQUAL( estimate.max_transactions_per_request(), max_bytes / HIVE_MAX_TRANSACTION_SIZE );
  BOOST_CHECK_LE( estimate.max_transactions_per_request() * HIVE_MAX_TRANSACTION_SIZE, max_bytes );
}

/// Small transactions bring a request up to the item limit within a few round trips, and never past it.
BOOST_AUTO_TEST_CASE( small_transactions_reach_item_cap )
{
  BOOST_CHECK_EQUAL( max_transactions, 400u );
  transaction_size_estimate estimate;
  const size_t requests = requests_until( estimate, 300, max_transactions );
  BOOST_CHECK_GT( requests, 0u );
  BOOST_CHECK_LE( requests, 6u );
  for( int i = 0; i < 10000; ++i )
    estimate.record( 300 );
  BOOST_CHECK_EQUAL( estimate.max_transactions_per_request(), max_transactions );
}

/// With transactions of steady size the item count settles where the expected reply fits the byte limit.
BOOST_AUTO_TEST_CASE( byte_cap_limits_items_for_large_transactions )
{
  transaction_size_estimate estimate;
  for( int i = 0; i < 1000; ++i )
    estimate.record( 4096 );
  BOOST_CHECK_EQUAL( estimate.mean(), 4096u + 15u ); // the mean settles within 1/16 of a byte step above the size
  BOOST_CHECK_EQUAL( estimate.max_transactions_per_request(), max_bytes / ( 4096 + 15 ) );
  BOOST_CHECK_LE( estimate.max_transactions_per_request() * 4096, max_bytes );
  BOOST_CHECK_LT( estimate.max_transactions_per_request(), max_transactions );
}

/// A few large transactions after many small ones shrink the next request at once.
BOOST_AUTO_TEST_CASE( burst_of_large_transactions_shrinks_request_quickly )
{
  transaction_size_estimate estimate;
  for( int i = 0; i < 1000; ++i )
    estimate.record( 500 );
  BOOST_REQUIRE_EQUAL( estimate.max_transactions_per_request(), max_transactions );

  for( int i = 0; i < 4; ++i )
    estimate.record( HIVE_MAX_TRANSACTION_SIZE );
  BOOST_CHECK_GE( estimate.mean(), size_t( HIVE_MAX_TRANSACTION_SIZE ) / 2 );
  BOOST_CHECK_LE( estimate.max_transactions_per_request(), 2 * max_bytes / HIVE_MAX_TRANSACTION_SIZE );
}

/// A peer sending transactions larger than the whole byte limit is still asked for one at a time.
BOOST_AUTO_TEST_CASE( at_least_one_transaction_per_request )
{
  transaction_size_estimate estimate;
  for( int i = 0; i < 100; ++i )
    estimate.record( 2 * max_bytes );
  BOOST_CHECK_EQUAL( estimate.max_transactions_per_request(), 1u );
}

BOOST_AUTO_TEST_SUITE_END()
