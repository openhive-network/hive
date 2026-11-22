#include <boost/test/unit_test.hpp>

#include <graphene/net/node.hpp>

#include <vector>

using graphene::net::active_connection_limits;
using graphene::net::active_connection_state;
using graphene::net::active_connection_verdict;
using graphene::net::breaker_stall_override_due;
using graphene::net::fetch_loop_wait;
using graphene::net::group_fetch_requests;
using graphene::net::handshaking_connection_state;
using graphene::net::handshaking_connection_verdict;
using graphene::net::item_hash_t;
using graphene::net::item_id;
using graphene::net::judge_active_connection;
using graphene::net::judge_handshaking_connection;
using graphene::net::next_fetch_pass;
using graphene::net::pending_transaction_pool_size;
using graphene::net::transaction_fetch_gate;
using graphene::net::transaction_fetch_window;

namespace {

const fc::time_point now = fc::time_point::from_iso_string( "2026-11-22T00:00:00" );

fc::time_point seconds_ago( int64_t seconds )
{
  return now - fc::seconds( seconds );
}

const active_connection_limits limits{ now, fc::seconds( 30 ), fc::seconds( 15 ), fc::seconds( 6 ), fc::seconds( 60 ) };

/// a peer connected for an hour that sent us a message a second ago, with nothing outstanding or stalled
active_connection_state healthy_peer()
{
  active_connection_state state;
  state.initiated = seconds_ago( 3600 );
  state.last_message_received = seconds_ago( 1 );
  return state;
}

/// a peer for which every reason holds
active_connection_state peer_failing_everything()
{
  active_connection_state state;
  state.initiated = seconds_ago( 3600 );
  state.last_message_received = seconds_ago( 3600 );
  state.last_sync_item_received = seconds_ago( 3600 );
  state.sync_item_ids_requested = seconds_ago( 3600 );
  state.ignored_item = item_id( graphene::net::trx_message_type, item_hash_t() );
  state.send_progress = seconds_ago( 3600 );
  state.sync_idle = true;
  return state;
}

item_id block( uint8_t n )
{
  item_hash_t hash;
  hash._hash[ 0 ] = n;
  return item_id( graphene::net::block_message_type, hash );
}

item_id transaction( uint8_t n )
{
  item_hash_t hash;
  hash._hash[ 0 ] = n;
  return item_id( graphene::net::trx_message_type, hash );
}

/// pool sizes against a max-mempool-size of 300 bytes, whose gate threshold is 100
const pending_transaction_pool_size pool_below_threshold{ 0, 300 };
const pending_transaction_pool_size pool_at_threshold{ 100, 300 };
const pending_transaction_pool_size pool_over_threshold{ 200, 300 };

} // namespace

BOOST_AUTO_TEST_SUITE( p2p_connection_policy_tests )

/// A healthy peer is kept
BOOST_AUTO_TEST_CASE( healthy_active_connection_kept )
{
  BOOST_CHECK( judge_active_connection( healthy_peer(), limits ) == active_connection_verdict::keep );
}

/// The reasons are ranked: removing the one that holds reveals the next, in the documented order
BOOST_AUTO_TEST_CASE( active_connection_reasons_ranked )
{
  active_connection_state state = peer_failing_everything();
  BOOST_CHECK( judge_active_connection( state, limits ) == active_connection_verdict::inactive );

  state.last_message_received = seconds_ago( 20 ); // quiet, but not inactive
  BOOST_CHECK( judge_active_connection( state, limits ) == active_connection_verdict::sync_items_ignored );

  state.last_sync_item_received.reset();
  BOOST_CHECK( judge_active_connection( state, limits ) == active_connection_verdict::sync_item_ids_ignored );

  state.sync_item_ids_requested.reset();
  BOOST_CHECK( judge_active_connection( state, limits ) == active_connection_verdict::item_request_ignored );

  state.ignored_item.reset();
  BOOST_CHECK( judge_active_connection( state, limits ) == active_connection_verdict::send_stalled );

  state.send_progress = fc::time_point::maximum();
  BOOST_CHECK( judge_active_connection( state, limits ) == active_connection_verdict::send_keepalive );

  state.last_message_received = seconds_ago( 1 );
  BOOST_CHECK( judge_active_connection( state, limits ) == active_connection_verdict::sync_stalled );

  state.sync_idle = false;
  BOOST_CHECK( judge_active_connection( state, limits ) == active_connection_verdict::keep );
}

/// Each timed reason holds only past its own timeout
BOOST_AUTO_TEST_CASE( active_connection_reasons_hold_only_past_their_timeouts )
{
  active_connection_state state = healthy_peer();
  state.last_message_received = seconds_ago( 30 );
  BOOST_CHECK( judge_active_connection( state, limits ) == active_connection_verdict::send_keepalive );
  state.last_message_received = seconds_ago( 31 );
  BOOST_CHECK( judge_active_connection( state, limits ) == active_connection_verdict::inactive );

  state = healthy_peer();
  state.last_sync_item_received = seconds_ago( 6 );
  BOOST_CHECK( judge_active_connection( state, limits ) == active_connection_verdict::keep );
  state.last_sync_item_received = seconds_ago( 7 );
  BOOST_CHECK( judge_active_connection( state, limits ) == active_connection_verdict::sync_items_ignored );

  state = healthy_peer();
  state.sync_item_ids_requested = seconds_ago( 6 );
  BOOST_CHECK( judge_active_connection( state, limits ) == active_connection_verdict::keep );
  state.sync_item_ids_requested = seconds_ago( 7 );
  BOOST_CHECK( judge_active_connection( state, limits ) == active_connection_verdict::sync_item_ids_ignored );

  // the slow-reader rule: nothing written for 60 s while something waits to be sent
  state = healthy_peer();
  state.send_progress = seconds_ago( 60 );
  BOOST_CHECK( judge_active_connection( state, limits ) == active_connection_verdict::keep );
  state.send_progress = seconds_ago( 61 );
  BOOST_CHECK( judge_active_connection( state, limits ) == active_connection_verdict::send_stalled );
}

/// A new connection is neither inactive, quiet nor stalled in sync, whatever it sent
BOOST_AUTO_TEST_CASE( new_active_connection_not_judged_idle )
{
  active_connection_state state = healthy_peer();
  state.initiated = seconds_ago( 10 );
  state.last_message_received = seconds_ago( 3600 );
  state.sync_idle = true;
  BOOST_CHECK( judge_active_connection( state, limits ) == active_connection_verdict::keep );
}

/// The handshake deadline holds whatever the connection sent; inactivity needs both directions quiet
BOOST_AUTO_TEST_CASE( handshaking_connection_verdicts )
{
  const fc::microseconds inactivity = fc::seconds( 5 );
  const fc::microseconds deadline = fc::seconds( 15 );
  BOOST_CHECK( judge_handshaking_connection( { seconds_ago( 16 ), seconds_ago( 0 ), seconds_ago( 0 ) }, now, inactivity, deadline ) ==
               handshaking_connection_verdict::past_deadline );
  BOOST_CHECK( judge_handshaking_connection( { seconds_ago( 10 ), seconds_ago( 6 ), seconds_ago( 6 ) }, now, inactivity, deadline ) ==
               handshaking_connection_verdict::inactive );
  BOOST_CHECK( judge_handshaking_connection( { seconds_ago( 10 ), seconds_ago( 6 ), seconds_ago( 1 ) }, now, inactivity, deadline ) ==
               handshaking_connection_verdict::keep );
  BOOST_CHECK( judge_handshaking_connection( { seconds_ago( 4 ), seconds_ago( 4 ), seconds_ago( 4 ) }, now, inactivity, deadline ) ==
               handshaking_connection_verdict::keep );
}

/// The breaker override is due only once the head is known and has not advanced for more than 60 s
BOOST_AUTO_TEST_CASE( breaker_stall_override_due_after_a_minute_of_no_progress )
{
  BOOST_CHECK( !breaker_stall_override_due( 100, seconds_ago( 60 ), now ) );
  BOOST_CHECK( breaker_stall_override_due( 100, seconds_ago( 61 ), now ) );
  BOOST_CHECK( !breaker_stall_override_due( 0, seconds_ago( 61 ), now ) );
  BOOST_CHECK( !breaker_stall_override_due( 100, fc::time_point(), now ) );
}

/// A peer's window is closed while its fetching is inhibited or the transactions in flight are at the cap
BOOST_AUTO_TEST_CASE( transaction_fetch_window_closed_by_inhibition_and_in_flight_cap )
{
  BOOST_CHECK_EQUAL( transaction_fetch_window( false, 9999, 10000, 400 ), 400u );
  BOOST_CHECK_EQUAL( transaction_fetch_window( false, 10000, 10000, 400 ), 0u );
  BOOST_CHECK_EQUAL( transaction_fetch_window( false, 20000, 10000, 400 ), 0u );
  BOOST_CHECK_EQUAL( transaction_fetch_window( true, 0, 10000, 400 ), 0u );
  BOOST_CHECK_EQUAL( transaction_fetch_window( false, 1000000, 0, 400 ), 400u ); // 0: no cap
}

/// Requests follow the items' order, one per run of a type, transactions split at the per-request maximum
BOOST_AUTO_TEST_CASE( fetch_requests_grouped_by_type_and_size )
{
  const std::vector< item_id > items = { block( 1 ), block( 2 ), transaction( 1 ), transaction( 2 ), transaction( 3 ),
                                         transaction( 4 ), transaction( 5 ) };
  const auto requests = group_fetch_requests( items, 2 );
  BOOST_REQUIRE_EQUAL( requests.size(), 4u );
  BOOST_CHECK_EQUAL( requests[ 0 ].first, uint32_t( graphene::net::block_message_type ) );
  BOOST_CHECK_EQUAL( requests[ 0 ].second.size(), 2u ); // blocks are not split
  for( size_t i = 1; i < 4; ++i )
    BOOST_CHECK_EQUAL( requests[ i ].first, uint32_t( graphene::net::trx_message_type ) );
  BOOST_CHECK_EQUAL( requests[ 1 ].second.size(), 2u );
  BOOST_CHECK_EQUAL( requests[ 2 ].second.size(), 2u );
  BOOST_CHECK_EQUAL( requests[ 3 ].second.size(), 1u );
  BOOST_CHECK( requests[ 3 ].second[ 0 ] == transaction( 5 ).item_hash );
  BOOST_CHECK( group_fetch_requests( {}, 2 ).empty() );
}

/// An open gate that withheld nothing leaves the configured interval and the peers' unblock time alone
BOOST_AUTO_TEST_CASE( next_fetch_pass_with_open_gate )
{
  transaction_fetch_gate gate( pool_below_threshold, 0, 0 );
  BOOST_CHECK_EQUAL( gate.transactions_to_request( 5, []() { return true; } ), 5u );
  const fetch_loop_wait wait = next_fetch_pass( gate, now, fc::milliseconds( 5 ), now + fc::seconds( 1 ), now + fc::milliseconds( 100 ) );
  BOOST_CHECK( wait.minimum_interval == fc::milliseconds( 5 ) );
  BOOST_CHECK( wait.wake_time == now + fc::seconds( 1 ) );
}

/// A gate out of headroom that withheld transactions wakes the loop when the next minimum is due
BOOST_AUTO_TEST_CASE( next_fetch_pass_with_bounded_gate )
{
  transaction_fetch_gate gate( pool_at_threshold, 0, 0 );
  BOOST_CHECK_EQUAL( gate.transactions_to_request( 5, []() { return true; } ), 0u );
  BOOST_REQUIRE( gate.bounded() );
  const fetch_loop_wait wait = next_fetch_pass( gate, now, fc::milliseconds( 5 ), now + fc::seconds( 1 ), now + fc::milliseconds( 100 ) );
  BOOST_CHECK( wait.minimum_interval == fc::milliseconds( 5 ) );
  BOOST_CHECK( wait.wake_time == now + fc::milliseconds( 100 ) );
}

/// A closed gate keeps triggers from waking the loop before the next minimum is due
BOOST_AUTO_TEST_CASE( next_fetch_pass_with_closed_gate )
{
  transaction_fetch_gate gate( pool_over_threshold, 0, 0 );
  BOOST_CHECK_EQUAL( gate.transactions_to_request( 5, []() { return true; } ), 0u );
  BOOST_REQUIRE( gate.closed() && gate.held_back() );
  const fetch_loop_wait wait = next_fetch_pass( gate, now, fc::milliseconds( 5 ), now + fc::seconds( 1 ), now + fc::milliseconds( 100 ) );
  BOOST_CHECK( wait.minimum_interval == fc::milliseconds( 100 ) );
  BOOST_CHECK( wait.wake_time == now + fc::milliseconds( 100 ) );
}

BOOST_AUTO_TEST_SUITE_END()
