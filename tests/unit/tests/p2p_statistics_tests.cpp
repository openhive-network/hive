#include <boost/test/unit_test.hpp>

#include <graphene/net/config.hpp>
#include <graphene/net/node.hpp>

#include <fc/crypto/ripemd160.hpp>

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <string>

using graphene::net::is_relay_sample;
using graphene::net::message_hash_type;
using graphene::net::p2p_statistics;

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

  const fc::variant_object report = statistics.take_report( start + fc::seconds( 30 ), 5, 7 );

  BOOST_CHECK_EQUAL( report_uint( report, "fetch", "wakes" ), 4u );
  BOOST_CHECK_EQUAL( report_uint( report, "fetch", "candidates_examined" ), 10u );
  BOOST_CHECK_EQUAL( report_uint( report, "fetch", "items_requested" ), 9u );
  BOOST_CHECK_EQUAL( report_uint( report, "fetch", "items_queued" ), 7u );
  BOOST_CHECK_EQUAL( report_uint( report, "fetch", "scheduling_us" ), 70u );

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
  const fc::variant_object next = statistics.take_report( start + fc::seconds( 60 ), 0, 0 );
  BOOST_CHECK_EQUAL( next["seconds"].as_int64(), 30 );
  for( const char* key : { "wakes", "candidates_examined", "items_requested", "scheduling_us" } )
    BOOST_CHECK_EQUAL( report_uint( next, "fetch", key ), 0u );
  for( const char* key : { "submissions", "completions", "max_in_flight", "limit_waits", "limit_wait_us" } )
    BOOST_CHECK_EQUAL( report_uint( next, "handoff", key ), 0u );
  for( const auto& bucket : next["handoff"].get_object()["completion_latency"].get_object() )
    BOOST_CHECK_EQUAL( bucket.value().as_uint64(), 0u );
  BOOST_CHECK_EQUAL( report_double( next, "handoff", "mean_in_flight" ), 0.0 );
  BOOST_CHECK_EQUAL( report_uint( next, "send", "writes" ), 0u );
  BOOST_CHECK_EQUAL( report_double( next, "send", "messages_per_write" ), 0.0 );
  BOOST_CHECK_EQUAL( report_uint( next, "transaction_requests", "requests" ), 0u );
  BOOST_CHECK_EQUAL( report_uint( next, "transaction_requests", "byte_cap_bound" ), 0u );
}

/// The pending pool gate reports its limit, the pool as the passes saw it and how many passes it held back.
/// The limit and the latest size are state, so they carry over into a period without passes; the counts start over
BOOST_AUTO_TEST_CASE( report_shows_the_pending_pool_gate )
{
  p2p_statistics statistics;
  const fc::time_point start = fc::time_point::now();

  statistics.record_pending_pool( 1000, 4000, false );
  statistics.record_pending_pool( 5000, 4000, true );
  statistics.record_pending_pool( 4500, 4000, true );

  const fc::variant_object report = statistics.take_report( start + fc::seconds( 30 ), 0, 0 );
  BOOST_CHECK_EQUAL( report_uint( report, "pending_pool", "limit_bytes" ), 4000u );
  BOOST_CHECK_EQUAL( report_uint( report, "pending_pool", "size_bytes" ), 4500u );
  BOOST_CHECK_EQUAL( report_uint( report, "pending_pool", "max_size_bytes" ), 5000u );
  BOOST_CHECK_EQUAL( report_uint( report, "pending_pool", "passes" ), 3u );
  BOOST_CHECK_EQUAL( report_uint( report, "pending_pool", "inhibited_passes" ), 2u );

  const fc::variant_object quiet = statistics.take_report( start + fc::seconds( 60 ), 0, 0 );
  BOOST_CHECK_EQUAL( report_uint( quiet, "pending_pool", "limit_bytes" ), 4000u );
  BOOST_CHECK_EQUAL( report_uint( quiet, "pending_pool", "size_bytes" ), 4500u );
  BOOST_CHECK_EQUAL( report_uint( quiet, "pending_pool", "max_size_bytes" ), 4500u );
  BOOST_CHECK_EQUAL( report_uint( quiet, "pending_pool", "passes" ), 0u );
  BOOST_CHECK_EQUAL( report_uint( quiet, "pending_pool", "inhibited_passes" ), 0u );
}

/// The limit is the given number of blocks of the current maximum block size; a fraction of a block is at least
/// one byte, and no positive number of blocks means no limit
BOOST_AUTO_TEST_CASE( pending_pool_fetch_limit_is_counted_in_blocks )
{
  using graphene::net::pending_pool_fetch_limit;
  BOOST_CHECK_EQUAL( pending_pool_fetch_limit( 2, 2 * 1024 * 1024 ), 4u * 1024 * 1024 );
  BOOST_CHECK_EQUAL( pending_pool_fetch_limit( 0.5, 65536 ), 32768u );
  BOOST_CHECK_EQUAL( pending_pool_fetch_limit( 1e-9, 65536 ), 1u );
  BOOST_CHECK_EQUAL( pending_pool_fetch_limit( 1e30, 65536 ), std::numeric_limits< uint64_t >::max() );
  BOOST_CHECK_EQUAL( pending_pool_fetch_limit( 0, 65536 ), 0u );
  BOOST_CHECK_EQUAL( pending_pool_fetch_limit( -1, 65536 ), 0u );
}

BOOST_AUTO_TEST_SUITE_END()
