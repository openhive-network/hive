#ifdef IS_TEST_NET
#include <boost/test/unit_test.hpp>

#include "../db_fixture/witness_fixture.hpp"

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

const uint32_t inventory_limit = GRAPHENE_NET_MAX_INVENTORY_SIZE_IN_MINUTES * GRAPHENE_NET_MAX_TRX_PER_SECOND * 60;

/// A p2p peer driven by the test rather than by a node. It completes the handshake as a node that is
/// in sync with the one under test, advertises the transaction hashes it is told to, delivers the
/// transactions it was given and either withholds or reports as unavailable anything else it is asked for.
/// Its connection lives on @p thread, like a node's connections live on the p2p thread.
class test_peer final : public graphene::net::peer_connection_delegate
{
public:
  test_peer( fc::thread& thread, const chain_id_type& chain_id, const std::string& user_agent )
    : _thread( thread ), _chain_id( chain_id ), _user_agent( user_agent ), _key( fc::ecc::private_key::generate() ) {}

  ~test_peer()
  {
    _thread.async( [this]()
    {
      if( _connection )
        _connection->destroy_connection( __FUNCTION__ );
      _connection.reset();
    } ).wait();
  }

  void connect( const fc::ip::endpoint& node_endpoint )
  {
    _thread.async( [&]()
    {
      _connection = graphene::net::peer_connection::make_shared( this );
      _connection->connect_to( node_endpoint );
      _local_endpoint = _connection->get_local_endpoint();
      send_hello();
    } ).wait();
  }

  /// true once the node asked us for blocks and we told it we have nothing it doesn't
  bool in_sync() const { return _in_sync; }
  bool closed() const { return _closed; }
  /// the endpoint the node sees this peer at
  const fc::ip::endpoint& endpoint() const { return _local_endpoint; }

  void provide( const std::vector< full_transaction_ptr >& transactions )
  {
    _thread.async( [&]()
    {
      for( const full_transaction_ptr& transaction : transactions )
        _deliverable[ transaction->get_legacy_transaction_message_hash() ] = transaction;
    } ).wait();
  }

  void advertise( const std::vector< item_hash_t >& hashes )
  {
    _thread.async( [&]()
    {
      // a connection closes when more than GRAPHENE_NET_MAXIMUM_QUEUED_MESSAGES_IN_BYTES wait to be sent,
      // so a flood goes out in several messages, each one sent before the next is queued
      const size_t max_hashes_per_message = GRAPHENE_NET_MAXIMUM_QUEUED_MESSAGES_IN_BYTES / 2 / sizeof( item_hash_t );
      for( size_t begin = 0; begin < hashes.size() && !_closed; begin += max_hashes_per_message )
      {
        const size_t end = std::min( hashes.size(), begin + max_hashes_per_message );
        const graphene::net::message inventory( graphene::net::item_ids_inventory_message( graphene::net::trx_message_type,
          std::vector< item_hash_t >( hashes.begin() + begin, hashes.begin() + end ) ) );
        const uint64_t bytes_sent_before = _connection->get_total_bytes_sent();
        _connection->send_message( inventory );
        while( _connection->get_total_bytes_sent() < bytes_sent_before + inventory.size && !_closed )
          fc::usleep( fc::milliseconds( 1 ) );
      }
    } ).wait();
  }

  /// from now on requests for items we can't deliver are answered right away (as unavailable),
  /// including the ones held so far
  void stop_withholding()
  {
    _thread.async( [&]()
    {
      _withholding = false;
      for( const graphene::net::item_id& item : _withheld )
        _connection->send_message( graphene::net::item_not_available_message( item ) );
      _withheld.clear();
    } ).wait();
  }

  void on_message( graphene::net::peer_connection* peer, const graphene::net::message& received_message ) override
  {
    using namespace graphene::net;
    switch( received_message.msg_type )
    {
    case core_message_type_enum::hello_message_type:
      peer->send_message( connection_accepted_message() );
      break;
    case core_message_type_enum::connection_accepted_message_type:
      peer->send_message( address_request_message() );
      break;
    case core_message_type_enum::address_request_message_type:
      peer->send_message( address_message() );
      break;
    case core_message_type_enum::fetch_blockchain_item_ids_message_type:
    {
      // answering with the last block of the node's synopsis tells it we have no blocks it doesn't
      const fetch_blockchain_item_ids_message request = received_message.as< fetch_blockchain_item_ids_message >();
      std::vector< item_hash_t > reply;
      if( !request.blockchain_synopsis.empty() )
        reply.push_back( request.blockchain_synopsis.back() );
      peer->send_message( blockchain_item_ids_inventory_message( 0, request.item_type, reply ) );
      _in_sync = true;
      break;
    }
    case core_message_type_enum::current_time_request_message_type:
      peer->send_message( current_time_reply_message( received_message.as< current_time_request_message >().request_sent_time,
                                                      fc::time_point::now(), fc::time_point::now() ) );
      break;
    case core_message_type_enum::fetch_items_message_type:
      on_fetch_items( received_message.as< fetch_items_message >() );
      break;
    case core_message_type_enum::connection_rejected_message_type:
    case core_message_type_enum::closing_connection_message_type:
      _closed = true;
      break;
    default:
      // the node's own advertisements, firewall checks, legacy duplicates of the messages above...
      break;
    }
  }

  void on_connection_closed( graphene::net::peer_connection* ) override { _closed = true; }

  std::shared_ptr< full_block_type > get_full_block_by_block_id( const block_id_type& ) override { return {}; }

private:
  void send_hello()
  {
    fc::sha256::encoder shared_secret_encoder;
    const fc::sha512 shared_secret = _connection->get_shared_secret();
    shared_secret_encoder.write( shared_secret.data(), sizeof( shared_secret ) );
    const graphene::net::node_id_t node_id = _key.get_public_key().serialize();
    fc::mutable_variant_object user_data;
    user_data[ "node_id" ] = node_id;
    user_data[ "chain_id" ] = _chain_id;
    _connection->send_message( graphene::net::hello_message( _user_agent, GRAPHENE_NET_PROTOCOL_VERSION,
                                                             _local_endpoint.get_address(), 0, _local_endpoint.port(), node_id,
                                                             _key.sign_compact( shared_secret_encoder.result() ), user_data ) );
  }

  void on_fetch_items( const graphene::net::fetch_items_message& request )
  {
    for( const item_hash_t& hash : request.items_to_fetch )
    {
      const graphene::net::item_id item( request.item_type, hash );
      auto deliverable = _deliverable.find( hash );
      if( request.item_type == graphene::net::trx_message_type && deliverable != _deliverable.end() )
        _connection->send_message( graphene::net::trx_message( deliverable->second ) );
      else if( _withholding )
        _withheld.push_back( item );
      else
        _connection->send_message( graphene::net::item_not_available_message( item ) );
    }
  }

  fc::thread& _thread;
  const chain_id_type _chain_id;
  const std::string _user_agent;
  const fc::ecc::private_key _key;
  graphene::net::peer_connection_ptr _connection;
  fc::ip::endpoint _local_endpoint;
  std::atomic< bool > _in_sync = false;
  std::atomic< bool > _closed = false;
  std::map< item_hash_t, full_transaction_ptr > _deliverable;
  bool _withholding = true;
  std::vector< graphene::net::item_id > _withheld;
};

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

BOOST_AUTO_TEST_SUITE_END()

#endif
