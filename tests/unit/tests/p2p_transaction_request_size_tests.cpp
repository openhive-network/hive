#include <boost/test/unit_test.hpp>

#include <graphene/net/config.hpp>
#include <graphene/net/node.hpp>

#include <hive/protocol/config.hpp>

#include <algorithm>
#include <cstddef>
#include <string>

using graphene::net::item_id;
using graphene::net::requested_items;
using graphene::net::transaction_size_estimate;

namespace {

const size_t max_transactions = GRAPHENE_NET_MAX_TRANSACTIONS_PER_PEER_DURING_NORMAL_OPERATION;
const size_t max_bytes = GRAPHENE_NET_MAX_TRANSACTION_BYTES_PER_FETCH;
const size_t max_in_flight = GRAPHENE_NET_MAX_TRANSACTIONS_IN_FLIGHT_PER_PEER;
const size_t max_bytes_in_flight = GRAPHENE_NET_MAX_TRANSACTION_BYTES_IN_FLIGHT_PER_PEER;

/// the estimate of a peer that defers its replies (only toward one does the window grow past one request) unless
/// @p peer_defers_replies is false, after many transactions of @p size
transaction_size_estimate settled_at( size_t size, bool peer_defers_replies = true )
{
  transaction_size_estimate estimate;
  estimate.set_peer_defers_replies( peer_defers_replies );
  for( int i = 0; i < 1000; ++i )
    estimate.record( size );
  return estimate;
}

/// how many transactions are outstanding once the peer is asked for as many as allowed, again and again, until it may be asked for none
size_t fill_window( const transaction_size_estimate& estimate )
{
  size_t in_flight = 0;
  for( size_t requested = estimate.transactions_to_request( 0 ); requested > 0; requested = estimate.transactions_to_request( in_flight ) )
    in_flight += requested;
  return in_flight;
}

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

/// A peer sending small transactions may have more outstanding than one request asks for: a second full request
/// while the first is outstanding, up to the in-flight item limit, with the expected bytes of all of them within
/// the byte limit.
BOOST_AUTO_TEST_CASE( small_transactions_pipeline_requests_up_to_item_window )
{
  BOOST_CHECK_EQUAL( max_in_flight, 2 * max_transactions );
  const transaction_size_estimate estimate = settled_at( 150 );
  BOOST_REQUIRE_EQUAL( estimate.max_transactions_per_request(), max_transactions );
  BOOST_CHECK_EQUAL( estimate.transactions_to_request( 0 ), max_in_flight );
  BOOST_CHECK_EQUAL( estimate.transactions_to_request( max_transactions ), max_transactions );
  BOOST_CHECK_EQUAL( estimate.transactions_to_request( max_in_flight - 10 ), 0u ); // less than half a request of room
  BOOST_CHECK_EQUAL( estimate.transactions_to_request( max_in_flight ), 0u );
  BOOST_CHECK_EQUAL( estimate.transactions_to_request( max_in_flight + 1 ), 0u );
  BOOST_CHECK_EQUAL( fill_window( estimate ), max_in_flight );
  BOOST_CHECK_LE( fill_window( estimate ) * estimate.mean(), max_bytes_in_flight );
}

/// With large transactions the byte limit bounds all outstanding requests together, not each one, though a peer
/// may always be asked for one full request: its send queue holds no more of our replies than with one request
/// at a time.
BOOST_AUTO_TEST_CASE( byte_limit_bounds_all_outstanding_requests )
{
  BOOST_CHECK_LT( max_bytes_in_flight, size_t( GRAPHENE_NET_MAXIMUM_QUEUED_MESSAGES_IN_BYTES ) );
  const size_t sizes[] = { 300, 1024, 4096, 16384, HIVE_MAX_TRANSACTION_SIZE };
  for( size_t size : sizes )
  {
    const transaction_size_estimate estimate = settled_at( size );
    const size_t window = fill_window( estimate );
    const size_t per_request = estimate.max_transactions_per_request();
    BOOST_CHECK_GE( window, per_request );
    if( window > per_request )
      BOOST_CHECK_LE( window * estimate.mean(), max_bytes_in_flight );
    BOOST_CHECK_EQUAL( estimate.transactions_to_request( window ), 0u );
  }
  // 4 KiB transactions: one full request takes the whole byte window
  const transaction_size_estimate estimate = settled_at( 4096 );
  BOOST_CHECK_EQUAL( estimate.transactions_to_request( estimate.max_transactions_per_request() ), 0u );
}

/// While some are outstanding, the next request waits for room for half a request; an idle peer is asked at once.
BOOST_AUTO_TEST_CASE( busy_peer_is_asked_again_only_for_half_a_request )
{
  const transaction_size_estimate estimate = settled_at( 4096 );
  const size_t per_request = estimate.max_transactions_per_request();
  BOOST_REQUIRE_GT( per_request, 2u );
  BOOST_CHECK_EQUAL( estimate.transactions_to_request( 0 ), per_request );
  BOOST_CHECK_EQUAL( estimate.transactions_to_request( per_request - 1 ), 0u );
  BOOST_CHECK_EQUAL( estimate.transactions_to_request( per_request / 2 + 1 ), 0u );
  BOOST_CHECK_EQUAL( estimate.transactions_to_request( per_request / 2 ), per_request - per_request / 2 );

  // a peer sending transactions larger than the whole byte limit has one outstanding at most
  const transaction_size_estimate huge = settled_at( 2 * max_bytes );
  BOOST_CHECK_EQUAL( huge.transactions_to_request( 0 ), 1u );
  BOOST_CHECK_EQUAL( huge.transactions_to_request( 1 ), 0u );
}

/// A mix settled at 300 B fills the whole item window: two full requests per round trip.  When the transactions
/// then turn out to be 8 KB each, the outstanding ones expect several times the peer's send queue limit, which the
/// peer answers as its queue drains (peer_connection::send_transaction_reply()); the window shrinks to one request
/// within the byte bound as the replies arrive.
BOOST_AUTO_TEST_CASE( window_shrinks_to_the_byte_bound_after_size_jump )
{
  const size_t new_size = 8192;
  transaction_size_estimate estimate = settled_at( 300 );
  BOOST_CHECK_EQUAL( estimate.max_transactions_per_request(), max_transactions );
  size_t in_flight = fill_window( estimate );
  BOOST_CHECK_EQUAL( in_flight, max_in_flight );
  BOOST_CHECK_GT( in_flight * new_size, size_t( GRAPHENE_NET_MAXIMUM_QUEUED_MESSAGES_IN_BYTES ) );

  for( int reply = 0; reply < 3000; ++reply )
  {
    BOOST_REQUIRE_GT( in_flight, 0u );
    --in_flight;
    estimate.record( new_size );
    in_flight += estimate.transactions_to_request( in_flight );
  }
  BOOST_CHECK_EQUAL( fill_window( estimate ), estimate.max_transactions_per_request() );
  BOOST_CHECK_LE( in_flight * new_size, max_bytes_in_flight );
}

/// Before the peer sent a transaction its window is one request of the largest transactions.
BOOST_AUTO_TEST_CASE( new_peer_window_is_one_request )
{
  const transaction_size_estimate estimate;
  BOOST_CHECK_EQUAL( fill_window( estimate ), estimate.max_transactions_per_request() );
}

/// Toward a peer that doesn't say it defers its replies, which queues each reply at once, a mix settled at 300 B keeps
/// one full request outstanding at most, as before requests were pipelined, so its queue overflows on a size jump no
/// sooner than then; toward one that does, the whole item window.
BOOST_AUTO_TEST_CASE( window_is_one_request_unless_the_peer_defers_replies )
{
  const transaction_size_estimate old_peer = settled_at( 300, false );
  BOOST_CHECK( !old_peer.peer_defers_replies() );
  const size_t per_request = old_peer.max_transactions_per_request();
  BOOST_REQUIRE_EQUAL( per_request, max_transactions );
  BOOST_CHECK_EQUAL( fill_window( old_peer ), per_request );
  for( size_t in_flight = 0; in_flight <= per_request + 1; ++in_flight )
    BOOST_CHECK_LE( in_flight + old_peer.transactions_to_request( in_flight ), std::max( in_flight, per_request ) );

  const transaction_size_estimate deferring_peer = settled_at( 300, true );
  BOOST_CHECK( deferring_peer.peer_defers_replies() );
  BOOST_CHECK_EQUAL( fill_window( deferring_peer ), max_in_flight );

  BOOST_CHECK( !transaction_size_estimate().peer_defers_replies() );
}

/// An outstanding transaction times out only once the peer has delivered none it was asked for within the timeout,
/// however long ago it was requested, up to the ceiling, which ends well within the life of the records of who else
/// advertised it.
BOOST_AUTO_TEST_CASE( transaction_request_times_out_on_lack_of_progress )
{
  const fc::microseconds timeout = fc::seconds( 6 );
  const fc::time_point start = fc::time_point::now();
  const auto at = [&]( double seconds ) { return start + fc::microseconds( int64_t( seconds * 1000000 ) ); };
  const item_id transaction( graphene::net::trx_message_type, fc::ripemd160::hash( std::string( "slow" ) ) );
  const double ceiling = 6.0 * GRAPHENE_NET_MAX_TRANSACTION_REQUEST_TIMEOUTS_WHILE_DELIVERING;
  BOOST_CHECK_LE( 2 * ceiling, 60.0 * GRAPHENE_NET_MAX_INVENTORY_SIZE_IN_MINUTES );

  requested_items requested;
  requested.insert( transaction, start );
  BOOST_CHECK( !requested.timed_out( at( 5.9 ), timeout ) );
  BOOST_CHECK( requested.timed_out( at( 6.1 ), timeout ) == transaction ); // nothing delivered: as before

  // deliveries of other requested transactions, every second, keep it waiting past the timeout
  for( int second = 1; second < ceiling; ++second )
  {
    requested.record_transaction_delivery( at( second ) );
    BOOST_CHECK( !requested.timed_out( at( second + 0.5 ), timeout ) );
  }
  // until the deliveries stop for the timeout
  const double last_reply = 10;
  requested_items stopped;
  stopped.insert( transaction, start );
  stopped.record_transaction_delivery( at( last_reply ) );
  BOOST_CHECK( !stopped.timed_out( at( last_reply + 5.9 ), timeout ) );
  BOOST_CHECK( stopped.timed_out( at( last_reply + 6.1 ), timeout ) == transaction );

  // or the ceiling passes since it was requested
  requested.record_transaction_delivery( at( ceiling + 0.05 ) );
  BOOST_CHECK( requested.timed_out( at( ceiling + 0.1 ), timeout ) == transaction );
}

/// A block times out the timeout after it was requested, however steadily the peer meanwhile answers transactions asked
/// for before it, with their bodies or as not available, so a peer that advertised a block first cannot hold it past
/// the timeout while the others that advertised it wait.
BOOST_AUTO_TEST_CASE( block_request_times_out_from_its_request_whatever_the_peer_answers )
{
  const fc::microseconds timeout = fc::seconds( 6 );
  const fc::time_point start = fc::time_point::now();
  const auto at = [&]( double seconds ) { return start + fc::microseconds( int64_t( seconds * 1000000 ) ); };
  const item_id block( graphene::net::block_message_type, fc::ripemd160::hash( std::string( "behind" ) ) );
  const auto transaction = []( int i ) { return item_id( graphene::net::trx_message_type, fc::ripemd160::hash( "earlier " + std::to_string( i ) ) ); };
  const fc::time_point earlier = at( -1 );

  requested_items behind_replies;
  for( int i = 0; i < 12; ++i )
    behind_replies.insert( transaction( i ), earlier );
  behind_replies.insert( block, start );
  // a body, then one not available, every half second
  for( int i = 0; i < 12; ++i )
  {
    const double now = 0.5 * ( i + 1 );
    behind_replies.erase( behind_replies.find( transaction( i ) ) );
    if( i % 2 == 0 )
      behind_replies.record_transaction_delivery( at( now ) );
    BOOST_CHECK( !behind_replies.timed_out( at( now ), timeout ) );
  }
  BOOST_CHECK_EQUAL( behind_replies.size(), 1u );
  BOOST_CHECK( !behind_replies.timed_out( at( 5.9 ), timeout ) );
  BOOST_CHECK( behind_replies.timed_out( at( 6.1 ), timeout ) == block );

  // nor does a transaction outstanding with the block, delivered steadily or not, extend it
  requested_items with_transactions;
  with_transactions.insert( block, start );
  with_transactions.insert( transaction( 0 ), earlier );
  for( int second = 1; second <= 6; ++second )
    with_transactions.record_transaction_delivery( at( second ) );
  BOOST_CHECK( with_transactions.timed_out( at( 6.1 ), timeout ) == block );
}

/// The requested items count their transactions, which is what the window is judged on, and the blocks apart.
BOOST_AUTO_TEST_CASE( requested_items_count_transactions_and_blocks )
{
  requested_items requested;
  const item_id transaction_1( graphene::net::trx_message_type, fc::ripemd160::hash( std::string( "1" ) ) );
  const item_id transaction_2( graphene::net::trx_message_type, fc::ripemd160::hash( std::string( "2" ) ) );
  const item_id block( graphene::net::block_message_type, fc::ripemd160::hash( std::string( "1" ) ) );
  const fc::time_point now = fc::time_point::now();

  requested.insert( transaction_1, now );
  requested.insert( transaction_2, now );
  requested.insert( transaction_2, now ); // already outstanding
  requested.insert( block, now );
  BOOST_CHECK_EQUAL( requested.size(), 3u );
  BOOST_CHECK_EQUAL( requested.transactions(), 2u );
  BOOST_CHECK_EQUAL( requested.blocks(), 1u );

  requested.erase( requested.find( block ) );
  BOOST_CHECK_EQUAL( requested.transactions(), 2u );
  BOOST_CHECK_EQUAL( requested.blocks(), 0u );
  requested.erase( requested.find( transaction_1 ) );
  BOOST_CHECK_EQUAL( requested.transactions(), 1u );
  BOOST_CHECK( requested.find( transaction_1 ) == requested.end() );
  BOOST_CHECK( requested.find( transaction_2 ) != requested.end() );
  BOOST_CHECK( requested.find( transaction_2 )->second == now );
  requested.erase( requested.find( transaction_2 ) );
  BOOST_CHECK( requested.empty() );
  BOOST_CHECK_EQUAL( requested.transactions(), 0u );
}

BOOST_AUTO_TEST_SUITE_END()
