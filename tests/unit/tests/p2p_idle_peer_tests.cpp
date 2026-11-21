#include <boost/test/unit_test.hpp>

#include <graphene/net/node.hpp>

#include <vector>

using graphene::net::idle_peer_candidate;
using graphene::net::idle_peer_sweep;
using graphene::net::select_idle_peers_to_disconnect;

namespace {

const fc::time_point now = fc::time_point::from_iso_string( "2026-11-21T00:00:00" );

idle_peer_sweep sweep( bool block_source_seen = true, size_t kept_at_least = 2 )
{
  idle_peer_sweep result;
  result.now = now;
  result.window = fc::seconds( 180 );
  result.kept_at_least = kept_at_least;
  result.block_source_seen = block_source_seen;
  return result;
}

idle_peer_candidate useful_seconds_ago( int64_t seconds, bool exempt = false )
{
  return { now - fc::seconds( seconds ), exempt };
}

} // namespace

BOOST_AUTO_TEST_SUITE( p2p_idle_peer_tests )

/// Peers idle for longer than the window are picked, the longest idle first; a peer just inside it is not
BOOST_AUTO_TEST_CASE( idle_peers_picked_longest_idle_first )
{
  const std::vector< idle_peer_candidate > peers = {
    useful_seconds_ago( 5 ), useful_seconds_ago( 200 ), useful_seconds_ago( 180 ), useful_seconds_ago( 400 ), useful_seconds_ago( 1 ) };
  const std::vector< size_t > picked = select_idle_peers_to_disconnect( peers, sweep() );
  BOOST_CHECK( picked == std::vector< size_t >( { 3, 1 } ) );
}

/// A peer fed by us, or one whose request or offered blocks are pending, is never picked however long it was idle
BOOST_AUTO_TEST_CASE( exempt_peers_kept )
{
  const std::vector< idle_peer_candidate > peers = {
    useful_seconds_ago( 1000, true ), useful_seconds_ago( 300 ), useful_seconds_ago( 1 ), useful_seconds_ago( 2 ) };
  BOOST_CHECK( select_idle_peers_to_disconnect( peers, sweep() ) == std::vector< size_t >( { 1 } ) );
}

/// When no peer gave us anything in the window and none is known to be ahead of us (a halted chain), nobody is picked
BOOST_AUTO_TEST_CASE( nobody_picked_without_a_block_source )
{
  const std::vector< idle_peer_candidate > peers = { useful_seconds_ago( 1000 ), useful_seconds_ago( 500 ), useful_seconds_ago( 300 ) };
  BOOST_CHECK( select_idle_peers_to_disconnect( peers, sweep( false ) ).empty() );
  BOOST_CHECK_EQUAL( select_idle_peers_to_disconnect( peers, sweep( true, 0 ) ).size(), 3u );
}

/// The picks leave kept_at_least of all peers, idle or not; the most recently useful idle peers are the ones kept
BOOST_AUTO_TEST_CASE( floor_of_kept_connections )
{
  std::vector< idle_peer_candidate > peers = { useful_seconds_ago( 1 ), useful_seconds_ago( 300 ), useful_seconds_ago( 900 ), useful_seconds_ago( 600 ) };
  BOOST_CHECK( select_idle_peers_to_disconnect( peers, sweep() ) == std::vector< size_t >( { 2, 3 } ) );

  peers = { useful_seconds_ago( 300 ), useful_seconds_ago( 900 ), useful_seconds_ago( 600 ) };
  BOOST_CHECK( select_idle_peers_to_disconnect( peers, sweep() ) == std::vector< size_t >( { 1 } ) );

  peers = { useful_seconds_ago( 300 ), useful_seconds_ago( 900 ) };
  BOOST_CHECK( select_idle_peers_to_disconnect( peers, sweep() ).empty() );
}

/// A window of 0 (idle_peer_disconnect_timeout 0) replaces nobody
BOOST_AUTO_TEST_CASE( zero_window_disables )
{
  idle_peer_sweep disabled = sweep( true, 0 );
  disabled.window = fc::microseconds();
  BOOST_CHECK( select_idle_peers_to_disconnect( { useful_seconds_ago( 1000 ) }, disabled ).empty() );
}

BOOST_AUTO_TEST_SUITE_END()
