#include <boost/test/unit_test.hpp>

#include <graphene/net/config.hpp>
#include <graphene/net/node.hpp>
#include <graphene/net/peer_connection.hpp>

#include <cstdint>
#include <optional>
#include <vector>

using graphene::net::block_source_candidate;
using graphene::net::choose_block_source;
using graphene::net::disconnect_breaker;
using graphene::net::peer_connection;

namespace {

const fc::time_point start = fc::time_point::from_iso_string( "2026-11-09T00:00:00" );

disconnect_breaker invalid_block_breaker()
{
  return disconnect_breaker( GRAPHENE_NET_INVALID_BLOCK_DISCONNECT_BREAKER_COUNT,
                             fc::seconds( GRAPHENE_NET_INVALID_BLOCK_DISCONNECT_BREAKER_WINDOW_SEC ) );
}

disconnect_breaker block_body_mismatch_breaker()
{
  return disconnect_breaker( GRAPHENE_NET_BLOCK_BODY_MISMATCH_DISCONNECT_BREAKER_COUNT,
                             fc::seconds( GRAPHENE_NET_BLOCK_BODY_MISMATCH_DISCONNECT_BREAKER_WINDOW_SEC ) );
}

} // namespace

BOOST_AUTO_TEST_SUITE( p2p_disconnect_breaker_tests )

/// Up to the threshold every disconnect goes ahead; beyond it, within the window, they are refused.
BOOST_AUTO_TEST_CASE( trips_after_threshold_within_window )
{
  disconnect_breaker breaker = block_body_mismatch_breaker();
  for( uint32_t i = 0; i < GRAPHENE_NET_BLOCK_BODY_MISMATCH_DISCONNECT_BREAKER_COUNT; ++i )
    BOOST_CHECK( !breaker.tripped( start + fc::seconds( i ) ) );
  BOOST_CHECK( breaker.tripped( start + fc::seconds( GRAPHENE_NET_BLOCK_BODY_MISMATCH_DISCONNECT_BREAKER_COUNT ) ) );
  BOOST_CHECK( breaker.tripped( start + fc::seconds( GRAPHENE_NET_BLOCK_BODY_MISMATCH_DISCONNECT_BREAKER_COUNT + 1 ) ) );
}

/// Refused disconnects are not recorded, so once the recorded ones leave the window disconnects resume.
BOOST_AUTO_TEST_CASE( resets_when_window_drains )
{
  disconnect_breaker breaker = block_body_mismatch_breaker();
  for( uint32_t i = 0; i < GRAPHENE_NET_BLOCK_BODY_MISMATCH_DISCONNECT_BREAKER_COUNT; ++i )
    BOOST_CHECK( !breaker.tripped( start ) );
  BOOST_CHECK( breaker.tripped( start + fc::seconds( GRAPHENE_NET_BLOCK_BODY_MISMATCH_DISCONNECT_BREAKER_WINDOW_SEC ) ) );
  BOOST_CHECK( !breaker.tripped( start + fc::seconds( GRAPHENE_NET_BLOCK_BODY_MISMATCH_DISCONNECT_BREAKER_WINDOW_SEC ) + fc::microseconds( 1 ) ) );
}

/// A burst of block-body mismatches trips only its own breaker: the invalid-block breaker, kept separately by
/// the node, still has its whole budget.
BOOST_AUTO_TEST_CASE( breakers_count_independently )
{
  disconnect_breaker mismatch_breaker = block_body_mismatch_breaker();
  disconnect_breaker invalid_breaker = invalid_block_breaker();
  for( uint32_t i = 0; i < GRAPHENE_NET_BLOCK_BODY_MISMATCH_DISCONNECT_BREAKER_COUNT; ++i )
    BOOST_CHECK( !mismatch_breaker.tripped( start ) );
  BOOST_CHECK( mismatch_breaker.tripped( start ) );

  for( uint32_t i = 0; i < GRAPHENE_NET_INVALID_BLOCK_DISCONNECT_BREAKER_COUNT; ++i )
    BOOST_CHECK( !invalid_breaker.tripped( start ) );
  BOOST_CHECK( invalid_breaker.tripped( start ) );
}

/// Of a deprioritized and a normal peer that both advertised a block, the normal one is asked, wherever it ranks.
BOOST_AUTO_TEST_CASE( block_source_prefers_peer_not_deprioritized )
{
  const std::vector<block_source_candidate> candidates{ { true, true }, { false, true } };
  BOOST_CHECK( choose_block_source( candidates ) == std::optional<size_t>( 1 ) );

  const std::vector<block_source_candidate> reversed{ { false, true }, { true, true } };
  BOOST_CHECK( choose_block_source( reversed ) == std::optional<size_t>( 0 ) );
}

/// Among peers that aren't deprioritized the load order is kept: the first available one is asked.
BOOST_AUTO_TEST_CASE( block_source_keeps_load_order )
{
  const std::vector<block_source_candidate> candidates{ { true, true }, { false, false }, { false, true }, { false, true } };
  BOOST_CHECK( choose_block_source( candidates ) == std::optional<size_t>( 2 ) );
}

/// A deprioritized peer that is the only one to have advertised the block is still asked: the node is never isolated.
BOOST_AUTO_TEST_CASE( block_source_falls_back_to_only_advertiser )
{
  const std::vector<block_source_candidate> only{ { true, true } };
  BOOST_CHECK( choose_block_source( only ) == std::optional<size_t>( 0 ) );

  const std::vector<block_source_candidate> several{ { true, false }, { true, true }, { true, true } };
  BOOST_CHECK( choose_block_source( several ) == std::optional<size_t>( 1 ) );
}

/// While a normal peer that advertised the block can't take a request yet, the block waits for it rather than
/// going to a deprioritized peer.
BOOST_AUTO_TEST_CASE( block_source_waits_for_busy_preferred_peer )
{
  const std::vector<block_source_candidate> candidates{ { true, true }, { false, false } };
  BOOST_CHECK( !choose_block_source( candidates ) );
}

/// Nobody to ask: no advertiser, or only advertisers that can't take a request.
BOOST_AUTO_TEST_CASE( block_source_none_available )
{
  BOOST_CHECK( !choose_block_source( {} ) );
  const std::vector<block_source_candidate> busy{ { true, false } };
  BOOST_CHECK( !choose_block_source( busy ) );
}

/// A peer kept despite a body mismatch is deprioritized until it delivers a block we accept.
BOOST_AUTO_TEST_CASE( deprioritization_ends_when_peer_delivers_good_block )
{
  auto peer = peer_connection::make_shared( nullptr );
  BOOST_CHECK( !peer->is_block_fetching_deprioritized() );

  peer->deprioritize_block_fetching();
  BOOST_CHECK( peer->is_block_fetching_deprioritized() );

  peer->restore_block_fetching_priority();
  BOOST_CHECK( !peer->is_block_fetching_deprioritized() );
}

/// Without a good block from the peer, the deprioritization lasts for the breaker's window and then lapses.
BOOST_AUTO_TEST_CASE( deprioritization_lapses_after_breaker_window )
{
  auto peer = peer_connection::make_shared( nullptr );
  const fc::time_point before = fc::time_point::now();
  peer->deprioritize_block_fetching();
  const fc::time_point after = fc::time_point::now();

  const fc::microseconds window = fc::seconds( GRAPHENE_NET_BLOCK_BODY_MISMATCH_DISCONNECT_BREAKER_WINDOW_SEC );
  BOOST_CHECK( peer->block_fetching_deprioritized_until >= before + window );
  BOOST_CHECK( peer->block_fetching_deprioritized_until <= after + window );

  peer->block_fetching_deprioritized_until = fc::time_point::now() - fc::microseconds( 1 );
  BOOST_CHECK( !peer->is_block_fetching_deprioritized() );
}

BOOST_AUTO_TEST_SUITE_END()
