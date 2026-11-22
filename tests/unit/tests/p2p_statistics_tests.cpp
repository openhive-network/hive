#include <boost/test/unit_test.hpp>

#include <graphene/net/config.hpp>
#include <graphene/net/core_messages.hpp>
#include <graphene/net/node.hpp>

#include <fc/crypto/ripemd160.hpp>

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <set>
#include <string>
#include <vector>

using graphene::net::fetch_candidate_order;
using graphene::net::fetch_candidate_queue;
using graphene::net::fetch_candidate_verdict;
using graphene::net::fetch_expiry_warning_limiter;
using graphene::net::fetch_minimum_schedule;
using graphene::net::fetch_turn_order;
using graphene::net::is_relay_sample;
using graphene::net::is_transaction_fetching_gated;
using graphene::net::item_id;
using graphene::net::message_hash_type;
using graphene::net::p2p_statistics;
using graphene::net::pending_transaction_pool_size;
using graphene::net::transaction_fetch_gate;
using graphene::net::transaction_fetch_gate_threshold;
using graphene::net::trx_message_type;

namespace {

message_hash_type hash_of( uint32_t i )
{
  return fc::ripemd160::hash( "transaction " + std::to_string( i ) );
}

uint64_t report_uint( const fc::variant_object& report, const char* group, const char* key )
{
  return report[group].get_object()[key].as_uint64();
}

double report_double( const fc::variant_object& report, const char* group, const char* key )
{
  return report[group].get_object()[key].as_double();
}

bool candidates() { return true; }
bool no_candidates() { return false; }

item_id transaction( uint32_t i ) { return item_id( trx_message_type, hash_of( i ) ); }

const fc::time_point queued_at = fc::time_point::from_iso_string( "2026-11-20T00:00:00" );

struct fetching_peer
{
  fetch_candidate_queue candidates;
  size_t requested = 0;
};

/// One pass of choosing transactions to request from @p peers (in connection order) as the node's fetch loop
/// runs it, each request expected to bring @p transaction_bytes; @p wanted holds the transactions still to fetch,
/// and a transaction requested from one peer leaves it, so the others drop it.  Returns what was requested, in turn
std::vector<message_hash_type> fetch_pass( std::vector<fetching_peer>& peers, fetch_turn_order& order, transaction_fetch_gate& gate,
                                           std::set<message_hash_type>& wanted, uint64_t transaction_bytes )
{
  std::vector<message_hash_type> requested;
  uint64_t examined = 0;
  const auto is_wanted = [&]( const item_id& candidate ) { return wanted.count( candidate.item_hash ) != 0; };
  std::vector<fetching_peer*> listed;
  std::vector<size_t> limits;
  for( fetching_peer& peer : peers )
  {
    order.visit( &peer, listed.size() );
    const size_t limit = peer.candidates.size() == 0 ? 0 :
      gate.transactions_to_request( 400, [&]() { return peer.candidates.has_transaction_to_take( is_wanted, examined ); } );
    if( limit > 0 )
    {
      peer.candidates.begin_pass();
      listed.push_back( &peer );
      limits.push_back( limit );
    }
  }
  std::vector<bool> done( listed.size(), false );
  std::vector<size_t> taken( listed.size(), 0 );
  order.take_turns( listed.size(), [&]( size_t turn )
  {
    if( done[ turn ] )
      return false;
    fetching_peer& peer = *listed[ turn ];
    const bool include_transactions =
      gate.may_request_more( [&]() { return peer.candidates.has_transaction_to_take( is_wanted, examined ); } );
    const fetch_candidate_order candidate_order = include_transactions && gate.next_request_is_minimum() ?
      fetch_candidate_order::newest_first : fetch_candidate_order::oldest_first;
    const std::optional<item_id> item = peer.candidates.take_next( include_transactions, candidate_order, [&]( const item_id& candidate )
      { return wanted.erase( candidate.item_hash ) != 0 ? fetch_candidate_verdict::take : fetch_candidate_verdict::drop; }, examined );
    if( !item )
    {
      done[ turn ] = true;
      return false;
    }
    gate.record_request( transaction_bytes );
    order.record_transaction( &peer );
    ++peer.requested;
    requested.push_back( item->item_hash );
    done[ turn ] = ++taken[ turn ] >= limits[ turn ];
    return true;
  } );
  return requested;
}

} // namespace

BOOST_AUTO_TEST_SUITE( p2p_statistics_tests )

/// The choice depends only on the message hash, so every node samples the same transactions, about one in N.
BOOST_AUTO_TEST_CASE( relay_samples_are_chosen_by_message_hash )
{
  const uint32_t transactions = 64000;
  uint32_t sampled = 0;
  for( uint32_t i = 0; i < transactions; ++i )
  {
    const message_hash_type hash = hash_of( i );
    const message_hash_type same_hash_on_another_node{ std::string( hash ) };
    BOOST_REQUIRE_EQUAL( is_relay_sample( hash ), is_relay_sample( same_hash_on_another_node ) );
    if( is_relay_sample( hash ) )
      ++sampled;
  }
  const uint32_t expected = transactions / GRAPHENE_NET_RELAY_SAMPLE_ONE_IN;
  BOOST_CHECK_GT( sampled, expected * 8 / 10 );
  BOOST_CHECK_LT( sampled, expected * 12 / 10 );

  message_hash_type chosen;
  chosen._hash[0] = 3 * 7;
  BOOST_CHECK( is_relay_sample( chosen, 7 ) );
  BOOST_CHECK( !is_relay_sample( chosen, 8 ) );
  BOOST_CHECK( is_relay_sample( chosen, 1 ) );
}

BOOST_AUTO_TEST_CASE( raw_realtime_clock_matches_the_system_clock )
{
  const int64_t system_us = std::chrono::duration_cast<std::chrono::microseconds>(
    std::chrono::system_clock::now().time_since_epoch() ).count();
  const int64_t raw_us = graphene::net::raw_realtime_microseconds();
  BOOST_CHECK_LT( std::abs( raw_us - system_us ), 1000000 );
}

/// A report carries what was recorded since the previous one, and counting starts over after it.
BOOST_AUTO_TEST_CASE( report_covers_one_period_and_resets_the_counters )
{
  p2p_statistics statistics;
  const fc::time_point start = fc::time_point::now();

  statistics.fetch.wakes = 4;
  statistics.fetch.candidates_examined = 10;
  statistics.fetch.items_requested = 9;
  statistics.fetch.transactions_gated = 3;
  statistics.fetch.transactions_bounded = 4;
  statistics.fetch.scheduling_time = fc::microseconds( 70 );

  statistics.withholding.not_available_answers = 5;
  statistics.withholding.busy_answers = 6;
  statistics.withholding.late_block_copies = 2;
  statistics.withholding.strikes = 3;
  statistics.withholding.disconnects = 1;

  statistics.record_submission( 1 );
  statistics.record_submission( 2 );
  statistics.record_submission( 6 );
  statistics.record_limit_wait( fc::microseconds( 300 ) );
  statistics.record_completion( fc::microseconds( 99 ) );
  statistics.record_completion( fc::microseconds( 100 ) );
  statistics.record_completion( fc::microseconds( 4999 ) );
  statistics.record_completion( fc::microseconds( 5000 ) );

  statistics.record_write( 3, 300 );
  statistics.record_write( 1, 100 );

  const size_t limit = GRAPHENE_NET_MAX_TRANSACTIONS_PER_PEER_DURING_NORMAL_OPERATION;
  statistics.record_transaction_request( 10, 10 );       // the byte cap bound it
  statistics.record_transaction_request( 4, 10 );        // fewer were advertised than the byte cap allows
  statistics.record_transaction_request( limit, limit ); // the per-peer limit bound it, not the byte cap

  const fc::variant_object report = statistics.take_report( start + fc::seconds( 30 ), 5, 7, { 40000, 90000 }, {} );

  BOOST_CHECK_EQUAL( report_uint( report, "fetch", "wakes" ), 4u );
  BOOST_CHECK_EQUAL( report_uint( report, "fetch", "candidates_examined" ), 10u );
  BOOST_CHECK_EQUAL( report_uint( report, "fetch", "items_requested" ), 9u );
  BOOST_CHECK_EQUAL( report_uint( report, "fetch", "items_queued" ), 7u );
  BOOST_CHECK_EQUAL( report_uint( report, "fetch", "scheduling_us" ), 70u );

  BOOST_CHECK_EQUAL( report_uint( report, "transaction_fetch_gate", "threshold_bytes" ), 30000u );
  BOOST_CHECK_EQUAL( report_uint( report, "transaction_fetch_gate", "pending_bytes" ), 40000u );
  BOOST_CHECK( report["transaction_fetch_gate"].get_object()["gated"].as_bool() );
  BOOST_CHECK_EQUAL( report_uint( report, "transaction_fetch_gate", "gated_wakes" ), 3u );
  BOOST_CHECK_EQUAL( report_uint( report, "transaction_fetch_gate", "bounded_wakes" ), 4u );

  BOOST_CHECK_EQUAL( report_uint( report, "handoff", "submissions" ), 3u );
  BOOST_CHECK_EQUAL( report_uint( report, "handoff", "completions" ), 4u );
  BOOST_CHECK_EQUAL( report_uint( report, "handoff", "in_flight" ), 5u );
  BOOST_CHECK_EQUAL( report_uint( report, "handoff", "max_in_flight" ), 6u );
  BOOST_CHECK_CLOSE( report_double( report, "handoff", "mean_in_flight" ), 3.0, 0.001 );
  BOOST_CHECK_EQUAL( report_uint( report, "handoff", "limit_waits" ), 1u );
  BOOST_CHECK_EQUAL( report_uint( report, "handoff", "limit_wait_us" ), 300u );
  const fc::variant_object& latency = report["handoff"].get_object()["completion_latency"].get_object();
  BOOST_CHECK_EQUAL( latency["lt100us"].as_uint64(), 1u );
  BOOST_CHECK_EQUAL( latency["lt250us"].as_uint64(), 1u );
  BOOST_CHECK_EQUAL( latency["lt500us"].as_uint64(), 0u );
  BOOST_CHECK_EQUAL( latency["lt1000us"].as_uint64(), 0u );
  BOOST_CHECK_EQUAL( latency["lt2000us"].as_uint64(), 0u );
  BOOST_CHECK_EQUAL( latency["lt5000us"].as_uint64(), 1u );
  BOOST_CHECK_EQUAL( latency["ge5000us"].as_uint64(), 1u );

  BOOST_CHECK_EQUAL( report_uint( report, "send", "writes" ), 2u );
  BOOST_CHECK_CLOSE( report_double( report, "send", "messages_per_write" ), 2.0, 0.001 );
  BOOST_CHECK_CLOSE( report_double( report, "send", "bytes_per_write" ), 200.0, 0.001 );

  BOOST_CHECK_EQUAL( report_uint( report, "transaction_requests", "requests" ), 3u );
  BOOST_CHECK_CLOSE( report_double( report, "transaction_requests", "transactions_per_request" ),
                     double( 10 + 4 + limit ) / 3, 0.001 );
  BOOST_CHECK_EQUAL( report_uint( report, "transaction_requests", "byte_cap_bound" ), 1u );

  BOOST_CHECK_EQUAL( report_uint( report, "withholding", "not_available_answers" ), 5u );
  BOOST_CHECK_EQUAL( report_uint( report, "withholding", "busy_answers" ), 6u );
  BOOST_CHECK_EQUAL( report_uint( report, "withholding", "late_block_copies" ), 2u );
  BOOST_CHECK_EQUAL( report_uint( report, "withholding", "strikes" ), 3u );
  BOOST_CHECK_EQUAL( report_uint( report, "withholding", "disconnects" ), 1u );

  // the next period starts empty, from the time of the last report
  const fc::variant_object next = statistics.take_report( start + fc::seconds( 60 ), 0, 0, { 20000, 90000 }, {} );
  BOOST_CHECK_EQUAL( next["seconds"].as_int64(), 30 );
  for( const char* key : { "wakes", "candidates_examined", "items_requested", "scheduling_us" } )
    BOOST_CHECK_EQUAL( report_uint( next, "fetch", key ), 0u );
  for( const char* key : { "submissions", "completions", "max_in_flight", "limit_waits", "limit_wait_us" } )
    BOOST_CHECK_EQUAL( report_uint( next, "handoff", key ), 0u );
  BOOST_CHECK_EQUAL( report_uint( next, "transaction_fetch_gate", "gated_wakes" ), 0u );
  BOOST_CHECK_EQUAL( report_uint( next, "transaction_fetch_gate", "bounded_wakes" ), 0u );
  BOOST_CHECK_EQUAL( report_uint( next, "transaction_fetch_gate", "pending_bytes" ), 20000u );
  BOOST_CHECK( !next["transaction_fetch_gate"].get_object()["gated"].as_bool() );
  for( const auto& bucket : next["handoff"].get_object()["completion_latency"].get_object() )
    BOOST_CHECK_EQUAL( bucket.value().as_uint64(), 0u );
  BOOST_CHECK_EQUAL( report_double( next, "handoff", "mean_in_flight" ), 0.0 );
  BOOST_CHECK_EQUAL( report_uint( next, "send", "writes" ), 0u );
  BOOST_CHECK_EQUAL( report_double( next, "send", "messages_per_write" ), 0.0 );
  BOOST_CHECK_EQUAL( report_uint( next, "transaction_requests", "requests" ), 0u );
  BOOST_CHECK_EQUAL( report_uint( next, "transaction_requests", "byte_cap_bound" ), 0u );
  for( const char* key : { "not_available_answers", "busy_answers", "late_block_copies", "strikes", "disconnects" } )
    BOOST_CHECK_EQUAL( report_uint( next, "withholding", key ), 0u );
}

/// The report gives how the period's transaction requests were spread over the active peers, counting those
/// that got none; without peers every figure is 0
BOOST_AUTO_TEST_CASE( report_gives_the_spread_of_transaction_requests_over_peers )
{
  p2p_statistics statistics;
  const fc::time_point start = fc::time_point::now();
  const pending_transaction_pool_size pool{ 0, 90000 };

  const fc::variant_object odd = statistics.take_report( start + fc::seconds( 30 ), 0, 0, pool, { 40, 0, 7, 300, 12 } );
  const fc::variant_object& spread = odd["transaction_requests"].get_object()["per_peer"].get_object();
  BOOST_CHECK_EQUAL( spread["peers"].as_uint64(), 5u );
  BOOST_CHECK_EQUAL( spread["requested_peers"].as_uint64(), 4u );
  BOOST_CHECK_EQUAL( spread["min"].as_uint64(), 0u );
  BOOST_CHECK_EQUAL( spread["median"].as_uint64(), 12u );
  BOOST_CHECK_EQUAL( spread["max"].as_uint64(), 300u );

  // the upper of the two middle counts
  const fc::variant_object even = statistics.take_report( start + fc::seconds( 60 ), 0, 0, pool, { 9, 3, 5, 1 } );
  const fc::variant_object& even_spread = even["transaction_requests"].get_object()["per_peer"].get_object();
  BOOST_CHECK_EQUAL( even_spread["requested_peers"].as_uint64(), 4u );
  BOOST_CHECK_EQUAL( even_spread["min"].as_uint64(), 1u );
  BOOST_CHECK_EQUAL( even_spread["median"].as_uint64(), 5u );
  BOOST_CHECK_EQUAL( even_spread["max"].as_uint64(), 9u );

  const fc::variant_object none = statistics.take_report( start + fc::seconds( 90 ), 0, 0, pool, {} );
  for( const char* key : { "peers", "requested_peers", "min", "median", "max" } )
    BOOST_CHECK_EQUAL( none["transaction_requests"].get_object()["per_peer"].get_object()[key].as_uint64(), 0u );
}

/// The node gates transaction fetching once its pending pool holds more than a third of max-mempool-size, however
/// small that is, and never with max-mempool-size 0
BOOST_AUTO_TEST_CASE( transaction_fetching_is_gated_above_a_third_of_the_mempool_limit )
{
  const uint64_t max_mempool_size = 100 * 1024 * 1024;
  const uint64_t threshold = transaction_fetch_gate_threshold( max_mempool_size );
  BOOST_CHECK_EQUAL( threshold, max_mempool_size / 3 );

  BOOST_CHECK( !is_transaction_fetching_gated( pending_transaction_pool_size{ 0, max_mempool_size } ) );
  BOOST_CHECK( !is_transaction_fetching_gated( pending_transaction_pool_size{ threshold, max_mempool_size } ) );
  BOOST_CHECK( is_transaction_fetching_gated( pending_transaction_pool_size{ threshold + 1, max_mempool_size } ) );
  BOOST_CHECK( is_transaction_fetching_gated( pending_transaction_pool_size{ max_mempool_size, max_mempool_size } ) );

  BOOST_CHECK_EQUAL( transaction_fetch_gate_threshold( 0 ), 0u );
  BOOST_CHECK( !is_transaction_fetching_gated( pending_transaction_pool_size{ 1024 * 1024, 0 } ) );

  for( const uint64_t tiny : { uint64_t( 1 ), uint64_t( 2 ) } )
  {
    BOOST_CHECK_EQUAL( transaction_fetch_gate_threshold( tiny ), 1u );
    BOOST_CHECK( !is_transaction_fetching_gated( pending_transaction_pool_size{ 1, tiny } ) );
    BOOST_CHECK( is_transaction_fetching_gated( pending_transaction_pool_size{ 2, tiny } ) );
  }
}

/// The closed gate lets a pass request only its minimum of transactions in all, and counts as holding transactions
/// back only for a peer that had transactions to give and room to request them once that minimum was requested; the
/// open gate passes every window through and holds nothing back
BOOST_AUTO_TEST_CASE( the_gate_holds_back_only_transactions_that_would_have_been_requested )
{
  const uint64_t max_mempool_size = 90000;
  const pending_transaction_pool_size over{ 40000, max_mempool_size };
  const pending_transaction_pool_size under{ 20000, max_mempool_size };

  transaction_fetch_gate closed( over, 0, 1 );
  BOOST_CHECK( closed.closed() );
  BOOST_CHECK_EQUAL( closed.transactions_to_request( 0, candidates ), 0u );   // window full or fetching inhibited
  BOOST_CHECK_EQUAL( closed.transactions_to_request( 400, candidates ), 400u ); // its minimum, from whichever peer takes it
  BOOST_CHECK( closed.may_request_more( candidates ) );
  BOOST_CHECK( closed.next_request_is_minimum() );
  BOOST_CHECK( !closed.requested_minimum() );
  closed.record_request( 1000 );
  BOOST_CHECK( closed.requested_minimum() );
  BOOST_CHECK( !closed.held_back() );
  BOOST_CHECK( !closed.may_request_more( no_candidates ) ); // nothing (live) advertised
  BOOST_CHECK( !closed.held_back() );
  BOOST_CHECK_EQUAL( closed.transactions_to_request( 400, no_candidates ), 0u );
  BOOST_CHECK( !closed.held_back() );
  BOOST_CHECK( !closed.may_request_more( candidates ) );
  BOOST_CHECK( closed.held_back() );
  BOOST_CHECK( !closed.bounded() );

  // with no minimum the closed gate requests nothing, and once one peer has been found to have candidates the
  // others are not looked at
  transaction_fetch_gate shut( over, 0, 0 );
  BOOST_CHECK_EQUAL( shut.transactions_to_request( 400, no_candidates ), 0u );
  BOOST_CHECK( !shut.held_back() );
  BOOST_CHECK_EQUAL( shut.transactions_to_request( 400, candidates ), 0u );
  BOOST_CHECK( shut.held_back() );
  size_t looked_at = 0;
  BOOST_CHECK_EQUAL( shut.transactions_to_request( 400, [&]() { ++looked_at; return true; } ), 0u );
  BOOST_CHECK( !shut.may_request_more( [&]() { ++looked_at; return true; } ) );
  BOOST_CHECK_EQUAL( looked_at, 0u );
  BOOST_CHECK( !shut.requested_minimum() );

  transaction_fetch_gate open( under, 0 );
  BOOST_CHECK( !open.closed() );
  BOOST_CHECK_EQUAL( open.transactions_to_request( 400, candidates ), 400u );
  BOOST_CHECK_EQUAL( open.transactions_to_request( 0, candidates ), 0u );
  BOOST_CHECK( !open.held_back() );
  BOOST_CHECK( !open.bounded() );
  BOOST_CHECK( !open.next_request_is_minimum() );
  open.record_request( 1000 );
  BOOST_CHECK( !open.requested_minimum() );
}

/// Below the threshold a pass requests no more than fits under it with what is already on its way into the pool,
/// going past it by at most one transaction; the transactions it then withholds count as bounded, not gated
BOOST_AUTO_TEST_CASE( a_pass_requests_no_more_than_the_headroom_below_the_threshold )
{
  const uint64_t max_mempool_size = 90000; // threshold 30000
  const pending_transaction_pool_size under{ 20000, max_mempool_size };

  transaction_fetch_gate gate( under, 6000, 0 ); // headroom 4000; a pass whose minimum isn't due
  BOOST_CHECK( !gate.closed() );
  BOOST_CHECK_EQUAL( gate.transactions_to_request( 400, candidates ), 400u );
  size_t requested = 0;
  while( gate.may_request_more( candidates ) )
  {
    gate.record_request( 300 );
    ++requested;
  }
  BOOST_CHECK_EQUAL( requested, 14u ); // 13 fit, the 14th goes past by 200 bytes
  BOOST_CHECK( gate.bounded() );
  BOOST_CHECK( !gate.held_back() );
  BOOST_CHECK( !gate.requested_minimum() );
  BOOST_CHECK_EQUAL( gate.transactions_to_request( 400, candidates ), 0u );

  // a peer with nothing more to give doesn't count as held back
  transaction_fetch_gate drained( under, 0, 0 );
  drained.record_request( 20000 );
  BOOST_CHECK( !drained.may_request_more( no_candidates ) );
  BOOST_CHECK( !drained.bounded() );

  // what is on its way may use up the headroom before the pass begins; the pass still requests its minimum
  transaction_fetch_gate taken( under, 10000 );
  BOOST_CHECK( !taken.closed() );
  BOOST_CHECK_EQUAL( taken.transactions_to_request( 0, candidates ), 0u );
  BOOST_CHECK_EQUAL( taken.transactions_to_request( 400, candidates ), 400u );
  BOOST_CHECK( !taken.bounded() );
  size_t requested_beyond_headroom = 0;
  while( taken.may_request_more( candidates ) )
  {
    taken.record_request( 300 );
    ++requested_beyond_headroom;
  }
  BOOST_CHECK_EQUAL( requested_beyond_headroom, size_t( GRAPHENE_NET_MIN_TRANSACTIONS_PER_GATED_FETCH ) );
  BOOST_CHECK( taken.bounded() );
  BOOST_CHECK( !taken.held_back() );
  BOOST_CHECK( taken.requested_minimum() );

  // with max-mempool-size 0 there is no gate and no bound
  transaction_fetch_gate disabled( pending_transaction_pool_size{ 1024 * 1024, 0 }, 1024 * 1024 );
  BOOST_CHECK_EQUAL( disabled.transactions_to_request( 400, candidates ), 400u );
  disabled.record_request( 1024 * 1024 );
  BOOST_CHECK( disabled.may_request_more( candidates ) );
  BOOST_CHECK( !disabled.bounded() );
}

/// A headroom worth one transaction per pass goes to the peer after the one that took the last transaction, so
/// over successive passes every peer with candidates is asked in turn rather than always the first one in
/// connection order; a peer without candidates is passed over
BOOST_AUTO_TEST_CASE( a_tight_headroom_rotates_over_the_peers_from_pass_to_pass )
{
  const pending_transaction_pool_size under{ 20000, 90000 }; // threshold 30000
  std::vector<fetching_peer> peers( 5 );
  std::set<message_hash_type> wanted;
  uint32_t next_transaction = 0;
  for( size_t i = 0; i < peers.size(); ++i )
  {
    const size_t advertised = i == 2 ? 0 : i == 1 ? 1 : 3; // peer 2 has none, peer 1 only one
    for( size_t j = 0; j < advertised; ++j )
    {
      const item_id item = transaction( next_transaction++ );
      peers[ i ].candidates.push( item, queued_at, fc::time_point() );
      wanted.insert( item.item_hash );
    }
  }

  fetch_turn_order order;
  std::vector<std::vector<size_t>> requested_after_pass;
  for( int pass = 0; pass < 5; ++pass )
  {
    // headroom 500: one 1000-byte transaction goes past it; passes whose minimum isn't due
    transaction_fetch_gate gate( under, 9500, 0 );
    fetch_pass( peers, order, gate, wanted, 1000 );
    BOOST_CHECK( gate.bounded() );
    std::vector<size_t> requested;
    for( const fetching_peer& peer : peers )
      requested.push_back( peer.requested );
    requested_after_pass.push_back( requested );
  }
  // peer 0, peer 1 (its only one), then after peer 1 (no longer listed) peer 3, peer 4, and round to peer 0
  const std::vector<std::vector<size_t>> expected{ { 1, 0, 0, 0, 0 }, { 1, 1, 0, 0, 0 }, { 1, 1, 0, 1, 0 },
                                                   { 1, 1, 0, 1, 1 }, { 2, 1, 0, 1, 1 } };
  BOOST_CHECK( requested_after_pass == expected );
}

/// Once the headroom is used up, a peer whose remaining candidates were all requested from another peer withholds
/// nothing, so the pass is not counted as bounded; one with a candidate still to fetch is
BOOST_AUTO_TEST_CASE( only_transactions_still_to_fetch_count_as_bounded )
{
  const pending_transaction_pool_size under{ 20000, 90000 }; // threshold 30000
  const item_id shared = transaction( 1 );
  const item_id own = transaction( 2 );

  std::vector<fetching_peer> peers( 2 );
  std::set<message_hash_type> wanted{ shared.item_hash };
  for( fetching_peer& peer : peers )
    peer.candidates.push( shared, queued_at, fc::time_point() );
  fetch_turn_order order;
  transaction_fetch_gate gate( under, 9500, 0 ); // headroom 500
  fetch_pass( peers, order, gate, wanted, 1000 );
  BOOST_CHECK_EQUAL( peers[ 0 ].requested + peers[ 1 ].requested, 1u );
  BOOST_CHECK( !gate.bounded() );
  BOOST_CHECK_EQUAL( peers[ 0 ].candidates.size() + peers[ 1 ].candidates.size(), 0u );

  peers[ 1 ].candidates.push( shared, queued_at, fc::time_point() );
  peers[ 1 ].candidates.push( own, queued_at, fc::time_point() );
  wanted = { shared.item_hash, own.item_hash };
  peers[ 0 ].candidates.push( shared, queued_at, fc::time_point() );
  transaction_fetch_gate bounded( under, 9500, 0 );
  fetch_pass( peers, order, bounded, wanted, 1000 );
  BOOST_CHECK( bounded.bounded() );
  BOOST_CHECK( wanted == std::set<message_hash_type>{ own.item_hash } );

  // with no headroom left before the pass and its minimum requested, a peer whose candidates were all taken isn't
  // counted either
  transaction_fetch_gate taken( under, 10000, 1 );
  taken.record_request( 1000 );
  uint64_t examined = 0;
  fetch_candidate_queue stale;
  stale.push( shared, queued_at, fc::time_point() );
  BOOST_CHECK_EQUAL( taken.transactions_to_request( 400, [&]()
    { return stale.has_transaction_to_take( [&]( const item_id& candidate ) { return wanted.count( candidate.item_hash ) != 0; }, examined ); } ), 0u );
  BOOST_CHECK( !taken.bounded() );
  BOOST_CHECK_EQUAL( stale.size(), 0u );
  BOOST_CHECK_EQUAL( examined, 1u );
}

/// However many peers advertise transactions, a pass of the closed gate requests its minimum in all, and over
/// successive passes that minimum goes to every peer with candidates in turn; so its intake is bounded by the rate
/// of passes that get a minimum rather than by the peers' windows.  With a minimum of one per pass, a fast-confirm
/// advertised by one peer among flooding ones is requested within as many passes as there are peers
BOOST_AUTO_TEST_CASE( a_closed_gate_requests_its_minimum_per_pass_from_the_peers_in_turn )
{
  const pending_transaction_pool_size over{ 40000, 90000 }; // threshold 30000
  std::vector<fetching_peer> peers( 4 );
  std::set<message_hash_type> wanted;
  uint32_t next_transaction = 0;
  for( size_t i = 0; i < peers.size(); ++i )
  {
    const size_t advertised = i == 2 ? 1 : 50; // peer 2 advertises a fast-confirm only, the others flood
    for( size_t j = 0; j < advertised; ++j )
    {
      const item_id item = transaction( next_transaction++ );
      peers[ i ].candidates.push( item, queued_at, fc::time_point() );
      wanted.insert( item.item_hash );
    }
  }
  const message_hash_type fast_confirm = hash_of( 100 );
  BOOST_REQUIRE( wanted.count( fast_confirm ) );

  fetch_turn_order order;
  std::vector<std::vector<size_t>> requested_after_pass;
  for( int pass = 0; pass < 6; ++pass )
  {
    transaction_fetch_gate gate( over, 0, 1 );
    fetch_pass( peers, order, gate, wanted, 200 );
    BOOST_CHECK( gate.held_back() );
    BOOST_CHECK( !gate.bounded() );
    std::vector<size_t> requested;
    for( const fetching_peer& peer : peers )
      requested.push_back( peer.requested );
    requested_after_pass.push_back( requested );
  }
  const std::vector<std::vector<size_t>> expected{ { 1, 0, 0, 0 }, { 1, 1, 0, 0 }, { 1, 1, 1, 0 },
                                                   { 1, 1, 1, 1 }, { 2, 1, 1, 1 }, { 2, 2, 1, 1 } };
  BOOST_CHECK( requested_after_pass == expected );
  BOOST_CHECK( !wanted.count( fast_confirm ) );
}

/// An empty pool doesn't stop fetching however many bytes are estimated to be on their way: with more estimated
/// in flight than the threshold, as many busy peers at the initial transaction size estimate make it, each pass
/// still requests its minimum, and only that
BOOST_AUTO_TEST_CASE( an_empty_pool_keeps_fetching_whatever_is_estimated_on_its_way )
{
  const uint64_t max_mempool_size = 10 * 1024 * 1024;
  const pending_transaction_pool_size empty{ 0, max_mempool_size };
  const uint64_t estimated_on_their_way = 8 * uint64_t( GRAPHENE_NET_MAX_TRANSACTION_BYTES_IN_FLIGHT_PER_PEER );
  BOOST_REQUIRE_GT( estimated_on_their_way, transaction_fetch_gate_threshold( max_mempool_size ) );

  const uint32_t minimum = GRAPHENE_NET_MIN_TRANSACTIONS_PER_GATED_FETCH;
  std::vector<fetching_peer> peers( 3 );
  std::set<message_hash_type> wanted;
  for( uint32_t i = 0; i < 3 * 30; ++i )
  {
    peers[ i % peers.size() ].candidates.push( transaction( i ), queued_at, fc::time_point() );
    wanted.insert( hash_of( i ) );
  }
  fetch_turn_order order;
  for( int pass = 0; pass < 3; ++pass )
  {
    transaction_fetch_gate gate( empty, estimated_on_their_way );
    BOOST_CHECK( !gate.closed() );
    fetch_pass( peers, order, gate, wanted, 200 );
    BOOST_CHECK( gate.bounded() );
    BOOST_CHECK( gate.requested_minimum() );
  }
  BOOST_CHECK_EQUAL( wanted.size(), 3 * 30 - 3 * minimum );
  // the turns go round the peers from pass to pass, so over three passes each peer gave a minimum's worth
  for( const fetching_peer& peer : peers )
    BOOST_CHECK_EQUAL( peer.requested, size_t( minimum ) );

  // a lone peer with more to give than the minimum makes the pass count as bounded, so the node checks again soon
  std::vector<fetching_peer> lone( 1 );
  for( uint32_t i = 1000; i < 1000 + minimum + 3; ++i )
  {
    lone[ 0 ].candidates.push( transaction( i ), queued_at, fc::time_point() );
    wanted.insert( hash_of( i ) );
  }
  fetch_turn_order lone_order;
  transaction_fetch_gate gate( empty, estimated_on_their_way );
  fetch_pass( lone, lone_order, gate, wanted, 200 );
  BOOST_CHECK_EQUAL( lone[ 0 ].requested, size_t( minimum ) );
  BOOST_CHECK( gate.bounded() );
}

/// The closed gate's minimum takes each peer's newest candidates: a fast-confirm a peer advertised after a backlog of
/// flooded transactions is requested in the first pass, before the backlog is (an open gate takes the oldest first)
BOOST_AUTO_TEST_CASE( the_minimum_takes_a_fast_confirm_behind_a_backlog_on_the_same_peer_first )
{
  const pending_transaction_pool_size over{ 40000, 90000 }; // threshold 30000
  const size_t backlog = 10 * GRAPHENE_NET_MIN_TRANSACTIONS_PER_GATED_FETCH;
  std::vector<fetching_peer> peers( 1 );
  std::set<message_hash_type> wanted;
  for( uint32_t i = 0; i <= backlog; ++i ) // the last one is the fast-confirm
  {
    peers[ 0 ].candidates.push( transaction( i ), queued_at + fc::milliseconds( i ), fc::time_point() );
    wanted.insert( hash_of( i ) );
  }
  const message_hash_type fast_confirm = hash_of( uint32_t( backlog ) );

  fetch_turn_order order;
  transaction_fetch_gate gate( over, 0 );
  const std::vector<message_hash_type> requested = fetch_pass( peers, order, gate, wanted, 200 );
  BOOST_CHECK( gate.held_back() );
  BOOST_REQUIRE_EQUAL( requested.size(), size_t( GRAPHENE_NET_MIN_TRANSACTIONS_PER_GATED_FETCH ) );
  BOOST_CHECK( requested.front() == fast_confirm );
  // newest first, all of it from the end of the backlog
  for( size_t i = 0; i < requested.size(); ++i )
    BOOST_CHECK( requested[ i ] == hash_of( uint32_t( backlog - i ) ) );

  // a pass whose minimum isn't due requests nothing
  transaction_fetch_gate not_due( over, 0, 0 );
  BOOST_CHECK( fetch_pass( peers, order, not_due, wanted, 200 ).empty() );
  BOOST_CHECK( not_due.held_back() );

  // an open gate takes the oldest first (with headroom enough for all of them)
  const pending_transaction_pool_size under{ 0, 900000 };
  transaction_fetch_gate open( under, 0 );
  const std::vector<message_hash_type> rest = fetch_pass( peers, order, open, wanted, 200 );
  BOOST_REQUIRE_EQUAL( rest.size(), backlog + 1 - GRAPHENE_NET_MIN_TRANSACTIONS_PER_GATED_FETCH );
  BOOST_CHECK( rest.front() == hash_of( 0 ) );
  BOOST_CHECK( wanted.empty() );
}

/// A pass gets a minimum only an interval after the last one that requested some of it; a pass that was due one
/// but requested none leaves the next pass due, and the check of a withholding gate is then an interval later
BOOST_AUTO_TEST_CASE( the_minimum_is_given_once_per_interval )
{
  const fc::time_point start = fc::time_point::from_iso_string( "2026-11-20T00:00:00" );
  const fc::microseconds interval = fc::milliseconds( GRAPHENE_NET_TRANSACTION_FETCH_GATE_RECHECK_MS );
  const size_t minimum = GRAPHENE_NET_MIN_TRANSACTIONS_PER_GATED_FETCH;
  const pending_transaction_pool_size over{ 40000, 90000 };
  const pending_transaction_pool_size under{ 0, 90000 };
  fetch_minimum_schedule schedule( interval );

  BOOST_CHECK_EQUAL( schedule.minimum_for_pass( start ), minimum );
  transaction_fetch_gate first( over, 0, schedule.minimum_for_pass( start ) );
  first.record_request( 200 );
  BOOST_CHECK( schedule.record_pass( first, start ) == start + interval );

  // the passes woken in between (5 ms apart at the default fetch interval) get none, and say when the next is due
  for( int pass = 1; pass < 20; ++pass )
  {
    const fc::time_point pass_start = start + fc::milliseconds( 5 * pass );
    BOOST_CHECK_EQUAL( schedule.minimum_for_pass( pass_start ), 0u );
    transaction_fetch_gate gate( over, 0, schedule.minimum_for_pass( pass_start ) );
    BOOST_CHECK( !gate.may_request_more( candidates ) );
    BOOST_CHECK( schedule.record_pass( gate, pass_start ) == start + interval );
  }

  // due again; an open gate with headroom requests nothing of its minimum, so the next pass is still due
  const fc::time_point due = start + interval;
  BOOST_CHECK_EQUAL( schedule.minimum_for_pass( due ), minimum );
  transaction_fetch_gate open( under, 0, schedule.minimum_for_pass( due ) );
  open.record_request( 200 );
  BOOST_CHECK( schedule.record_pass( open, due ) == due + interval );
  BOOST_CHECK_EQUAL( schedule.minimum_for_pass( due + fc::milliseconds( 5 ) ), minimum );
}

/// While the gate withholds transactions the expiry warning comes at most once per interval, with the sum since the
/// last one; an interval after the last pass that withheld any, every pass that expired something warns, including
/// what was left from the withholding period
BOOST_AUTO_TEST_CASE( expiry_warnings_are_rate_limited_only_while_gated )
{
  const fc::time_point start = fc::time_point::from_iso_string( "2026-11-20T00:00:00" );
  const fc::microseconds interval = fc::seconds( 30 );
  fetch_expiry_warning_limiter limiter( interval );

  BOOST_CHECK( limiter.record( 0, true, start ) == std::nullopt );
  BOOST_CHECK( limiter.record( 5, true, start ) == std::optional<size_t>( 5 ) );
  // passes 100 ms apart, each expiring some, for just under an interval: no warning
  for( int pass = 1; pass < 300; ++pass )
    BOOST_CHECK( limiter.record( 2, true, start + fc::milliseconds( 100 * pass ) ) == std::nullopt );
  BOOST_CHECK( limiter.record( 2, true, start + interval ) == std::optional<size_t>( 300 * 2 ) );

  const fc::time_point last_withheld = start + interval + fc::seconds( 1 );
  BOOST_CHECK( limiter.record( 3, true, last_withheld ) == std::nullopt );
  // the gate stops withholding: within an interval of the last pass that withheld, the sum still waits for the interval
  BOOST_CHECK( limiter.record( 1, false, last_withheld + fc::seconds( 1 ) ) == std::nullopt );
  BOOST_CHECK( limiter.record( 0, false, start + interval + interval ) == std::optional<size_t>( 4 ) );
  // an interval after the last pass that withheld, every expiry is warned about right away
  BOOST_CHECK( limiter.record( 0, false, last_withheld + interval ) == std::nullopt );
  BOOST_CHECK( limiter.record( 1, false, last_withheld + interval + fc::seconds( 1 ) ) == std::optional<size_t>( 1 ) );
  BOOST_CHECK( limiter.record( 1, false, last_withheld + interval + fc::seconds( 2 ) ) == std::optional<size_t>( 1 ) );
}

/// Passes that the open gate's headroom bounded (transaction_fetch_gate::bounded()) hold the warning back like gated
/// ones, also across the passes in between whose headroom was enough, and the warnings add up to every expiry
BOOST_AUTO_TEST_CASE( expiry_warnings_are_rate_limited_while_the_headroom_bounds_the_fetch )
{
  const fc::time_point start = fc::time_point::from_iso_string( "2026-11-20T00:00:00" );
  const fc::microseconds interval = fc::seconds( 30 );
  fetch_expiry_warning_limiter limiter( interval );
  const pending_transaction_pool_size under{ 20000, 90000 }; // threshold 30000

  std::vector<size_t> warnings;
  // 90 s of passes 100 ms apart, each expiring 3 items; every tenth pass has headroom enough for what it requests
  for( int pass = 0; pass < 900; ++pass )
  {
    const bool enough = pass % 10 == 9;
    transaction_fetch_gate gate( under, enough ? 0 : 9000 );
    BOOST_REQUIRE( !gate.closed() );
    BOOST_REQUIRE_EQUAL( gate.transactions_to_request( 400, candidates ), 400u );
    for( int request = 0; request < 2 || ( !enough && gate.may_request_more( candidates ) ); ++request )
      gate.record_request( 300 );
    BOOST_REQUIRE_EQUAL( gate.bounded(), !enough );

    if( const std::optional<size_t> to_report = limiter.record( 3, gate.closed() || gate.bounded(), start + fc::milliseconds( 100 * pass ) ) )
      warnings.push_back( *to_report );
  }
  // the first expiry right away, then the sum once per interval
  BOOST_CHECK( warnings == std::vector<size_t>( { 3, 300 * 3, 300 * 3 } ) );
  // the rest comes an interval after the last warning
  BOOST_CHECK( limiter.record( 0, false, start + fc::seconds( 90 ) ) == std::optional<size_t>( 299 * 3 ) );
}

BOOST_AUTO_TEST_SUITE_END()
