#include <boost/test/unit_test.hpp>

#include <graphene/net/config.hpp>
#include <graphene/net/node.hpp>

#include <fc/crypto/ripemd160.hpp>

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <string>

using graphene::net::fetch_expiry_warning_limiter;
using graphene::net::is_relay_sample;
using graphene::net::is_transaction_fetching_gated;
using graphene::net::message_hash_type;
using graphene::net::p2p_statistics;
using graphene::net::pending_transaction_pool_size;
using graphene::net::transaction_fetch_gate;
using graphene::net::transaction_fetch_gate_threshold;

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
  statistics.fetch.scheduling_time = fc::microseconds( 70 );

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

  const fc::variant_object report = statistics.take_report( start + fc::seconds( 30 ), 5, 7, { 40000, 90000 } );

  BOOST_CHECK_EQUAL( report_uint( report, "fetch", "wakes" ), 4u );
  BOOST_CHECK_EQUAL( report_uint( report, "fetch", "candidates_examined" ), 10u );
  BOOST_CHECK_EQUAL( report_uint( report, "fetch", "items_requested" ), 9u );
  BOOST_CHECK_EQUAL( report_uint( report, "fetch", "items_queued" ), 7u );
  BOOST_CHECK_EQUAL( report_uint( report, "fetch", "scheduling_us" ), 70u );

  BOOST_CHECK_EQUAL( report_uint( report, "transaction_fetch_gate", "threshold_bytes" ), 30000u );
  BOOST_CHECK_EQUAL( report_uint( report, "transaction_fetch_gate", "pending_bytes" ), 40000u );
  BOOST_CHECK( report["transaction_fetch_gate"].get_object()["gated"].as_bool() );
  BOOST_CHECK_EQUAL( report_uint( report, "transaction_fetch_gate", "gated_wakes" ), 3u );

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

  // the next period starts empty, from the time of the last report
  const fc::variant_object next = statistics.take_report( start + fc::seconds( 60 ), 0, 0, { 20000, 90000 } );
  BOOST_CHECK_EQUAL( next["seconds"].as_int64(), 30 );
  for( const char* key : { "wakes", "candidates_examined", "items_requested", "scheduling_us" } )
    BOOST_CHECK_EQUAL( report_uint( next, "fetch", key ), 0u );
  for( const char* key : { "submissions", "completions", "max_in_flight", "limit_waits", "limit_wait_us" } )
    BOOST_CHECK_EQUAL( report_uint( next, "handoff", key ), 0u );
  BOOST_CHECK_EQUAL( report_uint( next, "transaction_fetch_gate", "gated_wakes" ), 0u );
  BOOST_CHECK_EQUAL( report_uint( next, "transaction_fetch_gate", "pending_bytes" ), 20000u );
  BOOST_CHECK( !next["transaction_fetch_gate"].get_object()["gated"].as_bool() );
  for( const auto& bucket : next["handoff"].get_object()["completion_latency"].get_object() )
    BOOST_CHECK_EQUAL( bucket.value().as_uint64(), 0u );
  BOOST_CHECK_EQUAL( report_double( next, "handoff", "mean_in_flight" ), 0.0 );
  BOOST_CHECK_EQUAL( report_uint( next, "send", "writes" ), 0u );
  BOOST_CHECK_EQUAL( report_double( next, "send", "messages_per_write" ), 0.0 );
  BOOST_CHECK_EQUAL( report_uint( next, "transaction_requests", "requests" ), 0u );
  BOOST_CHECK_EQUAL( report_uint( next, "transaction_requests", "byte_cap_bound" ), 0u );
}

/// The node stops requesting transactions once its pending pool holds more than a third of max-mempool-size,
/// and never with max-mempool-size 0
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
}

/// The closed gate requests no transactions from anyone, but counts as holding them back only for a peer that had
/// transactions to give and room to request them; the open gate passes every window through and holds nothing back
BOOST_AUTO_TEST_CASE( the_gate_holds_back_only_transactions_that_would_have_been_requested )
{
  const uint64_t max_mempool_size = 90000;
  const pending_transaction_pool_size over{ 40000, max_mempool_size };
  const pending_transaction_pool_size under{ 20000, max_mempool_size };

  transaction_fetch_gate closed( over );
  BOOST_CHECK( closed.closed() );
  BOOST_CHECK_EQUAL( closed.transactions_to_request( 0, true ), 0u );    // window full or fetching inhibited
  BOOST_CHECK_EQUAL( closed.transactions_to_request( 400, false ), 0u ); // nothing (live) advertised
  BOOST_CHECK( !closed.held_back() );
  BOOST_CHECK_EQUAL( closed.transactions_to_request( 400, true ), 0u );
  BOOST_CHECK( closed.held_back() );

  transaction_fetch_gate open( under );
  BOOST_CHECK( !open.closed() );
  BOOST_CHECK_EQUAL( open.transactions_to_request( 400, true ), 400u );
  BOOST_CHECK_EQUAL( open.transactions_to_request( 0, true ), 0u );
  BOOST_CHECK( !open.held_back() );
}

/// While the gate is closed the expiry warning comes at most once per interval, with the sum since the last one;
/// with the gate open every pass that expired something warns, including what was left from the gated period
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

  BOOST_CHECK( limiter.record( 3, true, start + interval + fc::seconds( 1 ) ) == std::nullopt );
  // the gate opens: what is left is warned about on the next pass even if that pass expired nothing
  BOOST_CHECK( limiter.record( 0, false, start + interval + fc::seconds( 2 ) ) == std::optional<size_t>( 3 ) );
  BOOST_CHECK( limiter.record( 0, false, start + interval + fc::seconds( 3 ) ) == std::nullopt );
  BOOST_CHECK( limiter.record( 1, false, start + interval + fc::seconds( 4 ) ) == std::optional<size_t>( 1 ) );
  BOOST_CHECK( limiter.record( 1, false, start + interval + fc::seconds( 5 ) ) == std::optional<size_t>( 1 ) );
}

BOOST_AUTO_TEST_SUITE_END()
