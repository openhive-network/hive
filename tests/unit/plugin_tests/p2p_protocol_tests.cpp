#ifdef IS_TEST_NET
#include <boost/test/unit_test.hpp>

#include "../db_fixture/witness_fixture.hpp"
#include "../db_fixture/p2p_test_peer.hpp"

#include <hive/chain/block_compression_dictionaries.hpp>
#include <hive/chain/notifications.hpp>
#include <hive/plugins/p2p/p2p_plugin.hpp>
#include <hive/utilities/tempdir.hpp>

#include <graphene/net/config.hpp>
#include <graphene/net/core_messages.hpp>

#include <algorithm>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

/**
 * One node under test, test peers talking to it message by message (#1093). Each case starts its own node,
 * so cases can't affect each other through node-wide state; within a case, independent scenarios share the
 * node, each on connections of its own.
 *
 * Most cases don't need the node to produce blocks, so they start an idle node: no witness and genesis in
 * the past, which saves waiting for genesis and block slots (start and shutdown take a few hundred ms). Blocks it
 * should have are generated at once with debug_node; blocks it should sync from a test peer are generated
 * by a short-lived source node with the same genesis (generate_chain()). An idle node is not at live sync,
 * so it ignores transaction advertisements: the cases that need it to fetch or relay transactions start a
 * producing node (the witness_fixture default, genesis 3 s ahead and a block every 3 s).
 *
 * Assertions use what the test peer received (replies, rejection codes, closing reasons) and when its
 * connection closed, then the node's view (connected peers) and its chain, never the logs.
 *
 * Cases marked (T) in their comment wait for one of the node's timeouts. They set the timeouts they can to
 * 1-2 s, assert "not before the timeout" and "by the timeout plus generous slack", and run sequentially.
 */

namespace {

using namespace p2p_tests;
using namespace graphene::net;
using hive::plugins::p2p::p2p_plugin;

using block_ptr = std::shared_ptr< hive::chain::full_block_type >;

/// generous deadline for anything the node does in response to a message
const fc::microseconds reply_timeout = fc::seconds( 15 );
/// how long a "nothing else happens" check waits
const fc::microseconds quiet_period = fc::seconds( 2 );

struct p2p_protocol_fixture : public witness_fixture
{
  p2p_protocol_fixture() : peer_thread( "test_peers" ) {}

  /**
   * Generates @p count empty blocks on a separate node with the genesis the next start_node() will use,
   * so the node under test can sync them from a test peer.
   */
  std::vector< block_ptr > generate_chain( uint32_t count )
  {
    choose_past_genesis();
    std::vector< block_ptr > blocks;
    {
      witness_fixture source;
      source.set_data_dir( ( hive::utilities::temp_directory_path() / "p2p-protocol-chain-source" ).string() );
      source.set_genesis_time( get_genesis_time() );
      source.initialize( 0, {}, {} );
      source.schedule_blocks( count );
      blocks = source.get_block_reader().fetch_block_range( 1, count );
      const fc::optional< fc::logging_config > logging_config = source.get_logging_config();
      if( logging_config )
        _source_logging_config = *logging_config;
      source.theApp.generate_interrupt_request();
      source.theApp.wait4interrupt_request();
      source.theApp.quit( true );
      source.db = nullptr;
    }
    BOOST_REQUIRE_EQUAL( blocks.size(), count );
    return blocks;
  }

  /**
   * Starts the node under test, listening on loopback. @p extra_p2p_parameters (JSON members, comma
   * separated) are added to the p2p-parameters. Unless @p produce_blocks, the node has no witness and its
   * genesis is in the past.
   */
  void start_node( const std::string& extra_p2p_parameters = "", bool produce_blocks = false )
  {
    if( _source_logging_config )
      set_logging_config( _source_logging_config );
    std::string p2p_parameters = R"({"listen_endpoint":"127.0.0.1:0","accept_incoming_connections":true)";
    if( !extra_p2p_parameters.empty() )
      p2p_parameters += "," + extra_p2p_parameters;
    p2p_parameters += "}";
    if( produce_blocks )
      initialize( 3, {}, { "initminer" }, { config_line_t( { "p2p-parameters", { p2p_parameters } } ) } );
    else
    {
      choose_past_genesis();
      initialize( 0, {}, {}, { config_line_t( { "p2p-parameters", { p2p_parameters } } ) } );
    }
    fc::logger::get( "user" ).set_log_level( fc::log_level::info );
  }

  /// Runs @p body on its own thread against the started node, then shuts the node down
  void run( const std::function< void() >& body )
  {
    BOOST_SCOPE_EXIT( this_ )
    {
      this_->theApp.generate_interrupt_request();
      this_->theApp.wait4interrupt_request();
      this_->theApp.quit( true );
    } BOOST_SCOPE_EXIT_END

    bool test_passed = false;
    fc::thread api_thread;
    auto done = api_thread.async( [&]()
    {
      BOOST_SCOPE_EXIT( this_ ) { this_->theApp.generate_interrupt_request(); } BOOST_SCOPE_EXIT_END
      try
      {
        body();
        test_passed = !theApp.is_interrupt_request();
      }
      CATCH( "API" )
    } );
    done.wait();
    theApp.wait4interrupt_request();
    theApp.quit( true );
    db = nullptr; // prevent fixture destructor from accessing database after it was closed
    BOOST_REQUIRE( test_passed );
  }

  p2p_plugin& p2p() { return theApp.get_plugin< p2p_plugin >(); }

  fc::ip::endpoint node_endpoint() { return p2p().get_info()[ "listening_on" ].as< fc::ip::endpoint >(); }

  std::unique_ptr< test_peer > make_peer( const std::string& name )
  {
    return std::make_unique< test_peer >( peer_thread, db->get_chain_id(), name );
  }

  /// Connects @p peer and waits until the handshake is done and the node asked it for block ids, i.e. it is active
  void connect_active( test_peer& peer )
  {
    peer.connect( node_endpoint() );
    BOOST_REQUIRE_MESSAGE( peer.wait_for( fetch_blockchain_item_ids_message_type, reply_timeout ), "the node did not take up the peer" );
  }

  /// Connects @p peer and waits until the node considers it in sync, so it advertises items to it
  void connect_in_sync( test_peer& peer )
  {
    connect_active( peer );
    BOOST_REQUIRE( wait_until( [&]() { return peer.in_sync(); }, reply_timeout ) );
    BOOST_REQUIRE( wait_until( [&]()
    {
      if( peer.node_considers_us_in_sync() )
        return true;
      peer.tell_head( head_block_id() );
      fc::usleep( fc::milliseconds( 200 ) );
      return peer.node_considers_us_in_sync();
    }, reply_timeout ) );
  }

  /// whether the node lists @p peer among its connections
  bool node_lists( const test_peer& peer )
  {
    const fc::ip::endpoint endpoint = peer.endpoint();
    const std::vector< hive::plugins::p2p::api_peer_status > peers = p2p().get_connected_peers();
    return std::any_of( peers.begin(), peers.end(), [&]( const hive::plugins::p2p::api_peer_status& p ) { return p.host == endpoint; } );
  }

  size_t number_of_connections() { return p2p().get_connected_peers().size(); }

  block_id_type head_block_id()
  {
    block_id_type head;
    db->with_read_lock( [&]() { head = db->head_block_id(); } );
    return head;
  }

  block_id_type block_id( uint32_t num ) { return get_block_reader().get_block_by_number( num )->get_block_id(); }

  /// Sends @p request and returns the node's next reply of type T::type
  template< typename T >
  T request( test_peer& peer, const message& request )
  {
    const size_t before = peer.count( T::type );
    peer.send( request );
    BOOST_REQUIRE_MESSAGE( peer.wait_for( T::type, reply_timeout, before + 1 ), "no reply of type " << T::type );
    return peer.received_as< T >()[ before ];
  }

  /// Requires the node to close @p peer's connection with a closing_connection_message whose reason contains @p reason
  static void require_closed_with( const test_peer& peer, const std::string& reason )
  {
    BOOST_REQUIRE_MESSAGE( peer.wait_closed( reply_timeout ), "the node did not close the connection" );
    const fc::optional< closing_connection_message > closing = peer.closing_message();
    BOOST_REQUIRE_MESSAGE( closing, "the node closed the connection without a closing_connection_message" );
    BOOST_CHECK_MESSAGE( closing->reason_for_closing.find( reason ) != std::string::npos,
                         "closing reason '" << closing->reason_for_closing << "' does not contain '" << reason << "'" );
  }

  /// Requires the node to reject @p peer's hello with @p code and then close the connection
  static void require_rejected( const test_peer& peer, rejection_reason_code code )
  {
    BOOST_REQUIRE_MESSAGE( peer.wait_closed( reply_timeout ), "the node did not close the connection" );
    const fc::optional< connection_rejected_message > rejection = peer.rejection();
    BOOST_REQUIRE_MESSAGE( rejection, "the node did not reject the hello" );
    BOOST_CHECK_EQUAL( static_cast< int >( rejection->reason_code.value ), static_cast< int >( code ) );
    BOOST_CHECK_EQUAL( peer.count( connection_accepted_message_type ), 0u );
  }

  /// Requires @p peer's connection to stay open for the quiet period
  static void require_kept( const test_peer& peer )
  {
    fc::usleep( quiet_period );
    BOOST_REQUIRE_MESSAGE( !peer.closed(), "the node closed the connection" );
  }

  fc::thread peer_thread;

private:
  void choose_past_genesis()
  {
    // far enough in the past for the blocks a case generates to be in the past too
    if( get_genesis_time() == fc::time_point_sec() )
      set_genesis_time( fc::time_point_sec( ( fc::time_point::now().sec_since_epoch() - 1200 ) / HIVE_BLOCK_INTERVAL * HIVE_BLOCK_INTERVAL ) );
  }

  fc::optional< fc::logging_config > _source_logging_config;
};

std::vector< char > garbage( size_t size, char value = '\xff' ) { return std::vector< char >( size, value ); }

block_ptr with_bad_signature( const block_ptr& block )
{
  hive::chain::signed_block copy = block->get_block();
  copy.witness_signature = fc::ecc::private_key::generate().sign_compact( fc::sha256::hash( std::string( "not this block" ) ) );
  return hive::chain::full_block_type::create_from_signed_block( copy );
}

} // namespace

// Disabled by default: the suite has its own CI job (plugin_test_p2p_protocol) and runs only when named,
// e.g. --run_test=p2p_protocol_tests, so plugin_test_api's --run_test=!witness_tests leaves it out
BOOST_FIXTURE_TEST_SUITE( p2p_protocol_tests, p2p_protocol_fixture, * boost::unit_test::disabled() )

// ---------------------------------------------------------------------------------------------------------
// A. Connection setup and handshake
// ---------------------------------------------------------------------------------------------------------

/// A1: an inbound connection goes through the key exchange, both hellos, connection_accepted and the address
/// exchange, then the node asks for block ids and lists the peer. The node's hello is signed over the shared
/// secret and carries the chain id, its node id, its head and its compression dictionary.
BOOST_AUTO_TEST_CASE( inbound_handshake_test )
{
  start_node();
  run( [&]()
  {
    auto peer = make_peer( "inbound peer" );
    connect_active( *peer );

    const std::vector< received_message > messages = peer->received();
    const auto position = [&]( uint32_t type )
    {
      return std::find_if( messages.begin(), messages.end(), [&]( const received_message& r ) { return r.msg.msg_type == type; } ) - messages.begin();
    };
    BOOST_CHECK_EQUAL( messages.front().msg.msg_type, static_cast< uint32_t >( hello_message_type ) );
    BOOST_CHECK_LT( position( hello_message_type ), position( connection_accepted_message_type ) );
    BOOST_CHECK_LT( position( connection_accepted_message_type ), position( fetch_blockchain_item_ids_message_type ) );
    BOOST_CHECK_LT( position( address_request_message_type ), position( fetch_blockchain_item_ids_message_type ) );
    BOOST_CHECK_LT( position( address_message_type ), messages.size() );
    BOOST_CHECK_EQUAL( peer->count( connection_accepted_message_type ), 1u );

    const hello_message hello = *peer->node_hello();
    fc::sha256::encoder encoder;
    const fc::sha512 secret = peer->shared_secret();
    encoder.write( secret.data(), sizeof( secret ) );
    BOOST_CHECK( fc::ecc::public_key( hello.signed_shared_secret, encoder.result() ).serialize() == hello.node_public_key );
    BOOST_CHECK_EQUAL( hello.core_protocol_version, static_cast< uint32_t >( GRAPHENE_NET_PROTOCOL_VERSION ) );
    BOOST_CHECK_EQUAL( hello.inbound_port, node_endpoint().port() );
    BOOST_CHECK( hello.user_data[ "chain_id" ].as< chain_id_type >() == db->get_chain_id() );
    BOOST_CHECK( hello.user_data.contains( "node_id" ) );
    BOOST_CHECK_EQUAL( hello.user_data[ "last_known_block_number" ].as< uint32_t >(), get_block_num() );
    const std::optional< uint8_t > dictionary = hive::chain::get_last_available_zstd_compression_dictionary_number();
    BOOST_REQUIRE( dictionary );
    BOOST_CHECK_EQUAL( hello.user_data[ "last_available_zstd_compression_dictionary_number" ].as< uint32_t >(), *dictionary );
    // so the peer may keep more than one transaction request outstanding with the node
    BOOST_CHECK( hello.user_data.contains( GRAPHENE_NET_HELLO_DEFERS_TRANSACTION_REPLIES ) &&
                 hello.user_data[ GRAPHENE_NET_HELLO_DEFERS_TRANSACTION_REPLIES ].as_bool() );
    // the node sends the legacy hello too, for peers that predate the IPv6-aware messages
    BOOST_CHECK_EQUAL( peer->count( legacy_hello_message_type ), 1u );

    BOOST_CHECK( wait_until( [&]() { return node_lists( *peer ); }, reply_timeout ) );
    require_kept( *peer );
  } );
}

/// A2: the node dials a listening peer it is given with add_node() and completes the same handshake
BOOST_AUTO_TEST_CASE( outbound_handshake_test )
{
  start_node();
  run( [&]()
  {
    auto peer = make_peer( "listening peer" );
    const fc::ip::endpoint listening = peer->listen();
    p2p().add_node( listening );
    // the connect loop may only run on its next 10 s tick
    BOOST_REQUIRE_MESSAGE( peer->wait_for( fetch_blockchain_item_ids_message_type, fc::seconds( 30 ) ), "the node did not dial the peer" );
    BOOST_CHECK_EQUAL( peer->count( connection_accepted_message_type ), 1u );
    const hello_message hello = *peer->node_hello();
    BOOST_CHECK_EQUAL( hello.inbound_port, node_endpoint().port() );
    BOOST_CHECK( wait_until( [&]() { return node_lists( *peer ); }, reply_timeout ) );
    require_kept( *peer );
  } );
}

/// A3: a peer that sends only the legacy hello is accepted and gets legacy address messages; a peer that
/// sends both hellos has the legacy one ignored (a second hello would be a protocol error)
BOOST_AUTO_TEST_CASE( legacy_hello_test )
{
  start_node();
  run( [&]()
  {
    auto legacy = make_peer( "legacy peer" );
    legacy->hello().hello_form = hello_options::form::legacy;
    connect_active( *legacy );
    BOOST_CHECK_EQUAL( legacy->count( connection_accepted_message_type ), 1u );
    BOOST_CHECK( legacy->wait_for( legacy_address_message_type, reply_timeout ) );
    BOOST_CHECK_EQUAL( legacy->count( address_message_type ), 0u );

    auto both = make_peer( "peer sending both hellos" );
    both->hello().hello_form = hello_options::form::both;
    connect_active( *both );
    BOOST_CHECK_EQUAL( both->count( connection_accepted_message_type ), 1u );
    BOOST_CHECK( both->wait_for( address_message_type, reply_timeout ) );

    require_kept( *legacy );
    require_kept( *both );
    BOOST_CHECK( node_lists( *legacy ) );
    BOOST_CHECK( node_lists( *both ) );
  } );
}

/// A4: a hello signed with a key other than the one it names is rejected as invalid
BOOST_AUTO_TEST_CASE( invalid_hello_signature_test )
{
  start_node();
  run( [&]()
  {
    auto peer = make_peer( "forging peer" );
    peer->hello().signing_key = fc::ecc::private_key::generate();
    peer->connect( node_endpoint() );
    require_rejected( *peer, rejection_reason_code::invalid_hello_message );
    require_closed_with( *peer, "Invalid signature in hello message" );
  } );
}

/// A5: protocol versions below the minimum are rejected as too old; the minimum and newer versions are accepted
BOOST_AUTO_TEST_CASE( protocol_version_test )
{
  start_node();
  run( [&]()
  {
    auto too_old = make_peer( "too old peer" );
    too_old->hello().core_protocol_version = GRAPHENE_NET_MINIMUM_PROTOCOL_VERSION - 1;
    too_old->connect( node_endpoint() );
    require_rejected( *too_old, rejection_reason_code::client_too_old );

    auto oldest = make_peer( "oldest accepted peer" );
    oldest->hello().core_protocol_version = GRAPHENE_NET_MINIMUM_PROTOCOL_VERSION;
    connect_active( *oldest );

    auto newer = make_peer( "newer peer" );
    newer->hello().core_protocol_version = GRAPHENE_NET_PROTOCOL_VERSION + 1;
    connect_active( *newer );

    require_kept( *oldest );
    require_kept( *newer );
  } );
}

/// A6: a hello for another chain, or for no chain at all, is rejected as different_chain
BOOST_AUTO_TEST_CASE( different_chain_test )
{
  start_node();
  run( [&]()
  {
    auto other = make_peer( "other chain peer" );
    other->hello().chain_id = chain_id_type( fc::sha256::hash( std::string( "another chain" ) ) );
    other->connect( node_endpoint() );
    require_rejected( *other, rejection_reason_code::different_chain );

    auto none = make_peer( "chainless peer" );
    none->hello().chain_id.reset();
    none->connect( node_endpoint() );
    require_rejected( *none, rejection_reason_code::different_chain );
  } );
}

/// A7: a hello carrying the node's own node id is rejected as connected_to_self
BOOST_AUTO_TEST_CASE( connected_to_self_test )
{
  start_node();
  run( [&]()
  {
    auto probe = make_peer( "probe" );
    connect_active( *probe );
    const node_id_t node_id = probe->node_hello()->user_data[ "node_id" ].as< node_id_t >();

    auto mirror = make_peer( "mirror" );
    mirror->hello().node_id = node_id;
    mirror->connect( node_endpoint() );
    require_rejected( *mirror, rejection_reason_code::connected_to_self );
    require_kept( *probe );
  } );
}

/// A8: a second connection from a node id that is already connected is rejected as already_connected, and
/// the first connection survives. Of two connections between the same ids, the node with the lower id keeps
/// the choice: the peer's id here orders after the node's, so the node rejects (#932).
BOOST_AUTO_TEST_CASE( already_connected_test )
{
  start_node();
  run( [&]()
  {
    node_id_t node_id;
    {
      auto probe = make_peer( "probe" );
      connect_active( *probe );
      node_id = probe->node_hello()->user_data[ "node_id" ].as< node_id_t >();
    }
    BOOST_REQUIRE( wait_until( [&]() { return number_of_connections() == 0; }, reply_timeout ) );

    // the node's id is random bytes, not a key; ids compare bytewise, so the next number orders after it
    node_id_t duplicate_id = node_id;
    for( size_t i = sizeof( duplicate_id.data ); i-- > 0 && ++reinterpret_cast< unsigned char& >( duplicate_id.data[ i ] ) == 0; )
      ;
    BOOST_REQUIRE( node_id < duplicate_id );

    auto first = make_peer( "first connection" );
    first->hello().node_id = duplicate_id;
    connect_active( *first );
    auto second = make_peer( "second connection" );
    second->hello().node_id = duplicate_id;
    second->connect( node_endpoint() );
    require_rejected( *second, rejection_reason_code::already_connected );

    require_kept( *first );
    BOOST_CHECK( node_lists( *first ) );
    BOOST_CHECK_EQUAL( number_of_connections(), 1u );
  } );
}

/// A10: a second hello on an accepted connection ends it
BOOST_AUTO_TEST_CASE( second_hello_test )
{
  start_node();
  run( [&]()
  {
    auto peer = make_peer( "repeating peer" );
    connect_active( *peer );
    peer->send_hello();
    require_closed_with( *peer, "unexpected hello" );
    BOOST_CHECK( wait_until( [&]() { return !node_lists( *peer ); }, reply_timeout ) );
  } );
}

/// A11: a peer that rejects the node's hello, for each reason code, on inbound and outbound connections:
/// the node asks for addresses and then closes the connection, except after connected_to_self, which it
/// closes at once
BOOST_AUTO_TEST_CASE( peer_rejects_node_test )
{
  start_node();
  run( [&]()
  {
    const std::vector< rejection_reason_code > codes = {
      rejection_reason_code::unspecified, rejection_reason_code::different_chain, rejection_reason_code::already_connected,
      rejection_reason_code::connected_to_self, rejection_reason_code::not_accepting_connections, rejection_reason_code::blocked,
      rejection_reason_code::invalid_hello_message, rejection_reason_code::client_too_old };
    const auto rejecting_peer = [&]( rejection_reason_code code, const std::string& name )
    {
      auto peer = make_peer( name );
      peer->on( hello_message_type, [code]( test_peer& self, const message& )
      {
        self.send( connection_rejected_message( "test peer", GRAPHENE_NET_PROTOCOL_VERSION, self.endpoint(), code, "rejected by the test" ) );
      } );
      return peer;
    };
    const auto check = [&]( const test_peer& peer, rejection_reason_code code )
    {
      BOOST_TEST_CONTEXT( "reason code " << static_cast< int >( code ) )
      {
        if( code == rejection_reason_code::connected_to_self )
          BOOST_CHECK( peer.wait_closed( reply_timeout ) );
        else
          require_closed_with( peer, "You rejected my connection request" );
      }
    };

    std::vector< std::unique_ptr< test_peer > > inbound;
    for( rejection_reason_code code : codes )
    {
      inbound.push_back( rejecting_peer( code, "inbound rejecting peer" ) );
      inbound.back()->connect( node_endpoint() );
    }
    for( size_t i = 0; i < codes.size(); ++i )
      check( *inbound[ i ], codes[ i ] );

    std::vector< std::unique_ptr< test_peer > > outbound;
    for( rejection_reason_code code : { rejection_reason_code::not_accepting_connections, rejection_reason_code::already_connected } )
    {
      outbound.push_back( rejecting_peer( code, "outbound rejecting peer" ) );
      p2p().add_node( outbound.back()->listen() );
    }
    BOOST_REQUIRE( wait_until( [&]() { return std::all_of( outbound.begin(), outbound.end(), []( const auto& p ) { return p->connected(); } ); },
                               fc::seconds( 30 ) ) );
    check( *outbound[ 0 ], rejection_reason_code::not_accepting_connections );
    check( *outbound[ 1 ], rejection_reason_code::already_connected );
  } );
}

/// A12 (T): a connection that stalls in the handshake is closed once peer_inactivity_timeout (2 s here) passes,
/// not before: after the TCP connect with no key exchange, after the key exchange with no hello, and after
/// the hello with no reply to the node's hello
BOOST_AUTO_TEST_CASE( handshake_inactivity_test )
{
  const uint32_t timeout = 2;
  start_node( R"("peer_inactivity_timeout":)" + std::to_string( timeout ) );
  run( [&]()
  {
    raw_connection no_key_exchange( peer_thread );
    const fc::time_point tcp_connected = fc::time_point::now();
    no_key_exchange.connect_plain( node_endpoint() );

    auto no_hello = make_peer( "silent after key exchange" );
    no_hello->hello().count = 0;
    no_hello->ignore( hello_message_type );
    no_hello->ignore( legacy_hello_message_type );
    no_hello->connect( node_endpoint() );

    auto no_reply = make_peer( "silent after hello" );
    no_reply->ignore( hello_message_type );
    no_reply->ignore( legacy_hello_message_type );
    no_reply->connect( node_endpoint() );

    // the node checks once a second; the slack is for a loaded host
    const fc::microseconds latest = fc::seconds( timeout + 1 + 10 );
    BOOST_REQUIRE( no_key_exchange.wait_closed( latest ) );
    BOOST_REQUIRE( no_hello->wait_closed( latest ) );
    BOOST_REQUIRE( no_reply->wait_closed( latest ) );
    // the node's timestamps are in whole seconds
    const fc::microseconds earliest = fc::seconds( timeout ) - fc::seconds( 1 );
    BOOST_CHECK_GE( ( no_key_exchange.closed_at() - tcp_connected ).count(), earliest.count() );
    BOOST_CHECK_GE( ( no_hello->closed_at() - no_hello->connected_at() ).count(), earliest.count() );
    BOOST_CHECK_GE( ( no_reply->closed_at() - no_reply->connected_at() ).count(), earliest.count() );
    BOOST_CHECK_EQUAL( no_reply->count( connection_accepted_message_type ), 1u );
  } );
}

/// A13: a peer that skips its hello, here one from another chain (the hello is where the node would learn
/// that), gets answers only to the requests allowed before a hello: time and addresses. Its requests for
/// block ids and blocks go unanswered, and neither they nor addresses make it an active connection that the
/// node syncs with or advertises blocks to. A peer that sends its hello first and its request right after,
/// without waiting for connection_accepted, is answered.
BOOST_AUTO_TEST_CASE( messages_before_hello_test )
{
  start_node( "", true );
  run( [&]()
  {
    fc::sleep_until( get_genesis_time() + HIVE_BLOCK_INTERVAL );
    wait_for_block_num( 1 );

    auto listening = make_peer( "listening peer" );
    connect_in_sync( *listening );

    auto skipping = make_peer( "other chain peer skipping its hello" );
    skipping->hello().chain_id = chain_id_type( fc::sha256::hash( std::string( "another chain" ) ) );
    skipping->hello().count = 0;
    skipping->ignore( hello_message_type );
    skipping->ignore( legacy_hello_message_type );
    skipping->connect( node_endpoint() );

    const current_time_reply_message time = request< current_time_reply_message >( *skipping, current_time_request_message( fc::time_point::now() ) );
    BOOST_CHECK( time.request_received_time != fc::time_point() );

    // the node learns from the hello that a peer understands the IPv6-aware messages, so without one it
    // answers in the legacy form
    request< legacy_address_message >( *skipping, address_request_message() );
    BOOST_CHECK_EQUAL( skipping->count( address_message_type ), 0u );

    skipping->send( fetch_blockchain_item_ids_message( block_message_type, {} ) );
    skipping->send( fetch_items_message( block_message_type, { block_id( 1 ) } ) );
    skipping->send( address_message() );

    // a block the node advertises to its active peers while the skipping peer waits
    const uint32_t next = get_block_num() + 1;
    wait_for_block_num( next );
    const block_id_type id = block_id( next );
    BOOST_REQUIRE( wait_until( [&]()
    {
      for( const item_ids_inventory_message& inventory : listening->received_as< item_ids_inventory_message >() )
        if( std::find( inventory.item_hashes_available.begin(), inventory.item_hashes_available.end(), id ) != inventory.item_hashes_available.end() )
          return true;
      return false;
    }, reply_timeout ) );
    fc::usleep( quiet_period );

    BOOST_CHECK_EQUAL( skipping->count( blockchain_item_ids_inventory_message_type ), 0u );
    BOOST_CHECK_EQUAL( skipping->count( block_message_type ), 0u );
    BOOST_CHECK_EQUAL( skipping->count( compressed_block_message_type ), 0u );
    BOOST_CHECK_EQUAL( skipping->count( item_not_available_message_type ), 0u );
    BOOST_CHECK_EQUAL( skipping->count( item_ids_inventory_message_type ), 0u );
    // the node asks a peer for block ids once it takes it up
    BOOST_CHECK_EQUAL( skipping->count( fetch_blockchain_item_ids_message_type ), 0u );
    BOOST_CHECK_EQUAL( skipping->count( connection_accepted_message_type ), 0u );
    BOOST_CHECK( !node_lists( *skipping ) );
    BOOST_CHECK( node_lists( *listening ) );

    auto eager = make_peer( "eager legacy peer" );
    eager->hello().hello_form = hello_options::form::legacy;
    eager->connect( node_endpoint() );
    const blockchain_item_ids_inventory_message ids =
      request< blockchain_item_ids_inventory_message >( *eager, fetch_blockchain_item_ids_message( block_message_type, {} ) );
    BOOST_CHECK( !ids.item_hashes_available.empty() );
    BOOST_CHECK( eager->accepted() );
    BOOST_CHECK( wait_until( [&]() { return node_lists( *eager ); }, reply_timeout ) );
    require_kept( *eager );
  } );
}

// ---------------------------------------------------------------------------------------------------------
// B. Framing and decoding
// ---------------------------------------------------------------------------------------------------------

/// B1: a message header claiming more than MAX_MESSAGE_SIZE ends that connection, and only that one
BOOST_AUTO_TEST_CASE( oversized_message_header_test )
{
  start_node();
  run( [&]()
  {
    auto bystander = make_peer( "bystander" );
    connect_active( *bystander );

    raw_connection raw( peer_thread );
    raw.connect_secure( node_endpoint() );
    raw.write( raw_connection::frame( MAX_MESSAGE_SIZE + 1, current_time_request_message_type ) );
    BOOST_REQUIRE( raw.wait_closed( reply_timeout ) );

    require_kept( *bystander );
    BOOST_CHECK( node_lists( *bystander ) );
  } );
}

/// B2: a payload that doesn't decode as its core message type ends the connection, without a closing
/// message (the exception ends the read loop), for every core message type the node decodes. Truncated
/// payloads don't decode for any type; 40 bytes of 0xff decode for the fixed-size types (whose extra
/// bytes are ignored) but not for those that start with a length, which then claims far more than follows.
/// legacy_hello is left out: on a connection that sent a hello it is ignored undecoded (see A3).
BOOST_AUTO_TEST_CASE( undecodable_payload_test )
{
  start_node();
  run( [&]()
  {
    auto bystander = make_peer( "bystander" );
    connect_active( *bystander );

    const std::vector< uint32_t > fixed_size_types = {
      item_not_available_message_type, current_time_request_message_type, current_time_reply_message_type,
      legacy_check_firewall_message_type, legacy_check_firewall_reply_message_type };
    const std::vector< uint32_t > length_prefixed_types = {
      item_ids_inventory_message_type, blockchain_item_ids_inventory_message_type, fetch_blockchain_item_ids_message_type,
      fetch_items_message_type, address_message_type, legacy_address_message_type, legacy_connection_rejected_message_type,
      connection_rejected_message_type, check_firewall_message_type, check_firewall_reply_message_type,
      get_current_connections_reply_message_type, hello_message_type, trx_message_type };
    struct attempt
    {
      uint32_t type;
      std::vector< char > payload;
      bool closes;
      std::unique_ptr< test_peer > peer;
    };
    std::vector< attempt > attempts;
    for( uint32_t type : fixed_size_types )
    {
      attempts.push_back( { type, {}, true, nullptr } );
      attempts.push_back( { type, garbage( 3 ), true, nullptr } );
      attempts.push_back( { type, garbage( 40 ), false, nullptr } );
    }
    for( uint32_t type : length_prefixed_types )
    {
      attempts.push_back( { type, {}, true, nullptr } );
      attempts.push_back( { type, garbage( 3 ), true, nullptr } );
      attempts.push_back( { type, garbage( 40 ), true, nullptr } );
    }
    for( attempt& a : attempts )
    {
      a.peer = make_peer( "garbling peer" );
      connect_active( *a.peer );
      a.peer->send_raw( a.type, a.payload );
    }
    fc::usleep( quiet_period );
    for( const attempt& a : attempts )
      BOOST_TEST_CONTEXT( "message type " << a.type << ", " << a.payload.size() << " bytes" )
      {
        if( a.closes )
        {
          BOOST_CHECK_MESSAGE( a.peer->wait_closed( reply_timeout ), "the node kept the connection" );
          BOOST_CHECK( !a.peer->closing_message() );
        }
        else
          BOOST_CHECK_MESSAGE( !a.peer->closed(), "the node closed the connection" );
      }
    BOOST_CHECK( !bystander->closed() );
    BOOST_CHECK( node_lists( *bystander ) );
  } );
}

/// B3: a message type the node doesn't know is ignored, inside the core range and outside it
BOOST_AUTO_TEST_CASE( unknown_message_type_test )
{
  start_node();
  run( [&]()
  {
    auto peer = make_peer( "inventive peer" );
    connect_active( *peer );
    peer->send_raw( core_message_type_last - 1, garbage( 16 ) );
    peer->send_raw( core_message_type_last + 1, garbage( 16 ) );
    peer->send_raw( 1234, garbage( 16 ) );
    require_kept( *peer );
    // and the connection still works
    request< current_time_reply_message >( *peer, current_time_request_message( fc::time_point::now() ) );
  } );
}

/// B4: a connection dropped in the middle of a message, or without a closing message, is closed by the node
/// too, without affecting other peers
BOOST_AUTO_TEST_CASE( dropped_connection_test )
{
  start_node();
  run( [&]()
  {
    auto bystander = make_peer( "bystander" );
    connect_active( *bystander );

    auto quitter = make_peer( "quitter" );
    connect_active( *quitter );
    BOOST_REQUIRE( wait_until( [&]() { return node_lists( *quitter ); }, reply_timeout ) );
    quitter->close();
    BOOST_CHECK( wait_until( [&]() { return !node_lists( *quitter ); }, reply_timeout ) );

    raw_connection truncated( peer_thread );
    truncated.connect_secure( node_endpoint() );
    // a header announcing 1000 bytes and only the first few of them
    truncated.write( raw_connection::frame( 1000, current_time_request_message_type, garbage( 8, 0 ) ) );
    truncated.close();

    require_kept( *bystander );
    BOOST_CHECK_EQUAL( number_of_connections(), 1u );
  } );
}

/// B5: a closing_connection_message, with or without closing_due_to_error and also one that doesn't decode,
/// is a close request: the node records it and leaves closing the socket to the peer (it closes it itself
/// only when it asked first), so the connection stays until the peer closes it, and then it is gone. An
/// undecodable one doesn't end the connection as a decoding error would (B2).
BOOST_AUTO_TEST_CASE( closing_connection_message_test )
{
  start_node();
  run( [&]()
  {
    std::vector< std::unique_ptr< test_peer > > peers;
    for( int i = 0; i < 3; ++i )
    {
      peers.push_back( make_peer( "closing peer" ) );
      connect_active( *peers.back() );
    }
    BOOST_REQUIRE( wait_until( [&]() { return number_of_connections() == peers.size(); }, reply_timeout ) );
    peers[ 0 ]->send( closing_connection_message( "bye" ) );
    peers[ 1 ]->send( closing_connection_message( "bye", true, fc::exception( FC_LOG_MESSAGE( warn, "the test is done with you" ) ) ) );
    peers[ 2 ]->send_raw( closing_connection_message_type, garbage( 40 ) );
    for( const auto& peer : peers )
      require_kept( *peer );
    for( const auto& peer : peers )
      peer->close();
    BOOST_CHECK( wait_until( [&]() { return number_of_connections() == 0; }, reply_timeout ) );
  } );
}

// ---------------------------------------------------------------------------------------------------------
// C. Node as a server
// ---------------------------------------------------------------------------------------------------------

/// C1: fetch_blockchain_item_ids is answered with the ids after the newest block of the synopsis the node
/// has: from block 1 for an empty synopsis, just the head when the synopsis ends there, nothing when it
/// knows none of them
BOOST_AUTO_TEST_CASE( fetch_blockchain_item_ids_test )
{
  start_node();
  run( [&]()
  {
    const uint32_t blocks = 20;
    schedule_blocks( blocks );
    BOOST_REQUIRE_EQUAL( get_block_num(), blocks );

    auto peer = make_peer( "asking peer" );
    connect_active( *peer );
    const auto ask = [&]( const std::vector< item_hash_t >& synopsis )
    {
      return request< blockchain_item_ids_inventory_message >( *peer, fetch_blockchain_item_ids_message( block_message_type, synopsis ) );
    };

    blockchain_item_ids_inventory_message reply = ask( {} );
    BOOST_REQUIRE_EQUAL( reply.item_hashes_available.size(), blocks );
    BOOST_CHECK( reply.item_hashes_available.front() == block_id( 1 ) );
    BOOST_CHECK( reply.item_hashes_available.back() == block_id( blocks ) );
    BOOST_CHECK_EQUAL( reply.total_remaining_item_count, 0u );

    reply = ask( { block_id( 5 ), block_id( blocks ) } );
    BOOST_REQUIRE_EQUAL( reply.item_hashes_available.size(), 1u );
    BOOST_CHECK( reply.item_hashes_available.front() == block_id( blocks ) );

    const item_hash_t unknown = fc::ripemd160::hash( std::string( "unknown block" ) );
    reply = ask( { block_id( 5 ), block_id( 10 ), unknown } );
    BOOST_REQUIRE_EQUAL( reply.item_hashes_available.size(), blocks - 9 );
    BOOST_CHECK( reply.item_hashes_available.front() == block_id( 10 ) );

    reply = ask( { unknown } );
    BOOST_CHECK( reply.item_hashes_available.empty() );
    require_kept( *peer );
  } );
}

/// C2: blocks are served by id, in the form the peer understands: compressed to a peer of the current
/// protocol, uncompressed to a protocol 106 peer; unknown ids are answered with item_not_available
BOOST_AUTO_TEST_CASE( fetch_blocks_test )
{
  start_node();
  run( [&]()
  {
    schedule_blocks( 5 );

    auto current = make_peer( "current peer" );
    connect_active( *current );
    current->send( fetch_items_message( block_message_type, { block_id( 3 ), block_id( 4 ) } ) );
    BOOST_REQUIRE( current->wait_for( compressed_block_message_type, reply_timeout, 2 ) );
    BOOST_CHECK_EQUAL( current->count( block_message_type ), 0u );

    auto old = make_peer( "protocol 106 peer" );
    old->hello().core_protocol_version = GRAPHENE_NET_MINIMUM_PROTOCOL_VERSION;
    connect_active( *old );
    old->send( fetch_items_message( block_message_type, { block_id( 2 ) } ) );
    BOOST_REQUIRE( old->wait_for( block_message_type, reply_timeout ) );
    const block_message delivered = old->received( block_message_type ).front().msg.as_block_message( *thread_pool );
    BOOST_CHECK( delivered.full_block->get_block_id() == block_id( 2 ) );
    BOOST_CHECK_EQUAL( old->count( compressed_block_message_type ), 0u );

    const item_hash_t unknown = fc::ripemd160::hash( std::string( "unknown block" ) );
    const item_not_available_message not_available =
      request< item_not_available_message >( *current, fetch_items_message( block_message_type, { unknown } ) );
    BOOST_CHECK( not_available.requested_item.item_hash == unknown );
    require_kept( *current );
    require_kept( *old );
  } );
}

/// C4: fetch_items for an item type other than block or transaction throws in the node, which ends the
/// connection without a closing message. Documents current behaviour (see the result of #1093).
BOOST_AUTO_TEST_CASE( fetch_items_unknown_type_test )
{
  start_node();
  run( [&]()
  {
    auto peer = make_peer( "odd peer" );
    connect_active( *peer );
    peer->send( fetch_items_message( current_time_request_message_type, { fc::ripemd160::hash( std::string( "anything" ) ) } ) );
    BOOST_REQUIRE( peer->wait_closed( reply_timeout ) );
    BOOST_CHECK( !peer->closing_message() );
  } );
}

/// C5: current_time_request is answered with the request time echoed and the node's times filled in
BOOST_AUTO_TEST_CASE( current_time_test )
{
  start_node();
  run( [&]()
  {
    auto peer = make_peer( "clock peer" );
    connect_active( *peer );
    const fc::time_point sent = fc::time_point::now();
    const current_time_reply_message reply = request< current_time_reply_message >( *peer, current_time_request_message( sent ) );
    const fc::time_point received = fc::time_point::now();
    BOOST_CHECK( reply.request_sent_time == sent );
    BOOST_CHECK( reply.request_received_time >= sent && reply.request_received_time <= received );
    BOOST_CHECK( reply.reply_transmitted_time >= reply.request_received_time && reply.reply_transmitted_time <= received );
  } );
}

/// C6: address_request is answered with the node's active connections it can name an endpoint for;
/// a peer that listens (non-zero inbound port at its address) is listed
BOOST_AUTO_TEST_CASE( address_request_test )
{
  start_node();
  run( [&]()
  {
    auto listed = make_peer( "listening peer" );
    const fc::ip::endpoint listening = listed->listen();
    p2p().add_node( listening );
    BOOST_REQUIRE( listed->wait_for( fetch_blockchain_item_ids_message_type, fc::seconds( 30 ) ) );

    auto asking = make_peer( "asking peer" );
    connect_active( *asking );
    const address_message reply = request< address_message >( *asking, address_request_message() );
    BOOST_CHECK( std::any_of( reply.addresses.begin(), reply.addresses.end(),
                              [&]( const address_info& a ) { return a.remote_endpoint == listening; } ) );
  } );
}

/// C6: with peer_advertising_disabled the node answers address_request with no addresses
BOOST_AUTO_TEST_CASE( address_request_advertising_disabled_test )
{
  start_node( R"("peer_advertising_disabled":true)" );
  run( [&]()
  {
    auto listed = make_peer( "listening peer" );
    p2p().add_node( listed->listen() );
    BOOST_REQUIRE( listed->wait_for( fetch_blockchain_item_ids_message_type, fc::seconds( 30 ) ) );

    auto asking = make_peer( "asking peer" );
    connect_active( *asking );
    const address_message reply = request< address_message >( *asking, address_request_message() );
    BOOST_CHECK( reply.addresses.empty() );
    BOOST_CHECK( listed->received_as< address_message >().front().addresses.empty() );
  } );
}

/// C7: get_current_connections_request is answered with the connections, the asking one included
BOOST_AUTO_TEST_CASE( current_connections_test )
{
  start_node();
  run( [&]()
  {
    auto other = make_peer( "other peer" );
    connect_active( *other );
    auto asking = make_peer( "asking peer" );
    connect_active( *asking );
    BOOST_REQUIRE( wait_until( [&]() { return number_of_connections() == 2; }, reply_timeout ) );
    const get_current_connections_reply_message reply =
      request< get_current_connections_reply_message >( *asking, get_current_connections_request_message() );
    BOOST_CHECK_EQUAL( reply.current_connections.size(), 2u );
    for( const test_peer* peer : { other.get(), asking.get() } )
      BOOST_CHECK( std::any_of( reply.current_connections.begin(), reply.current_connections.end(),
                                [&]( const current_connection_data& c ) { return c.node_id == peer->node_id(); } ) );
  } );
}

/// C8: asked to check whether another node is reachable, the node dials the endpoint: connection_successful
/// when a node with the given id answers there, unable_to_connect when nothing listens
BOOST_AUTO_TEST_CASE( check_firewall_test )
{
  start_node();
  run( [&]()
  {
    auto asking = make_peer( "asking peer" );
    connect_active( *asking );

    auto reachable = make_peer( "reachable peer" );
    const fc::ip::endpoint listening = reachable->listen();
    check_firewall_message check;
    check.node_id = reachable->node_id();
    check.endpoint_to_check = listening;
    check_firewall_reply_message reply = request< check_firewall_reply_message >( *asking, check );
    BOOST_CHECK( reply.endpoint_checked == listening );
    BOOST_CHECK_EQUAL( static_cast< int >( reply.result.value ), static_cast< int >( firewall_check_result::connection_successful ) );

    fc::ip::endpoint unused;
    {
      fc::tcp_server server;
      server.listen( fc::ip::endpoint( fc::ip::address( "127.0.0.1" ), 0 ) );
      unused = server.get_local_endpoint();
    }
    check.node_id = make_peer( "absent peer" )->node_id();
    check.endpoint_to_check = unused;
    reply = request< check_firewall_reply_message >( *asking, check );
    BOOST_CHECK_EQUAL( static_cast< int >( reply.result.value ), static_cast< int >( firewall_check_result::unable_to_connect ) );
    require_kept( *asking );
  } );
}

// ---------------------------------------------------------------------------------------------------------
// D. Node as a client in live mode
// ---------------------------------------------------------------------------------------------------------

/// D2: a node that is not at live sync doesn't fetch advertised transactions
BOOST_AUTO_TEST_CASE( transactions_ignored_out_of_live_sync_test )
{
  start_node();
  run( [&]()
  {
    auto peer = make_peer( "advertising peer" );
    connect_active( *peer );
    const item_hash_t hash = fc::ripemd160::hash( std::string( "some transaction" ) );
    peer->advertise( { hash } );
    fc::usleep( quiet_period );
    BOOST_CHECK_EQUAL( peer->count( fetch_items_message_type ), 0u );
    BOOST_CHECK( !peer->closed() );
  } );
}

/// D3: an inventory of an item type other than block or transaction is ignored, the connection kept
BOOST_AUTO_TEST_CASE( inventory_of_unknown_type_test )
{
  start_node();
  run( [&]()
  {
    auto peer = make_peer( "odd peer" );
    connect_active( *peer );
    peer->advertise( current_time_request_message_type, { fc::ripemd160::hash( std::string( "anything" ) ) } );
    require_kept( *peer );
    BOOST_CHECK_EQUAL( peer->count( fetch_items_message_type ), 0u );
  } );
}

/// D1, D4, D9: on a producing node a transaction advertised and delivered when asked for is included in a
/// block; a transaction nobody asked for ends the connection, and so does a second copy of a requested one;
/// item_not_available for an item the node never asked for is ignored
BOOST_AUTO_TEST_CASE( live_transaction_test )
{
  start_node( "", true );
  run( [&]()
  {
    fc::sleep_until( get_genesis_time() + HIVE_BLOCK_INTERVAL );
    wait_for_block_num( 1 );

    std::mutex included_mutex;
    std::set< transaction_id_type > included;
    auto block_conn = db->add_post_apply_block_handler( [&]( const block_notification& note )
    {
      std::lock_guard< std::mutex > guard( included_mutex );
      for( const full_transaction_ptr& transaction : note.full_block->get_full_transactions() )
        included.insert( transaction->get_transaction_id() );
    }, get_chain_plugin(), 0 );
    BOOST_SCOPE_EXIT( &block_conn ) { hive::utilities::disconnect_signal( block_conn ); } BOOST_SCOPE_EXIT_END

    auto peer = make_peer( "delivering peer" );
    connect_active( *peer );
    const full_transaction_ptr transaction = build_transfer( HIVE_INIT_MINER_NAME, HIVE_NULL_ACCOUNT, ASSET( "0.001 TESTS" ), "delivered",
                                                            get_default_expiration() );
    peer->provide( { transaction } );
    peer->advertise( { transaction->get_legacy_transaction_message_hash() } );
    BOOST_REQUIRE( wait_until( [&]()
    {
      std::lock_guard< std::mutex > guard( included_mutex );
      return included.count( transaction->get_transaction_id() ) != 0;
    }, fc::seconds( 30 ) ) );
    BOOST_CHECK( peer->was_requested( transaction->get_legacy_transaction_message_hash() ) );

    peer->send( item_not_available_message( item_id( trx_message_type, fc::ripemd160::hash( std::string( "never asked for" ) ) ) ) );
    require_kept( *peer );

    const full_transaction_ptr unrequested = build_transfer( HIVE_INIT_MINER_NAME, HIVE_NULL_ACCOUNT, ASSET( "0.001 TESTS" ), "unrequested",
                                                             get_default_expiration() );
    peer->send( trx_message( unrequested ) );
    require_closed_with( *peer, "You sent me a message that I didn't request" );

    auto repeating = make_peer( "repeating peer" );
    repeating->answer_twice( fetch_items_message_type );
    connect_active( *repeating );
    const full_transaction_ptr twice = build_transfer( HIVE_INIT_MINER_NAME, HIVE_NULL_ACCOUNT, ASSET( "0.001 TESTS" ), "delivered twice",
                                                       get_default_expiration() );
    repeating->provide( { twice } );
    repeating->advertise( { twice->get_legacy_transaction_message_hash() } );
    require_closed_with( *repeating, "You sent me a message that I didn't request" );
  } );
}

/// D6: a block nobody asked for ends the connection
BOOST_AUTO_TEST_CASE( unrequested_block_test )
{
  const std::vector< block_ptr > chain = generate_chain( 3 );
  start_node();
  run( [&]()
  {
    auto peer = make_peer( "pushy peer" );
    connect_active( *peer );
    peer->send( block_message( chain[ 0 ] ) );
    require_closed_with( *peer, "You sent me a block that I didn't ask for" );
  } );
}

/// D10 (T): a transaction request that is ignored ends the connection once active_ignored_request_timeout
/// (2 s here) passes; one answered well inside it doesn't
BOOST_AUTO_TEST_CASE( ignored_transaction_request_test )
{
  const fc::microseconds timeout = fc::seconds( 2 );
  start_node( R"("active_ignored_request_timeout_microseconds":)" + std::to_string( timeout.count() ), true );
  run( [&]()
  {
    fc::sleep_until( get_genesis_time() + HIVE_BLOCK_INTERVAL );
    wait_for_block_num( 1 );

    auto slow = make_peer( "slow but in time peer" );
    slow->answer_after( fetch_items_message_type, fc::microseconds( timeout.count() / 3 ) );
    connect_active( *slow );
    const full_transaction_ptr answered = build_transfer( HIVE_INIT_MINER_NAME, HIVE_NULL_ACCOUNT, ASSET( "0.001 TESTS" ), "answered",
                                                          get_default_expiration() );
    slow->provide( { answered } );
    slow->advertise( { answered->get_legacy_transaction_message_hash() } );

    auto silent = make_peer( "silent peer" );
    connect_active( *silent );
    const item_hash_t withheld = fc::ripemd160::hash( std::string( "withheld transaction" ) );
    silent->advertise( { withheld } );
    BOOST_REQUIRE( silent->wait_for( fetch_items_message_type, reply_timeout ) );
    const fc::time_point requested = silent->received( fetch_items_message_type ).front().time;

    BOOST_REQUIRE( silent->wait_closed( timeout + fc::seconds( 1 + 10 ) ) );
    BOOST_CHECK_GE( ( silent->closed_at() - requested ).count(), timeout.count() - fc::milliseconds( 100 ).count() );

    BOOST_REQUIRE( slow->wait_for( fetch_items_message_type, reply_timeout ) );
    fc::usleep( timeout + fc::seconds( 2 ) );
    BOOST_CHECK( !slow->closed() );
  } );
}

/// D16: the node advertises the blocks it produces to an in-sync peer by block id
BOOST_AUTO_TEST_CASE( block_advertisement_test )
{
  start_node( "", true );
  run( [&]()
  {
    fc::sleep_until( get_genesis_time() + HIVE_BLOCK_INTERVAL );
    wait_for_block_num( 1 );
    auto peer = make_peer( "listening peer" );
    connect_in_sync( *peer );
    const uint32_t next = get_block_num() + 1;
    wait_for_block_num( next );
    const block_id_type id = block_id( next );
    BOOST_REQUIRE( wait_until( [&]()
    {
      for( const item_ids_inventory_message& inventory : peer->received_as< item_ids_inventory_message >() )
        if( inventory.item_type == block_message_type &&
            std::find( inventory.item_hashes_available.begin(), inventory.item_hashes_available.end(), id ) != inventory.item_hashes_available.end() )
          return true;
      return false;
    }, reply_timeout ) );
  } );
}

/// C3 and relay (category 2): a transaction fetched from peer A is advertised to the in-sync peer B and not
/// back to A; B's fetch is answered from the message cache, an unknown one with item_not_available, and a
/// hash asked for twice in one request is answered once
BOOST_AUTO_TEST_CASE( transaction_relay_test )
{
  start_node( "", true );
  run( [&]()
  {
    fc::sleep_until( get_genesis_time() + HIVE_BLOCK_INTERVAL );
    wait_for_block_num( 1 );

    auto source = make_peer( "source" );
    connect_in_sync( *source );
    auto observer = make_peer( "observer" );
    connect_in_sync( *observer );

    const full_transaction_ptr transaction = build_transfer( HIVE_INIT_MINER_NAME, HIVE_NULL_ACCOUNT, ASSET( "0.001 TESTS" ), "relayed",
                                                            get_default_expiration() );
    const item_hash_t hash = transaction->get_legacy_transaction_message_hash();
    source->provide( { transaction } );
    source->advertise( { hash } );
    BOOST_REQUIRE( wait_until( [&]() { return observer->was_advertised( hash ); }, reply_timeout ) );
    BOOST_CHECK( !source->was_advertised( hash ) );

    const item_hash_t unknown = fc::ripemd160::hash( std::string( "unknown transaction" ) );
    observer->send( fetch_items_message( trx_message_type, { hash, unknown, hash } ) );
    BOOST_REQUIRE( observer->wait_for( item_not_available_message_type, reply_timeout ) );
    BOOST_REQUIRE( observer->wait_for( trx_message_type, reply_timeout ) );
    fc::usleep( quiet_period );
    BOOST_CHECK_EQUAL( observer->count( trx_message_type ), 1u );
    for( const received_message& r : observer->received( trx_message_type ) )
      BOOST_CHECK( r.msg.as_trx_message( *thread_pool ).full_transaction->get_transaction_id() == transaction->get_transaction_id() );
    BOOST_CHECK( observer->received_as< item_not_available_message >().front().requested_item.item_hash == unknown );
  } );
}

/// Fallback (category 2): a peer that answers item_not_available for an advertised transaction loses
/// nothing but the request; the node fetches the transaction from the other peer that advertised it
BOOST_AUTO_TEST_CASE( fallback_after_not_available_test )
{
  start_node( "", true );
  run( [&]()
  {
    fc::sleep_until( get_genesis_time() + HIVE_BLOCK_INTERVAL );
    wait_for_block_num( 1 );

    std::mutex included_mutex;
    std::set< transaction_id_type > included;
    auto block_conn = db->add_post_apply_block_handler( [&]( const block_notification& note )
    {
      std::lock_guard< std::mutex > guard( included_mutex );
      for( const full_transaction_ptr& transaction : note.full_block->get_full_transactions() )
        included.insert( transaction->get_transaction_id() );
    }, get_chain_plugin(), 0 );
    BOOST_SCOPE_EXIT( &block_conn ) { hive::utilities::disconnect_signal( block_conn ); } BOOST_SCOPE_EXIT_END

    const full_transaction_ptr transaction = build_transfer( HIVE_INIT_MINER_NAME, HIVE_NULL_ACCOUNT, ASSET( "0.001 TESTS" ), "fallback",
                                                            get_default_expiration() );
    const item_hash_t hash = transaction->get_legacy_transaction_message_hash();
    auto empty_handed = make_peer( "empty-handed peer" );
    empty_handed->stop_withholding();
    connect_active( *empty_handed );
    auto holder = make_peer( "holder" );
    holder->provide( { transaction } );
    connect_active( *holder );

    empty_handed->advertise( { hash } );
    BOOST_REQUIRE( wait_until( [&]() { return empty_handed->was_requested( hash ); }, reply_timeout ) );
    holder->advertise( { hash } );
    BOOST_REQUIRE( wait_until( [&]()
    {
      std::lock_guard< std::mutex > guard( included_mutex );
      return included.count( transaction->get_transaction_id() ) != 0;
    }, fc::seconds( 30 ) ) );
    BOOST_CHECK( holder->was_requested( hash ) );
    BOOST_CHECK( !empty_handed->closed() );
  } );
}

/// Connection limit (category 2): with maximum_number_of_connections connections, the next peer's hello is
/// rejected with not_accepting_connections
BOOST_AUTO_TEST_CASE( connection_limit_test )
{
  start_node( R"("maximum_number_of_connections":2,"desired_number_of_connections":1)" );
  run( [&]()
  {
    auto first = make_peer( "first" );
    connect_active( *first );
    auto second = make_peer( "second" );
    connect_active( *second );
    auto third = make_peer( "third" );
    third->connect( node_endpoint() );
    require_rejected( *third, rejection_reason_code::not_accepting_connections );
    require_kept( *first );
    require_kept( *second );
  } );
}

// ---------------------------------------------------------------------------------------------------------
// E. Node as a client during sync
// ---------------------------------------------------------------------------------------------------------

/// E1, E10: a peer that is ahead is synced from: ids requested, blocks fetched in batches no larger than
/// maximum_blocks_per_peer_during_syncing and applied in order, until the node is in sync; the peer delivers
/// each batch in reverse order
BOOST_AUTO_TEST_CASE( sync_test )
{
  const uint32_t count = 30;
  const uint32_t batch = 7;
  const std::vector< block_ptr > chain = generate_chain( count );
  start_node( R"("maximum_blocks_per_peer_during_syncing":)" + std::to_string( batch ) );
  run( [&]()
  {
    std::mutex applied_mutex;
    std::vector< uint32_t > applied;
    auto block_conn = db->add_post_apply_block_handler( [&]( const block_notification& note )
    {
      std::lock_guard< std::mutex > guard( applied_mutex );
      applied.push_back( note.block_num );
    }, get_chain_plugin(), 0 );
    BOOST_SCOPE_EXIT( &block_conn ) { hive::utilities::disconnect_signal( block_conn ); } BOOST_SCOPE_EXIT_END

    auto peer = make_peer( "source peer" );
    peer->serve_blocks( chain );
    peer->on( fetch_items_message_type, []( test_peer& self, const message& m )
    {
      fetch_items_message request = m.as< fetch_items_message >();
      std::reverse( request.items_to_fetch.begin(), request.items_to_fetch.end() );
      self.answer( message( request ) );
    } );
    peer->connect( node_endpoint() );
    BOOST_REQUIRE_MESSAGE( wait_until( [&]() { return get_block_num() == count; }, fc::seconds( 60 ) ), "head is " << get_block_num() );
    BOOST_CHECK( head_block_id() == chain.back()->get_block_id() );
    {
      std::lock_guard< std::mutex > guard( applied_mutex );
      BOOST_REQUIRE_EQUAL( applied.size(), count );
      for( uint32_t i = 0; i < count; ++i )
        BOOST_CHECK_EQUAL( applied[ i ], i + 1 );
    }
    for( const fetch_items_message& request : peer->received_as< fetch_items_message >() )
    {
      BOOST_CHECK_EQUAL( request.item_type, static_cast< uint32_t >( block_message_type ) );
      BOOST_CHECK_LE( request.items_to_fetch.size(), batch );
    }
    // once it has everything, the node asks for more ids and is told there are none
    BOOST_CHECK( wait_until( [&]() { return peer->in_sync(); }, reply_timeout ) );
    require_kept( *peer );
  } );
}

/// E2, E3, E6: invalid replies to the node's request for block ids end the connection: a list that is not
/// sequential, one that doesn't start at block 1 for an empty synopsis, and one implying more blocks than
/// could exist by now
BOOST_AUTO_TEST_CASE( invalid_sync_ids_test )
{
  const std::vector< block_ptr > chain = generate_chain( 7 );
  start_node();
  run( [&]()
  {
    const auto peer_answering = [&]( const std::string& name, uint32_t remaining, const std::vector< item_hash_t >& ids )
    {
      auto peer = make_peer( name );
      peer->serve_blocks( chain );
      peer->on( fetch_blockchain_item_ids_message_type, [remaining, ids]( test_peer& self, const message& )
      {
        self.send( blockchain_item_ids_inventory_message( remaining, block_message_type, ids ) );
      } );
      peer->connect( node_endpoint() );
      return peer;
    };
    const auto id = [&]( uint32_t num ) { return item_hash_t( chain[ num - 1 ]->get_block_id() ); };

    auto unordered = peer_answering( "unordered ids", 0, { id( 1 ), id( 3 ), id( 2 ) } );
    require_closed_with( *unordered, "invalid response" );

    auto late_start = peer_answering( "ids not from block 1", 0, { id( 2 ), id( 3 ) } );
    require_closed_with( *late_start, "invalid response" );
    BOOST_CHECK_EQUAL( get_block_num(), 0u );

    // the plausibility check counts from the newest block both have, so give the node some first
    auto source = make_peer( "source of the first blocks" );
    source->serve_blocks( std::vector< block_ptr >( chain.begin(), chain.begin() + 5 ) );
    source->connect( node_endpoint() );
    BOOST_REQUIRE( wait_until( [&]() { return get_block_num() == 5; }, fc::seconds( 30 ) ) );

    // a million more blocks would reach about a month past now
    auto too_many = peer_answering( "implausibly many ids", 1000000, { id( 5 ), id( 6 ), id( 7 ) } );
    require_closed_with( *too_many, "more sync blocks than could possibly exist" );
  } );
}

/// E6 at genesis: a node without blocks measures the plausible number of sync blocks from the genesis time,
/// so a count reaching far past now ends the connection
BOOST_AUTO_TEST_CASE( implausible_sync_count_at_genesis_test )
{
  const std::vector< block_ptr > chain = generate_chain( 2 );
  start_node();
  run( [&]()
  {
    auto peer = make_peer( "boasting peer" );
    peer->serve_blocks( chain );
    peer->on( fetch_blockchain_item_ids_message_type, [&]( test_peer& self, const message& m )
    {
      // genesis is 20 minutes ago, so a million more blocks would reach about a month past now
      self.send( blockchain_item_ids_inventory_message( 1000000, block_message_type,
                                                        { chain[ 0 ]->get_block_id(), chain[ 1 ]->get_block_id() } ) );
    } );
    peer->connect( node_endpoint() );
    require_closed_with( *peer, "more sync blocks than could possibly exist" );
    BOOST_CHECK_EQUAL( peer->count( fetch_blockchain_item_ids_message_type ), 1u );
    BOOST_CHECK_EQUAL( get_block_num(), 0u );
  } );
}

/// E4: a reply that doesn't start at one of the synopsis ids ends the connection
BOOST_AUTO_TEST_CASE( sync_ids_off_synopsis_test )
{
  const std::vector< block_ptr > chain = generate_chain( 10 );
  start_node();
  run( [&]()
  {
    // the node first syncs the first five blocks, so its later synopsis is not empty
    auto first = make_peer( "first source" );
    first->serve_blocks( std::vector< block_ptr >( chain.begin(), chain.begin() + 5 ) );
    first->connect( node_endpoint() );
    BOOST_REQUIRE( wait_until( [&]() { return get_block_num() == 5; }, fc::seconds( 30 ) ) );

    auto peer = make_peer( "off synopsis" );
    peer->serve_blocks( chain );
    peer->on( fetch_blockchain_item_ids_message_type, [&]( test_peer& self, const message& )
    {
      self.send( blockchain_item_ids_inventory_message( 0, block_message_type, { chain[ 7 ]->get_block_id(), chain[ 8 ]->get_block_id() } ) );
    } );
    peer->connect( node_endpoint() );
    require_closed_with( *peer, "invalid response" );
  } );
}

/// E5: block ids the node didn't ask for are ignored
BOOST_AUTO_TEST_CASE( unrequested_sync_ids_test )
{
  const std::vector< block_ptr > chain = generate_chain( 5 );
  start_node();
  run( [&]()
  {
    auto peer = make_peer( "chatty peer" );
    connect_active( *peer );
    // the node's own request has been answered (we have nothing it lacks); this one it didn't ask for
    peer->send( blockchain_item_ids_inventory_message( 0, block_message_type, { chain[ 0 ]->get_block_id(), chain[ 1 ]->get_block_id() } ) );
    require_kept( *peer );
    BOOST_CHECK_EQUAL( peer->count( fetch_items_message_type ), 0u );
    BOOST_CHECK_EQUAL( get_block_num(), 0u );
  } );
}

/// E7 (T): a request for block ids that is ignored ends the connection once active_ignored_request_timeout
/// (2 s here) passes
BOOST_AUTO_TEST_CASE( ignored_sync_ids_request_test )
{
  const fc::microseconds timeout = fc::seconds( 2 );
  const std::vector< block_ptr > chain = generate_chain( 5 );
  start_node( R"("active_ignored_request_timeout_microseconds":)" + std::to_string( timeout.count() ) );
  run( [&]()
  {
    auto peer = make_peer( "silent source" );
    peer->serve_blocks( chain );
    peer->ignore( fetch_blockchain_item_ids_message_type );
    peer->connect( node_endpoint() );
    BOOST_REQUIRE( peer->wait_for( fetch_blockchain_item_ids_message_type, reply_timeout ) );
    const fc::time_point requested = peer->received( fetch_blockchain_item_ids_message_type ).front().time;
    BOOST_REQUIRE( peer->wait_closed( timeout + fc::seconds( 1 + 10 ) ) );
    BOOST_CHECK_GE( ( peer->closed_at() - requested ).count(), timeout.count() - fc::milliseconds( 100 ).count() );
  } );
}

/// E8 (T): sync blocks that are not delivered end the connection once active_ignored_request_timeout (2 s here)
/// passes; blocks delivered slowly, well inside it, don't
BOOST_AUTO_TEST_CASE( undelivered_sync_blocks_test )
{
  const fc::microseconds timeout = fc::seconds( 2 );
  const std::vector< block_ptr > chain = generate_chain( 6 );
  start_node( R"("active_ignored_request_timeout_microseconds":)" + std::to_string( timeout.count() ) +
              R"(,"maximum_blocks_per_peer_during_syncing":2)" );
  run( [&]()
  {
    auto withholding = make_peer( "withholding source" );
    withholding->serve_blocks( chain );
    withholding->ignore( fetch_items_message_type );
    withholding->connect( node_endpoint() );
    BOOST_REQUIRE( withholding->wait_for( fetch_items_message_type, reply_timeout ) );
    const fc::time_point requested = withholding->received( fetch_items_message_type ).front().time;
    BOOST_REQUIRE( withholding->wait_closed( timeout + fc::seconds( 1 + 10 ) ) );
    BOOST_CHECK_GE( ( withholding->closed_at() - requested ).count(), timeout.count() - fc::milliseconds( 100 ).count() );

    auto slow = make_peer( "slow source" );
    slow->serve_blocks( chain );
    slow->answer_after( fetch_items_message_type, fc::microseconds( timeout.count() / 3 ) );
    slow->connect( node_endpoint() );
    BOOST_REQUIRE_MESSAGE( wait_until( [&]() { return get_block_num() == chain.size(); }, fc::seconds( 60 ) ), "head is " << get_block_num() );
    BOOST_CHECK( !slow->closed() );
  } );
}

/// E9: item_not_available for the sync blocks a peer offered ends the connection when the peer needs nothing
/// from the node; while the node thinks the peer needs its blocks, it only stops fetching from it and keeps it
BOOST_AUTO_TEST_CASE( missing_sync_item_test )
{
  const std::vector< block_ptr > chain = generate_chain( 5 );
  start_node();
  run( [&]()
  {
    const auto forgetful_peer = [&]( const std::string& name, bool tells_head )
    {
      auto peer = make_peer( name );
      peer->serve_blocks( chain );
      peer->on( fetch_items_message_type, []( test_peer& self, const message& m )
      {
        const fetch_items_message request = m.as< fetch_items_message >();
        for( const item_hash_t& hash : request.items_to_fetch )
          self.send( item_not_available_message( item_id( request.item_type, hash ) ) );
      } );
      if( tells_head )
        peer->on( fetch_blockchain_item_ids_message_type, [&]( test_peer& self, const message& m )
        {
          // asking for blocks after our head first tells the node we need none of its blocks
          self.send( fetch_blockchain_item_ids_message( block_message_type, { chain.back()->get_block_id() } ) );
          self.answer( m );
        } );
      peer->connect( node_endpoint() );
      return peer;
    };

    auto in_need = forgetful_peer( "forgetful peer the node may serve", false );
    BOOST_REQUIRE( in_need->wait_for( fetch_items_message_type, reply_timeout ) );
    require_kept( *in_need );
    const size_t requests = in_need->count( fetch_items_message_type );
    fc::usleep( quiet_period );
    BOOST_CHECK_EQUAL( in_need->count( fetch_items_message_type ), requests );
    in_need->close();

    auto satisfied = forgetful_peer( "forgetful peer in need of nothing", true );
    require_closed_with( *satisfied, "missing a sync item you claim to have" );
    BOOST_CHECK_EQUAL( get_block_num(), 0u );
  } );
}

/// E11: a sync block with an invalid signature ends the connection and is not applied
BOOST_AUTO_TEST_CASE( invalid_sync_block_test )
{
  const std::vector< block_ptr > chain = generate_chain( 5 );
  start_node();
  run( [&]()
  {
    std::vector< block_ptr > bad_chain = chain;
    bad_chain[ 2 ] = with_bad_signature( chain[ 2 ] );
    auto peer = make_peer( "forging source" );
    peer->serve_blocks( bad_chain );
    peer->connect( node_endpoint() );
    BOOST_REQUIRE( peer->wait_closed( fc::seconds( 30 ) ) );
    BOOST_CHECK( peer->closing_message() );
    BOOST_CHECK_LT( get_block_num(), 3u );

    // a good source completes the sync
    auto good = make_peer( "good source" );
    good->serve_blocks( chain );
    good->connect( node_endpoint() );
    BOOST_REQUIRE_MESSAGE( wait_until( [&]() { return get_block_num() == chain.size(); }, fc::seconds( 60 ) ), "head is " << get_block_num() );
  } );
}

/// E13: a peer that disconnects in the middle of sync doesn't stall the node: it syncs the rest when a
/// source comes back
BOOST_AUTO_TEST_CASE( sync_source_leaves_test )
{
  const uint32_t count = 20;
  const std::vector< block_ptr > chain = generate_chain( count );
  start_node( R"("maximum_blocks_per_peer_during_syncing":5)" );
  run( [&]()
  {
    auto leaving = make_peer( "leaving source" );
    leaving->serve_blocks( chain );
    leaving->on( fetch_items_message_type, []( test_peer& self, const message& m )
    {
      // delivers the first batch and leaves
      if( self.count( fetch_items_message_type ) != 1 )
        return;
      self.answer( m );
      self.later( fc::milliseconds( 50 ), [&self]() { self.close(); } );
    } );
    leaving->connect( node_endpoint() );
    BOOST_REQUIRE( leaving->wait_closed( reply_timeout ) );
    const uint32_t reached = get_block_num();
    BOOST_CHECK_LT( reached, count );

    auto returning = make_peer( "returning source" );
    returning->serve_blocks( chain );
    returning->connect( node_endpoint() );
    BOOST_REQUIRE_MESSAGE( wait_until( [&]() { return get_block_num() == count; }, fc::seconds( 60 ) ), "head is " << get_block_num() );
  } );
}

// ---------------------------------------------------------------------------------------------------------
// F. Connection lifetime
// ---------------------------------------------------------------------------------------------------------

/// F1, F2 (T): a silent active peer gets a keepalive current_time_request after half of the inactivity
/// timeout (10 block intervals, 30 s); one that answers is kept, one that doesn't is disconnected for
/// inactivity at the full timeout and, as it keeps its socket open, forcibly closed GRAPHENE_NET_PEER_DISCONNECT_TIMEOUT
/// (20 s) later. Both intervals are hard-coded, so the case takes about a minute.
BOOST_AUTO_TEST_CASE( inactivity_test )
{
  start_node();
  run( [&]()
  {
    const fc::microseconds inactivity_timeout = fc::seconds( 10 * HIVE_BLOCK_INTERVAL );
    const fc::microseconds keepalive_after = fc::seconds( 10 * HIVE_BLOCK_INTERVAL / 2 );
    // the node checks once a second and its timestamps are in whole seconds; the late slack is for a loaded host
    const fc::microseconds early = fc::seconds( 1 );
    const fc::microseconds late = fc::seconds( 1 + 10 );

    auto answering = make_peer( "answering peer" );
    connect_active( *answering );
    auto silent = make_peer( "silent peer" );
    silent->ignore( current_time_request_message_type );
    silent->keep_open_when_asked_to_close();
    connect_active( *silent );
    // when the node last heard from the silent peer
    const fc::time_point last_heard = fc::time_point::now();

    // the node asks for the time right after the handshake too; a keepalive comes later
    const auto first_keepalive = [&]()
    {
      fc::optional< fc::time_point > first;
      for( const received_message& r : silent->received( current_time_request_message_type ) )
        if( r.time > last_heard + early )
          return fc::optional< fc::time_point >( r.time );
      return first;
    };
    BOOST_REQUIRE( wait_until( [&]() { return bool( first_keepalive() ); }, keepalive_after + late ) );
    BOOST_CHECK_GE( ( *first_keepalive() - last_heard ).count(), ( keepalive_after - early ).count() );

    BOOST_REQUIRE( wait_until( [&]() { return bool( silent->closing_message() ); }, inactivity_timeout - keepalive_after + late ) );
    const fc::time_point asked_to_close = silent->received( closing_connection_message_type ).front().time;
    BOOST_CHECK_GE( ( asked_to_close - last_heard ).count(), ( inactivity_timeout - early ).count() );
    BOOST_CHECK( silent->closing_message()->reason_for_closing.find( "inactivity" ) != std::string::npos );
    BOOST_CHECK( !silent->closed() );

    BOOST_REQUIRE( silent->wait_closed( fc::seconds( GRAPHENE_NET_PEER_DISCONNECT_TIMEOUT ) + late ) );
    BOOST_CHECK_GE( ( silent->closed_at() - asked_to_close ).count(), ( fc::seconds( GRAPHENE_NET_PEER_DISCONNECT_TIMEOUT ) - early ).count() );
    BOOST_CHECK( !silent->closed_by_us() );

    BOOST_CHECK( answering->count( current_time_request_message_type ) >= 1 );
    BOOST_CHECK( !answering->closed() );
    BOOST_CHECK( node_lists( *answering ) );
  } );
}

BOOST_AUTO_TEST_SUITE_END()

#endif
