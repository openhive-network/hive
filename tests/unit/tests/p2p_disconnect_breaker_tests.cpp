#include <boost/test/unit_test.hpp>

#include <graphene/net/config.hpp>
#include <graphene/net/node.hpp>
#include <graphene/net/peer_connection.hpp>

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

using graphene::net::block_copy_grace;
using graphene::net::block_download_holds_rerequest_until;
using graphene::net::block_request_deadline;
using graphene::net::block_source_candidate;
using graphene::net::choose_block_source;
using graphene::net::disconnect_breaker;
using graphene::net::message_in_progress;
using graphene::net::peer_connection;
using graphene::net::unavailable_item_strikes;

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

/// Of two peers that advertised a block, one with transactions outstanding and one idle, the idle one is asked
/// whichever comes first: the other would send the block only after the replies it owes us.
BOOST_AUTO_TEST_CASE( block_source_prefers_idle_peer_over_one_with_transactions_outstanding )
{
  const block_source_candidate with_transactions{ false, true, false };
  const block_source_candidate idle{ false, true, true };
  BOOST_CHECK( choose_block_source( { with_transactions, idle } ) == std::optional<size_t>( 1 ) );
  BOOST_CHECK( choose_block_source( { idle, with_transactions } ) == std::optional<size_t>( 0 ) );
  BOOST_CHECK( choose_block_source( { with_transactions, with_transactions, idle } ) == std::optional<size_t>( 2 ) );
}

/// Without an idle advertiser that can take the request, a peer with transactions outstanding is still asked rather
/// than the block waiting for one to go idle; a deprioritized idle peer doesn't count as one.
BOOST_AUTO_TEST_CASE( block_source_asks_peer_with_transactions_outstanding_when_no_idle_one_can_be )
{
  const block_source_candidate with_transactions{ false, true, false };
  const block_source_candidate blocked{ false, false, false };
  const block_source_candidate deprioritized_idle{ true, true, true };
  BOOST_CHECK( choose_block_source( { with_transactions } ) == std::optional<size_t>( 0 ) );
  BOOST_CHECK( choose_block_source( { blocked, with_transactions } ) == std::optional<size_t>( 1 ) );
  BOOST_CHECK( choose_block_source( { deprioritized_idle, with_transactions } ) == std::optional<size_t>( 1 ) );
}

/// A peer that delivered us a block before is asked before one that never did, idle or not: a peer may advertise
/// blocks only to be asked for them and withhold them.  Among peers alike in that, the idle one still goes first, and a
/// deprioritized peer still only when no other peer advertised the block
BOOST_AUTO_TEST_CASE( block_source_prefers_peer_that_delivered_a_block )
{
  const block_source_candidate unproven_idle{ false, true, true, false };
  const block_source_candidate proven_idle{ false, true, true, true };
  const block_source_candidate proven_with_transactions{ false, true, false, true };
  const block_source_candidate proven_blocked{ false, false, true, true };
  const block_source_candidate proven_deprioritized{ true, true, true, true };
  BOOST_CHECK( choose_block_source( { unproven_idle, proven_with_transactions } ) == std::optional<size_t>( 1 ) );
  BOOST_CHECK( choose_block_source( { unproven_idle, proven_with_transactions, proven_idle } ) == std::optional<size_t>( 2 ) );
  BOOST_CHECK( choose_block_source( { proven_idle, unproven_idle } ) == std::optional<size_t>( 0 ) );
  // a proven peer that can't take a request now doesn't hold the block back from one that can
  BOOST_CHECK( choose_block_source( { proven_blocked, unproven_idle } ) == std::optional<size_t>( 1 ) );
  BOOST_CHECK( choose_block_source( { proven_deprioritized, unproven_idle } ) == std::optional<size_t>( 1 ) );
}

/// The ranking in full: delivered and idle, delivered with transactions outstanding, never delivered and idle, never
/// delivered with transactions outstanding, whichever order they come in
BOOST_AUTO_TEST_CASE( block_source_ranks_delivered_then_idle )
{
  const block_source_candidate proven_idle{ false, true, true, true };
  const block_source_candidate proven_busy{ false, true, false, true };
  const block_source_candidate unproven_idle{ false, true, true, false };
  const block_source_candidate unproven_busy{ false, true, false, false };
  BOOST_CHECK( choose_block_source( { unproven_busy, unproven_idle, proven_busy, proven_idle } ) == std::optional<size_t>( 3 ) );
  BOOST_CHECK( choose_block_source( { unproven_busy, unproven_idle, proven_busy } ) == std::optional<size_t>( 2 ) );
  BOOST_CHECK( choose_block_source( { unproven_busy, unproven_idle } ) == std::optional<size_t>( 1 ) );
  BOOST_CHECK( choose_block_source( { unproven_busy } ) == std::optional<size_t>( 0 ) );
}

/// A peer that advertises only blocks is never asked for transactions, so it has nothing outstanding; it ranks as busy,
/// so it doesn't outrank a proven relay we keep busy with transaction requests, while an idle relay outranks it
BOOST_AUTO_TEST_CASE( block_only_advertiser_does_not_outrank_busy_relay )
{
  auto block_only = peer_connection::make_shared( nullptr );
  BOOST_REQUIRE( block_only->idle() );
  const fc::time_point now = fc::time_point::now();
  const block_source_candidate proven_block_only{ false, true, block_only->idle_block_source( now ), true };
  const block_source_candidate proven_busy_relay{ false, true, false, true };
  const block_source_candidate proven_idle_relay{ false, true, true, true };
  BOOST_CHECK( choose_block_source( { proven_busy_relay, proven_block_only } ) == std::optional<size_t>( 0 ) );
  BOOST_CHECK( choose_block_source( { proven_block_only, proven_idle_relay } ) == std::optional<size_t>( 1 ) );
}

/// A peer ranks as an idle block source only while nothing is outstanding with it and it advertised transactions to
/// us within the inventory window
BOOST_AUTO_TEST_CASE( idle_block_source_needs_recent_transaction_advertisements )
{
  auto peer = peer_connection::make_shared( nullptr );
  const fc::time_point now = fc::time_point::now();
  BOOST_CHECK( !peer->idle_block_source( now ) );

  peer->last_transaction_advertisement_time = now;
  BOOST_CHECK( peer->idle_block_source( now ) );
  const fc::microseconds window = fc::minutes( GRAPHENE_NET_MAX_INVENTORY_SIZE_IN_MINUTES );
  BOOST_CHECK( peer->idle_block_source( now + window - fc::microseconds( 1 ) ) );
  BOOST_CHECK( !peer->idle_block_source( now + window ) );

  peer->items_requested_from_peer.insert( graphene::net::item_id( graphene::net::trx_message_type, fc::ripemd160::hash( std::string( "t" ) ) ), now );
  BOOST_CHECK( !peer->idle_block_source( now ) );
}

/// A block's first request has 1 s to deliver, each later one twice the one before: three withholders in a row cost
/// 1 + 2 + 4 s
BOOST_AUTO_TEST_CASE( block_request_deadlines_double )
{
  static_assert( GRAPHENE_NET_BLOCK_REREQUEST_TIMEOUT_MS == 1000 );
  static_assert( GRAPHENE_NET_MAX_OUTSTANDING_REQUESTS_PER_BLOCK == 3 );
  BOOST_CHECK( block_request_deadline( 0 ) == fc::seconds( 1 ) );
  BOOST_CHECK( block_request_deadline( 1 ) == fc::seconds( 2 ) );
  BOOST_CHECK( block_request_deadline( 2 ) == fc::seconds( 4 ) );
}

/// A late copy restores priority up to two deadlines past its deadline, but not in the second half of the request
/// timeout, and never less than within the deadline
BOOST_AUTO_TEST_CASE( block_copy_grace_is_bounded_by_the_request_timeout )
{
  static_assert( GRAPHENE_NET_BLOCK_COPY_GRACE_DEADLINES == 2 );
  const fc::microseconds default_timeout = fc::seconds( 6 );
  BOOST_CHECK( block_copy_grace( fc::seconds( 1 ), default_timeout ) == fc::seconds( 3 ) );
  BOOST_CHECK( block_copy_grace( fc::seconds( 2 ), default_timeout ) == fc::seconds( 3 ) );
  BOOST_CHECK( block_copy_grace( fc::seconds( 4 ), default_timeout ) == fc::seconds( 4 ) );
  BOOST_CHECK( block_copy_grace( fc::seconds( 1 ), fc::seconds( 20 ) ) == fc::seconds( 3 ) );
  BOOST_CHECK( block_copy_grace( fc::seconds( 1 ), fc::seconds( 2 ) ) == fc::seconds( 1 ) );
}

/// Not-available strikes decay: three answers spread over 30 minutes leave the peer short of the limit, three
/// within a minute reach it
BOOST_AUTO_TEST_CASE( unavailable_item_strikes_decay )
{
  static_assert( GRAPHENE_NET_MAX_UNAVAILABLE_ITEM_STRIKES == 3 );
  unavailable_item_strikes spread;
  for( int minute : { 0, 15, 30 } )
  {
    const fc::time_point now = start + fc::minutes( minute );
    BOOST_CHECK( spread.record( now ) );
    BOOST_CHECK( !spread.exhausted( now ) );
  }

  unavailable_item_strikes close;
  BOOST_CHECK( close.record( start ) );
  BOOST_CHECK( close.record( start + fc::seconds( 25 ) ) );
  BOOST_CHECK( !close.exhausted( start + fc::seconds( 25 ) ) );
  BOOST_CHECK( close.record( start + fc::seconds( 50 ) ) );
  BOOST_CHECK( close.exhausted( start + fc::seconds( 50 ) ) );
}

/// Each strike is forgotten a decay interval after it was made, whenever the next one comes: strikes just under an
/// interval apart never add up to the limit, which they did when a new strike restarted the decay clock
BOOST_AUTO_TEST_CASE( unavailable_item_strikes_decay_continuously )
{
  const fc::microseconds decay = fc::seconds( GRAPHENE_NET_UNAVAILABLE_ITEM_STRIKE_DECAY_SEC );
  const fc::microseconds apart = decay - fc::seconds( 1 );
  unavailable_item_strikes strikes;
  for( int i = 0; i < 6; ++i )
  {
    const fc::time_point now = start + fc::microseconds( apart.count() * i );
    BOOST_CHECK( strikes.record( now ) );
    BOOST_CHECK_EQUAL( strikes.count( now ), uint32_t( std::min( i + 1, 2 ) ) );
    BOOST_CHECK( !strikes.exhausted( now ) );
  }

  // and the count reported between strikes is the decayed one
  unavailable_item_strikes reported;
  BOOST_CHECK( reported.record( start ) );
  BOOST_CHECK( reported.record( start + fc::minutes( 5 ) ) );
  BOOST_CHECK_EQUAL( reported.count( start + decay - fc::seconds( 1 ) ), 2u );
  BOOST_CHECK_EQUAL( reported.count( start + decay ), 1u );
  BOOST_CHECK_EQUAL( reported.count( start + fc::minutes( 5 ) + decay ), 0u );
}

/// Answers within a demotion of the strike they follow are no new strike; each strike is forgotten a decay interval
/// after it was made, and a delivery clears them all
BOOST_AUTO_TEST_CASE( unavailable_item_strikes_counting )
{
  const fc::microseconds demotion = fc::seconds( GRAPHENE_NET_UNAVAILABLE_ITEM_DEMOTION_SEC );
  const fc::microseconds decay = fc::seconds( GRAPHENE_NET_UNAVAILABLE_ITEM_STRIKE_DECAY_SEC );
  unavailable_item_strikes strikes;
  BOOST_CHECK( strikes.record( start ) );
  BOOST_CHECK( !strikes.record( start + demotion - fc::microseconds( 1 ) ) );
  BOOST_CHECK_EQUAL( strikes.count( start + demotion - fc::microseconds( 1 ) ), 1u );
  BOOST_CHECK( strikes.record( start + demotion ) );
  BOOST_CHECK_EQUAL( strikes.count( start + demotion ), 2u );
  // an interval after the first: that one forgotten, one new
  BOOST_CHECK( strikes.record( start + decay ) );
  BOOST_CHECK_EQUAL( strikes.count( start + decay ), 2u );
  const fc::microseconds twice( decay.count() * 2 );
  // an interval after the last: all forgotten, one new
  BOOST_CHECK( strikes.record( start + twice ) );
  BOOST_CHECK_EQUAL( strikes.count( start + twice ), 1u );

  strikes.clear();
  BOOST_CHECK_EQUAL( strikes.count( start + twice ), 0u );
}

/// A block message arriving at a rate that completes it within the request timeout holds off asking another peer
/// until a re-request deadline after its bytes last arrived; one arriving slower holds nothing
BOOST_AUTO_TEST_CASE( block_download_holds_rerequest_while_on_time )
{
  const fc::microseconds request_timeout = fc::seconds( 6 );
  const fc::microseconds rerequest = fc::milliseconds( GRAPHENE_NET_BLOCK_REREQUEST_TIMEOUT_MS );
  const fc::time_point sent = start;
  message_in_progress download;
  download.msg_type = graphene::net::block_message_type;
  download.bytes_total = 2 * 1024 * 1024;
  download.started = sent + fc::milliseconds( 200 );

  // a quarter in 1 s: all of it 4.2 s after the request
  download.bytes_read = download.bytes_total / 4;
  download.last_progress = download.started + fc::seconds( 1 );
  BOOST_CHECK( block_download_holds_rerequest_until( download, sent, request_timeout ) == download.last_progress + rerequest );

  // a tenth in 1 s: 10.2 s, past the timeout
  download.bytes_read = download.bytes_total / 10;
  BOOST_CHECK( block_download_holds_rerequest_until( download, sent, request_timeout ) == fc::time_point() );

  // just the header so far
  download.bytes_read = 16;
  download.last_progress = download.started;
  BOOST_CHECK( block_download_holds_rerequest_until( download, sent, request_timeout ) == download.started + rerequest );
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
