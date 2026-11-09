#include <boost/test/unit_test.hpp>

#include <graphene/net/config.hpp>
#include <graphene/net/node.hpp>

#include <cstdint>

using graphene::net::disconnect_breaker;

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

BOOST_AUTO_TEST_SUITE_END()
