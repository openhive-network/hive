#ifdef IS_TEST_NET
#include <boost/test/unit_test.hpp>

#include "../db_fixture/witness_fixture.hpp"
#include "../db_fixture/p2p_test_peer.hpp"

#include <hive/chain/full_block.hpp>
#include <hive/chain/notifications.hpp>
#include <hive/plugins/p2p/p2p_plugin.hpp>

#include <graphene/net/config.hpp>
#include <graphene/net/core_messages.hpp>
#include <graphene/net/peer_connection.hpp>

#include <algorithm>
#include <atomic>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <vector>

namespace {

using graphene::net::item_hash_t;
using p2p_tests::test_peer;

const uint32_t inventory_limit = GRAPHENE_NET_MAX_INVENTORY_SIZE_IN_MINUTES * GRAPHENE_NET_MAX_TRX_PER_SECOND * 60;

struct inventory_limit_report
{
  fc::ip::endpoint peer;
  fc::time_point time; ///< when the node logged it
  uint32_t deferred = 0;
  uint32_t dropped = 0;
  uint32_t still_deferred = 0;
};

/// collects the node's "advertised more items than our inventory limit" warnings
class inventory_limit_report_catcher : public fc::appender
{
public:
  void log( const fc::log_message& m ) override
  {
    if( m.get_format().find( "advertised more items than our inventory limit" ) == std::string::npos )
      return;
    const fc::variant_object data = m.get_data();
    inventory_limit_report report;
    report.peer = data[ "peer" ].as< fc::ip::endpoint >();
    report.time = m.get_context().get_timestamp();
    report.deferred = data[ "deferred" ].as< uint32_t >();
    report.dropped = data[ "dropped" ].as< uint32_t >();
    report.still_deferred = data[ "still_deferred" ].as< uint32_t >();
    ilog( "inventory limit report: ${deferred} deferred, ${dropped} dropped, ${still_deferred} still deferred",
          ( "deferred", report.deferred )( "dropped", report.dropped )( "still_deferred", report.still_deferred ) );
    std::lock_guard< std::mutex > guard( _mutex );
    _reports.push_back( report );
  }

  std::vector< inventory_limit_report > reports() const
  {
    std::lock_guard< std::mutex > guard( _mutex );
    return _reports;
  }

private:
  mutable std::mutex _mutex;
  std::vector< inventory_limit_report > _reports;
};

std::vector< item_hash_t > hashes_of( const std::vector< full_transaction_ptr >& transactions )
{
  std::vector< item_hash_t > hashes;
  for( const full_transaction_ptr& transaction : transactions )
    hashes.push_back( transaction->get_legacy_transaction_message_hash() );
  return hashes;
}

} // namespace

// Part of witness_tests so that, like the other tests on a real-time witness node, it runs as a unit of its own.
BOOST_FIXTURE_TEST_SUITE( witness_tests, witness_fixture )

/// A peer advertises more transactions than the per-peer inventory limit and does not deliver them.
/// The node defers what is over the limit (and says so), admits the deferred transactions once there
/// is room again and fetches them, drops the ones still deferred when the inventory window passes,
/// and meanwhile keeps fetching and including the transactions of another, honest peer.
BOOST_AUTO_TEST_CASE( inventory_limit_deferral_test )
{
  try
  {
    BOOST_SCOPE_EXIT( this_ )
    {
      this_->theApp.generate_interrupt_request();
      this_->theApp.wait4interrupt_request();
      this_->theApp.quit( true );
    } BOOST_SCOPE_EXIT_END

    // The node listens on loopback only, for the peers this test runs in-process. They hold the
    // requests for transactions they won't deliver longer than the default 6 s after which the node
    // would disconnect them.
    const std::string p2p_parameters =
      R"({"listen_endpoint":"127.0.0.1:0","accept_incoming_connections":true,"active_ignored_request_timeout_microseconds":300000000})";
    initialize( 3, {}, { "initminer" }, { config_line_t( { "p2p-parameters", { p2p_parameters } } ) } );
    bool test_passed = false;
    fc::logger::get( "user" ).set_log_level( fc::log_level::info ); // suppress fast confirm broadcast messages
    // the reports are warnings; debug logging of each of the tens of thousands of advertisements would
    // also stretch their processing over more than one second, see below
    fc::logger::get( "p2p" ).set_log_level( fc::log_level::warn );

    fc::thread api_thread;
    auto _future = api_thread.async( [&]()
    {
      auto catcher = fc::shared_ptr< inventory_limit_report_catcher >( new inventory_limit_report_catcher() );
      fc::logger::get( "p2p" ).add_appender( catcher );

      std::mutex included_mutex;
      std::set< transaction_id_type > included;
      auto block_conn = db->add_post_apply_block_handler( [&]( const block_notification& note )
      {
        std::lock_guard< std::mutex > guard( included_mutex );
        for( const full_transaction_ptr& transaction : note.full_block->get_full_transactions() )
          included.insert( transaction->get_transaction_id() );
      }, get_chain_plugin(), 0 );

      BOOST_SCOPE_EXIT( this_, catcher, &block_conn )
      {
        fc::logger::get( "p2p" ).remove_appender( catcher );
        hive::utilities::disconnect_signal( block_conn );
        this_->theApp.generate_interrupt_request();
      } BOOST_SCOPE_EXIT_END

      const auto count_included = [&]( const std::vector< full_transaction_ptr >& transactions )
      {
        std::lock_guard< std::mutex > guard( included_mutex );
        size_t count = 0;
        for( const full_transaction_ptr& transaction : transactions )
          count += included.count( transaction->get_transaction_id() );
        return count;
      };
      const auto wait_until = [&]( const std::function< bool() >& condition, fc::time_point deadline )
      {
        while( !condition() )
        {
          if( fc::time_point::now() >= deadline || theApp.is_interrupt_request() )
            return false;
          fc::usleep( fc::milliseconds( 100 ) );
        }
        return true;
      };
      const auto last_report = [&]() { return catcher->reports().back(); };

      try
      {
        fc::sleep_until( get_genesis_time() + 2 * HIVE_BLOCK_INTERVAL );
        BOOST_REQUIRE_GT( get_block_num(), 0u );

        const fc::ip::endpoint node_endpoint =
          theApp.get_plugin< hive::plugins::p2p::p2p_plugin >().get_info()[ "listening_on" ].as< fc::ip::endpoint >();
        ilog( "node under test listens at ${ep}", ( "ep", node_endpoint ) );

        fc::thread peer_thread( "test_peers" );
        test_peer honest_peer( peer_thread, db->get_chain_id(), "honest peer" );
        honest_peer.connect( node_endpoint );
        BOOST_REQUIRE( wait_until( [&]() { return honest_peer.in_sync(); }, fc::time_point::now() + fc::seconds( 10 ) ) );

        std::vector< full_transaction_ptr > deferred_transactions;
        std::vector< full_transaction_ptr > honest_transactions;
        for( int i = 0; i < 20; ++i )
        {
          deferred_transactions.push_back( build_transfer( HIVE_INIT_MINER_NAME, HIVE_NULL_ACCOUNT, ASSET( "0.001 TESTS" ),
                                                           "deferred " + std::to_string( i ), get_default_expiration() ) );
          honest_transactions.push_back( build_transfer( HIVE_INIT_MINER_NAME, HIVE_NULL_ACCOUNT, ASSET( "0.001 TESTS" ),
                                                         "honest " + std::to_string( i ), get_default_expiration() ) );
        }

        // One more than the limit fills the flooding peer's inventory, the rest of the flood is deferred.
        // The node timestamps both in seconds, so when it processes the whole flood within one second
        // (it starts right after a second begins) the deferred hashes are as old as the inventory: when
        // that expires they expire with it, rather than being admitted into the room it leaves.
        // When the processing ends in a later second (on a loaded host), how many deferred hashes expire
        // rather than being admitted depends on when the node next happens to expire inventory, and an
        // admission alone is not reported, so the flood is repeated once by a new flooding peer, whose
        // inventory and deferred list start empty. The node reports the deferral after it stamped the whole
        // flood, so the flood stayed within one second if the report is logged in the second it started.
        const uint32_t never_delivered = 500;
        std::unique_ptr< test_peer > flooding_peer;
        fc::time_point flood_time;
        size_t first_report = 0; // where the reports of the current flooding peer start
        for( int attempt = 1; ; ++attempt )
        {
          flooding_peer = std::make_unique< test_peer >( peer_thread, db->get_chain_id(), "flooding peer" );
          flooding_peer->connect( node_endpoint );
          BOOST_REQUIRE( wait_until( [&]() { return flooding_peer->in_sync(); }, fc::time_point::now() + fc::seconds( 10 ) ) );

          std::vector< item_hash_t > flood;
          for( uint32_t i = 0; i < inventory_limit + 1 + never_delivered; ++i )
            flood.push_back( fc::ripemd160::hash( "never delivered " + std::to_string( attempt ) + " " + std::to_string( i ) ) );
          first_report = catcher->reports().size();
          fc::sleep_until( fc::time_point( fc::time_point_sec( fc::time_point::now() ) ) + fc::seconds( 1 ) );
          flood_time = fc::time_point::now();
          flooding_peer->advertise( flood );

          BOOST_REQUIRE_MESSAGE( wait_until( [&]() { return catcher->reports().size() > first_report; }, flood_time + fc::seconds( 10 ) ),
                                 "the node did not report advertisements over its inventory limit" );
          const inventory_limit_report report = last_report();
          BOOST_CHECK( report.peer == flooding_peer->endpoint() );
          BOOST_CHECK_EQUAL( report.deferred, never_delivered );
          BOOST_CHECK_EQUAL( report.dropped, 0u );
          BOOST_CHECK_EQUAL( report.still_deferred, never_delivered );

          if( fc::time_point_sec( report.time ) == fc::time_point_sec( flood_time ) )
            break;
          ilog( "flood ${attempt} started at ${start} and was reported at ${end}, across a second boundary",
                ( "attempt", attempt )( "start", flood_time )( "end", report.time ) );
          BOOST_REQUIRE_MESSAGE( attempt < 2, "the node processed the flood across a second boundary twice" );
          flooding_peer.reset();
        }

        // these arrive while earlier advertisements are still deferred, so they are deferred too
        fc::sleep_until( flood_time + fc::seconds( 5 ) );
        flooding_peer->provide( deferred_transactions );
        flooding_peer->advertise( hashes_of( deferred_transactions ) );

        honest_peer.provide( honest_transactions );
        honest_peer.advertise( hashes_of( honest_transactions ) );
        BOOST_REQUIRE_MESSAGE( wait_until( [&]() { return count_included( honest_transactions ) == honest_transactions.size(); },
                                           fc::time_point::now() + fc::seconds( 30 ) ),
                               "transactions of the honest peer were not included while the flooding peer was over its limit" );
        BOOST_CHECK_EQUAL( count_included( deferred_transactions ), 0u );

        BOOST_REQUIRE_MESSAGE( wait_until( [&]() { return last_report().still_deferred == deferred_transactions.size() + never_delivered; },
                                           flood_time + fc::seconds( 30 ) ),
                               "the node did not report the later advertisements as deferred" );
        BOOST_CHECK_EQUAL( last_report().deferred, deferred_transactions.size() );
        BOOST_CHECK_EQUAL( last_report().dropped, 0u );
        BOOST_CHECK_EQUAL( count_included( deferred_transactions ), 0u );

        // When the inventory window passes, the flood expires from the inventory together with the hashes
        // deferred along with it, and the later deferred transactions are admitted into the room left.
        BOOST_REQUIRE_MESSAGE( wait_until( [&]() { return last_report().still_deferred == 0; },
                                           flood_time + fc::minutes( GRAPHENE_NET_MAX_INVENTORY_SIZE_IN_MINUTES ) + fc::seconds( 20 ) ),
                               "advertisements were still deferred after the inventory window passed" );
        BOOST_CHECK_EQUAL( last_report().deferred, 0u );
        BOOST_CHECK_EQUAL( last_report().dropped, never_delivered );
        BOOST_CHECK_EQUAL( count_included( deferred_transactions ), 0u );

        // the node is still waiting for the first flooded transactions it asked for; once the peer tells
        // it they are not available, it fetches the admitted ones
        flooding_peer->stop_withholding();
        BOOST_REQUIRE_MESSAGE( wait_until( [&]() { return count_included( deferred_transactions ) == deferred_transactions.size(); },
                                           fc::time_point::now() + fc::seconds( 30 ) ),
                               "deferred transactions were not fetched and included once there was room for them" );

        const std::vector< inventory_limit_report > all_reports = catcher->reports();
        const std::vector< inventory_limit_report > reports( all_reports.begin() + first_report, all_reports.end() );
        for( const inventory_limit_report& report : reports )
        {
          BOOST_CHECK( report.peer == flooding_peer->endpoint() );
          BOOST_CHECK_LE( report.still_deferred, inventory_limit );
        }
        BOOST_CHECK_EQUAL( reports.back().still_deferred, 0u );
        BOOST_CHECK( !flooding_peer->closed() );
        BOOST_CHECK( !honest_peer.closed() );

        ilog( "'API' thread finished" );
        test_passed = !theApp.is_interrupt_request();
      }
      CATCH( "API" )
    } );

    _future.wait();
    theApp.wait4interrupt_request();
    theApp.quit( true );
    db = nullptr; // prevent fixture destructor from accessing database after it was closed
    BOOST_REQUIRE( test_passed );
    ilog( "Test done" );
  }
  FC_LOG_AND_RETHROW()
}

/// Peers ask the node for blocks the way a syncing node does. One request names blocks in order with an id
/// repeated and an id the node doesn't have among them: each block arrives once, in the order asked, and the
/// unknown id is reported unavailable in its place. A peer that asks for more blocks than its send queue holds
/// (GRAPHENE_NET_MAXIMUM_QUEUED_MESSAGES_IN_BYTES) is disconnected, and the node keeps serving the first peer.
BOOST_AUTO_TEST_CASE( block_request_test )
{
  try
  {
    BOOST_SCOPE_EXIT( this_ )
    {
      this_->theApp.generate_interrupt_request();
      this_->theApp.wait4interrupt_request();
      this_->theApp.quit( true );
    } BOOST_SCOPE_EXIT_END

    const std::string p2p_parameters = R"({"listen_endpoint":"127.0.0.1:0","accept_incoming_connections":true})";
    initialize( 3, {}, { "initminer" }, { config_line_t( { "p2p-parameters", { p2p_parameters } } ) } );
    bool test_passed = false;
    fc::logger::get( "user" ).set_log_level( fc::log_level::info ); // suppress fast confirm broadcast messages

    fc::thread api_thread;
    auto _future = api_thread.async( [&]()
    {
      BOOST_SCOPE_EXIT( this_ )
      {
        this_->theApp.generate_interrupt_request();
      } BOOST_SCOPE_EXIT_END

      const auto wait_until = [&]( const std::function< bool() >& condition, fc::time_point deadline )
      {
        while( !condition() )
        {
          if( fc::time_point::now() >= deadline || theApp.is_interrupt_request() )
            return false;
          fc::usleep( fc::milliseconds( 100 ) );
        }
        return true;
      };

      try
      {
        const uint32_t block_count = 10;
        BOOST_REQUIRE( wait_until( [&]() { return get_block_num() >= block_count; },
                                   get_genesis_time() + ( block_count + 5 ) * HIVE_BLOCK_INTERVAL ) );
        std::vector< block_id_type > block_ids;
        for( uint32_t block_num = 1; block_num <= block_count; ++block_num )
          block_ids.push_back( get_block_reader().find_block_id_for_num( block_num ) );

        const fc::ip::endpoint node_endpoint =
          theApp.get_plugin< hive::plugins::p2p::p2p_plugin >().get_info()[ "listening_on" ].as< fc::ip::endpoint >();
        fc::thread peer_thread( "test_peers" );
        test_peer syncing_peer( peer_thread, db->get_chain_id(), "syncing peer" );
        syncing_peer.connect( node_endpoint );
        BOOST_REQUIRE( wait_until( [&]() { return syncing_peer.in_sync(); }, fc::time_point::now() + fc::seconds( 10 ) ) );

        const item_hash_t unknown_id = fc::ripemd160::hash( std::string( "no such block" ) );
        std::vector< item_hash_t > request( block_ids.begin(), block_ids.begin() + 5 );
        request.push_back( block_ids[ 2 ] );
        request.push_back( unknown_id );
        request.insert( request.end(), block_ids.begin() + 5, block_ids.end() );
        request.push_back( block_ids[ 0 ] );
        syncing_peer.request_blocks( request );

        BOOST_REQUIRE_MESSAGE( wait_until( [&]() { return syncing_peer.blocks_received().size() >= block_count &&
                                                          syncing_peer.blocks_not_available().size() >= 1; },
                                           fc::time_point::now() + fc::seconds( 10 ) ),
                               "the node did not send the requested blocks" );
        fc::usleep( fc::seconds( 1 ) ); // anything sent for the repeated ids would have arrived by now
        BOOST_CHECK( syncing_peer.blocks_received() == block_ids );
        BOOST_REQUIRE_EQUAL( syncing_peer.blocks_not_available().size(), 1u );
        BOOST_CHECK( syncing_peer.blocks_not_available().front() == unknown_id );

        // more ids than the send queue of one peer holds: a queued block request takes more than the 35 bytes
        // GRAPHENE_NET_MAXIMUM_QUEUED_MESSAGES_IN_BYTES / greedy_count allows for each, while the request itself
        // (20 bytes per id) fits in one message and in the greedy peer's own send queue
        test_peer greedy_peer( peer_thread, db->get_chain_id(), "greedy peer" );
        greedy_peer.connect( node_endpoint );
        BOOST_REQUIRE( wait_until( [&]() { return greedy_peer.in_sync(); }, fc::time_point::now() + fc::seconds( 10 ) ) );
        std::vector< item_hash_t > greedy_request;
        const size_t greedy_count = 30000;
        for( size_t i = 0; i < greedy_count; ++i )
          greedy_request.push_back( fc::ripemd160::hash( "unknown block " + std::to_string( i ) ) );
        greedy_request.front() = block_ids.front();
        greedy_peer.request_blocks( greedy_request );
        BOOST_CHECK_MESSAGE( wait_until( [&]() { return greedy_peer.closed(); }, fc::time_point::now() + fc::seconds( 10 ) ),
                             "the node kept the connection of a peer that asked for more blocks than its send queue holds" );
        BOOST_CHECK_LT( greedy_peer.blocks_received().size() + greedy_peer.blocks_not_available().size(), greedy_count );

        syncing_peer.request_blocks( { block_ids.back() } );
        BOOST_CHECK_MESSAGE( wait_until( [&]() { return syncing_peer.blocks_received().size() == block_count + 1; },
                                         fc::time_point::now() + fc::seconds( 10 ) ),
                             "the node stopped serving the syncing peer" );
        BOOST_CHECK( !syncing_peer.closed() );

        ilog( "'API' thread finished" );
        test_passed = !theApp.is_interrupt_request();
      }
      CATCH( "API" )
    } );

    _future.wait();
    theApp.wait4interrupt_request();
    theApp.quit( true );
    db = nullptr; // prevent fixture destructor from accessing database after it was closed
    BOOST_REQUIRE( test_passed );
    ilog( "Test done" );
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_SUITE_END()

#endif
