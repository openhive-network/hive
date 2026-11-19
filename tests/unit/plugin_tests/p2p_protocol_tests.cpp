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

#include <fc/bitutil.hpp>

#include <algorithm>
#include <atomic>
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
 * producing node (the witness_fixture default, genesis 3 s ahead and a block every 3 s). Blocks an idle node
 * should get in live mode come from a peer that delivers them when asked but doesn't offer them for sync
 * (test_peer::provide_blocks()). A pruned node is an idle node without a block log (block-log-split 0), which
 * keeps only its last irreversible block.
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
   * so the node under test can sync them from a test peer. With @p fork_at, the slot before block
   * fork_at is missed, so that block and every later one differ from those of a chain without the gap.
   */
  std::vector< block_ptr > generate_chain( uint32_t count, uint32_t fork_at = 0 )
  {
    choose_past_genesis();
    std::vector< block_ptr > blocks;
    {
      witness_fixture source;
      source.set_data_dir( ( hive::utilities::temp_directory_path() / "p2p-protocol-chain-source" ).string() );
      source.set_genesis_time( get_genesis_time() );
      source.initialize( 0, {}, {} );
      if( fork_at == 0 )
        source.schedule_blocks( count );
      else
      {
        source.schedule_blocks( fork_at - 1 );
        source.db_plugin->debug_generate_blocks( source.init_account_priv_key, 1, source.default_skip, 1, false );
        source.schedule_blocks( count - fork_at );
      }
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
   * separated) are added to the p2p-parameters, @p extra_config to the other options. Unless
   * @p produce_blocks, the node has no witness and its genesis is in the past.
   */
  void start_node( const std::string& extra_p2p_parameters = "", bool produce_blocks = false,
                   const config_arg_override_t& extra_config = {} )
  {
    if( _source_logging_config )
      set_logging_config( _source_logging_config );
    std::string p2p_parameters = R"({"listen_endpoint":"127.0.0.1:0","accept_incoming_connections":true)";
    if( !extra_p2p_parameters.empty() )
      p2p_parameters += "," + extra_p2p_parameters;
    p2p_parameters += "}";
    config_arg_override_t config = extra_config;
    config.emplace_back( config_line_t( { "p2p-parameters", { p2p_parameters } } ) );
    if( produce_blocks )
      initialize( 3, {}, { "initminer" }, config );
    else
    {
      choose_past_genesis();
      initialize( 0, {}, {}, config );
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

  /// Syncs the node under test to block @p count of @p chain from a short-lived source peer
  void sync_node( const std::vector< block_ptr >& chain, uint32_t count )
  {
    auto source = make_peer( "initial source" );
    source->serve_blocks( std::vector< block_ptr >( chain.begin(), chain.begin() + count ) );
    source->connect( node_endpoint() );
    BOOST_REQUIRE_MESSAGE( wait_until( [&]() { return get_block_num() == count; }, fc::seconds( 30 ) ), "head is " << get_block_num() );
    BOOST_REQUIRE( wait_until( [&]() { return source->in_sync(); }, reply_timeout ) );
  }

  /// Connects a peer that delivers the blocks of @p chain it is asked for but offers none for sync (see provide_blocks())
  std::unique_ptr< test_peer > connect_live_peer( const std::string& name, const std::vector< block_ptr >& chain )
  {
    auto peer = make_peer( name );
    peer->provide_blocks( chain );
    connect_active( *peer );
    return peer;
  }

  bool head_reaches( uint32_t num, fc::microseconds timeout = reply_timeout )
  {
    return wait_until( [&]() { return get_block_num() >= num; }, timeout );
  }

  uint32_t last_irreversible_block_num()
  {
    uint32_t num = 0;
    db->with_read_lock( [&]() { num = db->get_last_irreversible_block_num(); } );
    return num;
  }

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

  /// How many times the node asked @p peer for the item @p hash so far
  static size_t times_requested( const test_peer& peer, const item_hash_t& hash )
  {
    size_t times = 0;
    for( const fetch_items_message& request : peer.received_as< fetch_items_message >() )
      times += std::count( request.items_to_fetch.begin(), request.items_to_fetch.end(), hash );
    return times;
  }

  /// When the node first asked @p peer for the item @p hash; requires that it did
  static fc::time_point first_request_time( const test_peer& peer, const item_hash_t& hash )
  {
    for( const received_message& r : peer.received( fetch_items_message_type ) )
    {
      const fetch_items_message request = r.msg.as< fetch_items_message >();
      if( std::find( request.items_to_fetch.begin(), request.items_to_fetch.end(), hash ) != request.items_to_fetch.end() )
        return r.time;
    }
    BOOST_FAIL( "the node did not ask for the item" );
    return fc::time_point();
  }

  /// Whether the node advertised to @p peer the block item @p hash (a block id or a legacy block message hash)
  static bool block_advertised( const test_peer& peer, const item_hash_t& hash )
  {
    for( const item_ids_inventory_message& inventory : peer.received_as< item_ids_inventory_message >() )
      if( inventory.item_type == block_message_type &&
          std::find( inventory.item_hashes_available.begin(), inventory.item_hashes_available.end(), hash ) != inventory.item_hashes_available.end() )
        return true;
    return false;
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

/// A copy of @p block with the same header, and so the same id, whose body doesn't match the header's transaction_merkle_root
block_ptr with_corrupted_body( const block_ptr& block )
{
  hive::chain::signed_block copy = block->get_block();
  copy.transactions.emplace_back();
  copy.transactions.back().ref_block_num = 1;
  const block_ptr corrupted = hive::chain::full_block_type::create_from_signed_block( copy );
  BOOST_REQUIRE( corrupted->get_block_id() == block->get_block_id() );
  return corrupted;
}

/// An id of block number @p num that names no real block (the number is the first 4 bytes of an id)
block_id_type fake_block_id( uint32_t num, const std::string& salt )
{
  block_id_type id = fc::ripemd160::hash( salt );
  id._hash[ 0 ] = fc::endian_reverse_u32( num );
  return id;
}

/// Whether a compressed_block_message names a compression dictionary (see core_messages.cpp)
bool compressed_with_dictionary( const message& m )
{
  BOOST_REQUIRE_EQUAL( m.msg_type, static_cast< uint32_t >( compressed_block_message_type ) );
  BOOST_REQUIRE_GT( m.data.size(), 2u );
  return ( m.data[ 0 ] & 0x40 ) != 0;
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

/// A9, C1 (pruned): a node that keeps no block log (block-log-split 0) has pruned every block below its last
/// irreversible block, its tail. A hello from a peer more than GRAPHENE_NET_PRUNED_TAIL_GRACE_BLOCKS below the tail
/// is rejected ("I can't help you sync"), one within the grace is accepted (#967). An accepted peer that then asks
/// for block ids after a block that far below the tail, or from the start of the chain, is disconnected for it.
BOOST_AUTO_TEST_CASE( pruned_node_test )
{
  start_node( "", false, { config_line_t( { "block-log-split", { "0" } } ) } );
  run( [&]()
  {
    schedule_blocks( 60 );
    const uint32_t tail = last_irreversible_block_num();
    BOOST_REQUIRE_GT( tail, GRAPHENE_NET_PRUNED_TAIL_GRACE_BLOCKS + 1 );
    const std::string reason = "I can't help you sync";

    auto too_far = make_peer( "peer below the tail" );
    too_far->hello().last_known_block_number = tail - GRAPHENE_NET_PRUNED_TAIL_GRACE_BLOCKS - 1;
    too_far->connect( node_endpoint() );
    require_rejected( *too_far, rejection_reason_code::unspecified );
    BOOST_CHECK( too_far->rejection()->reason_string.find( reason ) != std::string::npos );

    auto within = make_peer( "peer within the grace" );
    within->hello().last_known_block_number = tail - GRAPHENE_NET_PRUNED_TAIL_GRACE_BLOCKS;
    connect_active( *within );
    // the hello doesn't have to name a block at all
    auto unknown = make_peer( "peer not saying where it is" );
    connect_active( *unknown );
    require_kept( *within );
    require_kept( *unknown );

    within->send( fetch_blockchain_item_ids_message( block_message_type,
      { fake_block_id( tail - GRAPHENE_NET_PRUNED_TAIL_GRACE_BLOCKS - 1, "old block" ) } ) );
    require_closed_with( *within, reason );
    unknown->send( fetch_blockchain_item_ids_message( block_message_type, {} ) );
    require_closed_with( *unknown, reason );
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

/// B6: a peer that stops reading while the node has replies for it is disconnected once more than
/// GRAPHENE_NET_MAXIMUM_QUEUED_MESSAGES_IN_BYTES wait to be sent, and other peers are unaffected. The peer stops
/// reading by not returning from its handler of the first reply, and keeps asking for the ids of all 300 blocks
/// (6 kB a reply) until the socket buffers between them are full and the node's queue overflows.
BOOST_AUTO_TEST_CASE( send_queue_limit_test )
{
  start_node();
  run( [&]()
  {
    schedule_blocks( 300 );
    auto bystander = make_peer( "bystander" );
    connect_active( *bystander );
    auto stalled = make_peer( "peer that stops reading" );
    connect_active( *stalled );
    BOOST_REQUIRE( wait_until( [&]() { return node_lists( *stalled ); }, reply_timeout ) );

    std::atomic< bool > resume = false;
    BOOST_SCOPE_EXIT( &resume ) { resume = true; } BOOST_SCOPE_EXIT_END
    stalled->on( blockchain_item_ids_inventory_message_type, [&resume]( test_peer&, const message& )
    {
      while( !resume )
        fc::usleep( fc::milliseconds( 20 ) );
    } );
    // p2p_plugin::get_connected_peers() throws (bad file descriptor) while a connection whose socket the node closed
    // is still among its active connections, which it is for a moment after the queue overflows; that is no answer
    bool listing_failed = false;
    const auto still_listed = [&]()
    {
      try
      {
        return node_lists( *stalled );
      }
      catch( const fc::exception& e )
      {
        BOOST_TEST_MESSAGE( "listing the connections failed: " << e.to_string() );
        listing_failed = true;
        return true;
      }
    };
    const fetch_blockchain_item_ids_message request( block_message_type, {} );
    bool dropped = false;
    for( int sent = 0; sent < 20000 && !dropped; sent += 500 )
    {
      for( int i = 0; i < 500; ++i )
        stalled->send( request );
      dropped = wait_until( [&]() { return !still_listed(); }, fc::milliseconds( 200 ) );
    }
    BOOST_REQUIRE_MESSAGE( dropped, "the node kept a peer that doesn't read" );
    BOOST_TEST_MESSAGE( "listing the connections " << ( listing_failed ? "failed" : "never failed" ) << " meanwhile" );
    resume = true;
    BOOST_CHECK( stalled->wait_closed( reply_timeout ) );
    require_kept( *bystander );
    BOOST_CHECK( node_lists( *bystander ) );
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

/// C2, D16, category 2 (protocol versions): each peer gets blocks in its own form. The node advertises a block it
/// produced by block id to current peers and by legacy block message hash to a protocol 106 peer, and serves the
/// block by that name: uncompressed to the protocol 106 peer, compressed to the current ones, using a dictionary
/// only for the peer that reported having one.
BOOST_AUTO_TEST_CASE( block_forms_by_protocol_test )
{
  start_node( "", true );
  run( [&]()
  {
    fc::sleep_until( get_genesis_time() + HIVE_BLOCK_INTERVAL );
    wait_for_block_num( 1 );

    auto plain = make_peer( "current peer without dictionaries" );
    connect_in_sync( *plain );
    auto with_dictionary = make_peer( "current peer with dictionaries" );
    const std::optional< uint8_t > dictionary = hive::chain::get_last_available_zstd_compression_dictionary_number();
    BOOST_REQUIRE( dictionary );
    with_dictionary->hello().last_available_zstd_compression_dictionary_number = *dictionary;
    connect_in_sync( *with_dictionary );
    auto old = make_peer( "protocol 106 peer" );
    old->hello().core_protocol_version = GRAPHENE_NET_MINIMUM_PROTOCOL_VERSION;
    connect_in_sync( *old );

    const uint32_t next = get_block_num() + 1;
    wait_for_block_num( next );
    const block_ptr produced = get_block_reader().get_block_by_number( next );
    const block_id_type id = produced->get_block_id();
    const item_hash_t legacy_hash = produced->get_legacy_block_message_hash();
    BOOST_REQUIRE( wait_until( [&]()
    {
      return block_advertised( *plain, id ) && block_advertised( *with_dictionary, id ) && block_advertised( *old, legacy_hash );
    }, reply_timeout ) );
    BOOST_CHECK( !block_advertised( *old, id ) );
    BOOST_CHECK( !block_advertised( *plain, legacy_hash ) );

    old->request_blocks( { legacy_hash } );
    BOOST_REQUIRE( old->wait_for( block_message_type, reply_timeout ) );
    BOOST_CHECK( old->blocks_received() == std::vector< block_id_type >{ id } );
    BOOST_CHECK_EQUAL( old->count( compressed_block_message_type ), 0u );

    for( test_peer* peer : { plain.get(), with_dictionary.get() } )
    {
      peer->request_blocks( { id } );
      BOOST_REQUIRE( peer->wait_for( compressed_block_message_type, reply_timeout ) );
      BOOST_CHECK( peer->blocks_received() == std::vector< block_id_type >{ id } );
      BOOST_CHECK_EQUAL( peer->count( block_message_type ), 0u );
    }
    BOOST_CHECK( !compressed_with_dictionary( plain->received( compressed_block_message_type ).front().msg ) );
    const message with = with_dictionary->received( compressed_block_message_type ).front().msg;
    BOOST_REQUIRE( compressed_with_dictionary( with ) );
    BOOST_CHECK_LE( static_cast< uint8_t >( with.data[ 1 ] ), *dictionary );
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

/// D5: requested transactions that the chain rejects - signed with the wrong key, expired, or already in a block -
/// cost the peer nothing: the connection is kept, none of them is included or relayed, and none is fetched again
/// when advertised again, as the node remembers recently failed items. A valid transaction delivered with them is
/// included and relayed.
BOOST_AUTO_TEST_CASE( invalid_transactions_test )
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
    const auto is_included = [&]( const full_transaction_ptr& transaction )
    {
      std::lock_guard< std::mutex > guard( included_mutex );
      return included.count( transaction->get_transaction_id() ) != 0;
    };
    const auto transfer = [&]( const std::string& memo, int64_t expires_in )
    {
      signed_transaction transaction;
      transfer_operation op;
      op.from = HIVE_INIT_MINER_NAME;
      op.to = HIVE_NULL_ACCOUNT;
      op.amount = ASSET( "0.001 TESTS" );
      op.memo = memo;
      transaction.operations.emplace_back( op );
      db->with_read_lock( [&]()
      {
        transaction.set_reference_block( db->head_block_id() );
        transaction.set_expiration( fc::time_point_sec( db->head_block_time().sec_since_epoch() + expires_in ) );
      } );
      return transaction;
    };

    const int64_t valid_for = get_default_expiration();
    const full_transaction_ptr expired_transaction = build_transaction( transfer( "expired", -HIVE_BLOCK_INTERVAL ) );
    const full_transaction_ptr wrongly_signed = full_transaction_type::create_from_signed_transaction( transfer( "wrongly signed", valid_for ),
                                                                                                       serialization_type::hf26, false );
    wrongly_signed->sign_transaction( { fc::ecc::private_key::generate() }, db->get_chain_id(), serialization_type::hf26 );
    const full_transaction_ptr duplicate = build_transaction( transfer( "already included", valid_for ) );
    // pushed to the chain directly, so the node's p2p message cache doesn't know it
    schedule_transaction( duplicate );
    BOOST_REQUIRE( wait_until( [&]() { return is_included( duplicate ); }, fc::seconds( 30 ) ) );
    const full_transaction_ptr valid = build_transaction( transfer( "valid", valid_for ) );
    const std::vector< full_transaction_ptr > invalid = { wrongly_signed, expired_transaction, duplicate };

    auto observer = make_peer( "observer" );
    connect_in_sync( *observer );
    auto peer = make_peer( "delivering peer" );
    connect_active( *peer );
    std::vector< item_hash_t > hashes;
    for( const full_transaction_ptr& transaction : invalid )
      hashes.push_back( transaction->get_legacy_transaction_message_hash() );
    hashes.push_back( valid->get_legacy_transaction_message_hash() );
    peer->provide( invalid );
    peer->provide( { valid } );
    peer->advertise( hashes );
    BOOST_REQUIRE( wait_until( [&]() { return is_included( valid ); }, fc::seconds( 30 ) ) );
    BOOST_CHECK( wait_until( [&]() { return observer->was_advertised( hashes.back() ); }, reply_timeout ) );
    // the invalid ones were delivered with the valid one, so the node is done with them too; one more block
    wait_for_block_num( get_block_num() + 1 );

    hashes.pop_back();
    peer->advertise( hashes );
    require_kept( *peer );
    for( size_t i = 0; i < invalid.size(); ++i )
      BOOST_TEST_CONTEXT( "transaction " << i )
      {
        BOOST_CHECK_EQUAL( times_requested( *peer, hashes[ i ] ), 1u );
        BOOST_CHECK( !observer->was_advertised( hashes[ i ] ) );
        if( invalid[ i ] != duplicate )
          BOOST_CHECK( !is_included( invalid[ i ] ) );
      }
  } );
}

/// D7, D8, D14: block ids a current peer advertises are judged against the chain. A block the node has, a dead fork
/// (a block at or below its last irreversible block that is not its own) and a block implausibly far past its head
/// (more than GRAPHENE_NET_FUTURE_BLOCK_IDS_GRACE_BLOCKS) are not fetched; the next block is, once, though it is
/// advertised again while its request is outstanding and after it arrived (#1004). A protocol 106 peer advertises
/// by legacy block message hash, and the block is fetched by that hash.
BOOST_AUTO_TEST_CASE( block_advertisements_test )
{
  // early in the chain a block becomes irreversible HIVE_MAX_WITNESSES blocks after it, so a few more give a dead fork
  const uint32_t synced = HIVE_MAX_WITNESSES + 3;
  const std::vector< block_ptr > chain = generate_chain( synced + 3 );
  start_node();
  run( [&]()
  {
    sync_node( chain, synced );
    const uint32_t last_irreversible = last_irreversible_block_num();
    BOOST_REQUIRE_GE( last_irreversible, 1u );

    auto peer = make_peer( "advertising peer" );
    peer->provide_blocks( chain );
    peer->answer_after( fetch_items_message_type, fc::milliseconds( 500 ) );
    connect_active( *peer );
    const block_id_type known = chain[ last_irreversible ]->get_block_id();
    const block_id_type dead_fork = fake_block_id( last_irreversible, "dead fork" );
    const block_id_type far_ahead = fake_block_id( synced + GRAPHENE_NET_FUTURE_BLOCK_IDS_GRACE_BLOCKS + 1, "far ahead" );
    const block_id_type next = chain[ synced ]->get_block_id();
    peer->advertise( block_message_type, { known, dead_fork, far_ahead } );
    peer->advertise( block_message_type, { next } );
    BOOST_REQUIRE( wait_until( [&]() { return times_requested( *peer, next ) != 0; }, reply_timeout ) );
    peer->advertise( block_message_type, { next } );
    BOOST_REQUIRE( head_reaches( synced + 1 ) );
    peer->advertise( block_message_type, { next } );
    fc::usleep( quiet_period );
    BOOST_CHECK_EQUAL( times_requested( *peer, next ), 1u );
    BOOST_CHECK_EQUAL( times_requested( *peer, known ), 0u );
    BOOST_CHECK_EQUAL( times_requested( *peer, dead_fork ), 0u );
    BOOST_CHECK_EQUAL( times_requested( *peer, far_ahead ), 0u );

    auto old = make_peer( "protocol 106 peer" );
    old->hello().core_protocol_version = GRAPHENE_NET_MINIMUM_PROTOCOL_VERSION;
    old->provide_blocks( chain );
    connect_active( *old );
    const item_hash_t legacy_hash = chain[ synced + 1 ]->get_legacy_block_message_hash();
    old->advertise( block_message_type, { legacy_hash } );
    BOOST_REQUIRE( head_reaches( synced + 2 ) );
    BOOST_CHECK_EQUAL( times_requested( *old, legacy_hash ), 1u );
    BOOST_CHECK( head_block_id() == chain[ synced + 1 ]->get_block_id() );

    peer->advertise( block_message_type, { chain[ synced + 2 ]->get_block_id() } );
    BOOST_REQUIRE( head_reaches( synced + 3 ) );
    require_kept( *peer );
    require_kept( *old );
  } );
}

/// D7 (T): once the node's head has not advanced for GRAPHENE_NET_STALLED_HEAD_RESYNC_SEC (60 s), a block id implausibly
/// far past it is no longer ignored: the node restarts sync with the peer that advertised it and asks it for block
/// ids. Before that the same advertisement is ignored. The interval is hard-coded, so the case takes over a minute.
BOOST_AUTO_TEST_CASE( stalled_head_resync_test )
{
  const std::vector< block_ptr > chain = generate_chain( 5 );
  start_node();
  run( [&]()
  {
    sync_node( chain, 5 );
    // the node saw its head advance at the latest now
    const fc::time_point head_advanced = fc::time_point::now();
    auto peer = make_peer( "peer far ahead" );
    connect_active( *peer );
    const size_t id_requests = peer->count( fetch_blockchain_item_ids_message_type );
    const block_id_type far_ahead = fake_block_id( 5 + GRAPHENE_NET_FUTURE_BLOCK_IDS_GRACE_BLOCKS + 1, "far ahead" );
    const auto resynced = [&]() { return peer->count( fetch_blockchain_item_ids_message_type ) > id_requests; };

    peer->advertise( block_message_type, { far_ahead } );
    fc::usleep( quiet_period );
    BOOST_REQUIRE( !resynced() );

    // the advertisement is judged when it arrives, so it is repeated until the node acts on it
    const fc::time_point deadline = head_advanced + fc::seconds( GRAPHENE_NET_STALLED_HEAD_RESYNC_SEC + 1 + 10 );
    while( !resynced() && fc::time_point::now() < deadline )
    {
      peer->advertise( block_message_type, { far_ahead } );
      wait_until( resynced, fc::seconds( 1 ) );
    }
    BOOST_REQUIRE_MESSAGE( resynced(), "the node did not restart sync" );
    const fc::time_point asked = peer->received( fetch_blockchain_item_ids_message_type ).back().time;
    BOOST_CHECK_GE( ( asked - head_advanced ).count(), fc::seconds( GRAPHENE_NET_STALLED_HEAD_RESYNC_SEC - 1 ).count() );
    require_kept( *peer );
  } );
}

/// D13: a requested block that doesn't link to the node's chain (its parent is missing) restarts sync with the
/// peer that sent it: the node asks it for block ids again and syncs the missing block from it
BOOST_AUTO_TEST_CASE( unlinkable_block_test )
{
  const std::vector< block_ptr > chain = generate_chain( 7 );
  start_node();
  run( [&]()
  {
    sync_node( chain, 5 );
    auto peer = connect_live_peer( "peer ahead", chain );
    const size_t id_requests = peer->count( fetch_blockchain_item_ids_message_type );
    // from now on it offers its blocks when asked for ids, so the restarted sync finds the missing one
    peer->serve_blocks( chain );
    peer->advertise( block_message_type, { chain[ 6 ]->get_block_id() } );
    BOOST_REQUIRE( wait_until( [&]() { return times_requested( *peer, chain[ 6 ]->get_block_id() ) != 0; }, reply_timeout ) );
    BOOST_REQUIRE( wait_until( [&]() { return peer->count( fetch_blockchain_item_ids_message_type ) > id_requests; }, reply_timeout ) );
    BOOST_REQUIRE_MESSAGE( head_reaches( 7 ), "head is " << get_block_num() );
    BOOST_CHECK( head_block_id() == chain.back()->get_block_id() );
    require_kept( *peer );
  } );
}

/// D15: block advertisements are not throttled by the transaction inventory limit. A peer that advertised more
/// transactions than the limit (GRAPHENE_NET_MAX_INVENTORY_SIZE_IN_MINUTES * GRAPHENE_NET_MAX_TRX_PER_SECOND * 60)
/// gets the next block it advertises asked for at once: the inventory keeps room for blocks, and the transactions
/// beyond the limit are deferred (see inventory_limit_deferral_test) rather than taking that room. The peer
/// withholds everything it is asked for, so the ignored-request timeout is raised out of the way.
BOOST_AUTO_TEST_CASE( block_advertisements_not_throttled_test )
{
  start_node( R"("active_ignored_request_timeout_microseconds":)" + std::to_string( fc::seconds( 120 ).count() ), true );
  run( [&]()
  {
    fc::sleep_until( get_genesis_time() + HIVE_BLOCK_INTERVAL );
    wait_for_block_num( 1 );
    auto flooder = make_peer( "flooding peer" );
    connect_active( *flooder );
    const size_t limit = GRAPHENE_NET_MAX_INVENTORY_SIZE_IN_MINUTES * GRAPHENE_NET_MAX_TRX_PER_SECOND * 60;
    std::vector< item_hash_t > transactions;
    for( size_t i = 0; i < limit + 1000; ++i )
      transactions.push_back( fc::ripemd160::hash( "flooded transaction " + std::to_string( i ) ) );
    flooder->advertise( transactions );
    BOOST_REQUIRE( flooder->wait_for( fetch_items_message_type, reply_timeout ) );

    const block_id_type block = fake_block_id( get_block_num() + 1, "advertised block" );
    flooder->advertise( block_message_type, { block } );
    BOOST_CHECK( wait_until( [&]() { return times_requested( *flooder, block ) != 0; }, reply_timeout ) );
    BOOST_CHECK( !flooder->closed() );
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

/// Category 2 (fallback), D9, D12: a block the node asked one peer for is fetched from another peer that
/// advertised it too when the first one doesn't deliver it: when it ignores the request (it is disconnected once
/// active_ignored_request_timeout, 2 s here, passes, and only then is the other peer asked), when it answers
/// item_not_available (it is kept), and when it sends a copy whose body doesn't match the header (it is
/// disconnected, #999, and the block is fetched again from the other peer, #1001)
BOOST_AUTO_TEST_CASE( block_fallback_test )
{
  const fc::microseconds timeout = fc::seconds( 2 );
  const std::vector< block_ptr > chain = generate_chain( 8 );
  start_node( R"("active_ignored_request_timeout_microseconds":)" + std::to_string( timeout.count() ) );
  run( [&]()
  {
    sync_node( chain, 5 );
    auto backup = connect_live_peer( "backup", chain );
    const auto id = [&]( uint32_t num ) { return item_hash_t( chain[ num - 1 ]->get_block_id() ); };

    auto silent = make_peer( "silent advertiser" );
    silent->ignore( fetch_items_message_type );
    connect_active( *silent );
    silent->advertise( block_message_type, { id( 6 ) } );
    BOOST_REQUIRE( wait_until( [&]() { return times_requested( *silent, id( 6 ) ) != 0; }, reply_timeout ) );
    backup->advertise( block_message_type, { id( 6 ) } );
    BOOST_REQUIRE_MESSAGE( head_reaches( 6, timeout + fc::seconds( 1 + 10 ) ), "head is " << get_block_num() );
    BOOST_CHECK( silent->wait_closed( reply_timeout ) );
    BOOST_CHECK_EQUAL( times_requested( *backup, id( 6 ) ), 1u );
    BOOST_CHECK_GE( ( first_request_time( *backup, id( 6 ) ) - first_request_time( *silent, id( 6 ) ) ).count(),
                    timeout.count() - fc::milliseconds( 100 ).count() );

    auto empty_handed = make_peer( "empty-handed advertiser" );
    empty_handed->stop_withholding();
    connect_active( *empty_handed );
    empty_handed->advertise( block_message_type, { id( 7 ) } );
    BOOST_REQUIRE( wait_until( [&]() { return times_requested( *empty_handed, id( 7 ) ) != 0; }, reply_timeout ) );
    backup->advertise( block_message_type, { id( 7 ) } );
    BOOST_REQUIRE_MESSAGE( head_reaches( 7 ), "head is " << get_block_num() );
    require_kept( *empty_handed );

    auto corrupting = make_peer( "corrupting advertiser" );
    corrupting->provide_blocks( { with_corrupted_body( chain[ 7 ] ) } );
    // the backup advertises the block while the corrupted copy is on its way
    corrupting->answer_after( fetch_items_message_type, fc::milliseconds( 500 ) );
    connect_active( *corrupting );
    corrupting->advertise( block_message_type, { id( 8 ) } );
    BOOST_REQUIRE( wait_until( [&]() { return times_requested( *corrupting, id( 8 ) ) != 0; }, reply_timeout ) );
    backup->advertise( block_message_type, { id( 8 ) } );
    require_closed_with( *corrupting, "body does not match its header" );
    BOOST_REQUIRE_MESSAGE( head_reaches( 8 ), "head is " << get_block_num() );
    BOOST_CHECK( head_block_id() == chain[ 7 ]->get_block_id() );
    BOOST_CHECK_EQUAL( times_requested( *backup, id( 8 ) ), 1u );
    require_kept( *backup );
  } );
}

/// D11, category 2 (breakers) (T): a requested block that fails validation ends the sender's connection, and so
/// does one whose body doesn't match its header, until GRAPHENE_NET_INVALID_BLOCK_DISCONNECT_BREAKER_COUNT and
/// GRAPHENE_NET_BLOCK_BODY_MISMATCH_DISCONNECT_BREAKER_COUNT (5 each) such disconnects happened within the breaker
/// window (60 s): the next sender is kept, as so many bad peers suggest a problem of the node's own. Once the
/// node's head has not advanced for GRAPHENE_NET_BREAKER_STALL_OVERRIDE_SEC (60 s), the kept senders are
/// disconnected after all. The intervals are hard-coded, so the case takes over a minute.
BOOST_AUTO_TEST_CASE( disconnect_breakers_test )
{
  const std::vector< block_ptr > chain = generate_chain( 6 );
  start_node();
  run( [&]()
  {
    sync_node( chain, 5 );
    // the node saw its head advance at the latest now
    const fc::time_point head_advanced = fc::time_point::now();
    // returns the sender the tripped breaker keeps
    const auto trip_breaker = [&]( const block_ptr& bad, const std::string& reason, uint32_t breaker_count )
    {
      std::vector< std::unique_ptr< test_peer > > senders;
      for( uint32_t i = 0; i <= breaker_count; ++i )
        BOOST_TEST_CONTEXT( reason << ", sender " << i )
        {
          senders.push_back( make_peer( "bad block sender" ) );
          test_peer& peer = *senders.back();
          // each sender gets its own copy, so a bad signature gives each block another id
          const block_ptr copy = bad ? bad : with_bad_signature( chain[ 5 ] );
          peer.provide_blocks( { copy } );
          connect_active( peer );
          peer.advertise( block_message_type, { copy->get_block_id() } );
          // one sender at a time, so the breaker trips at a known one
          BOOST_REQUIRE( wait_until( [&]() { return times_requested( peer, copy->get_block_id() ) != 0; }, reply_timeout ) );
          if( i < breaker_count )
            require_closed_with( peer, reason );
          else
            require_kept( peer );
        }
      return std::move( senders.back() );
    };
    auto kept_invalid = trip_breaker( nullptr, "deemed to be invalid", GRAPHENE_NET_INVALID_BLOCK_DISCONNECT_BREAKER_COUNT );
    auto kept_mismatch = trip_breaker( with_corrupted_body( chain[ 5 ] ), "body does not match its header",
                                       GRAPHENE_NET_BLOCK_BODY_MISMATCH_DISCONNECT_BREAKER_COUNT );
    BOOST_CHECK_EQUAL( get_block_num(), 5u );

    const fc::time_point due = head_advanced + fc::seconds( GRAPHENE_NET_BREAKER_STALL_OVERRIDE_SEC );
    for( test_peer* kept : { kept_invalid.get(), kept_mismatch.get() } )
    {
      // the node checks once a second; the slack is for a loaded host
      BOOST_REQUIRE( wait_until( [&]() { return bool( kept->closing_message() ); }, due - fc::time_point::now() + fc::seconds( 1 + 10 ) ) );
      require_closed_with( *kept, "our chain has made no progress since" );
      BOOST_CHECK_GE( ( kept->received( closing_connection_message_type ).front().time - head_advanced ).count(),
                      fc::seconds( GRAPHENE_NET_BREAKER_STALL_OVERRIDE_SEC - 1 ).count() );
    }
  } );
}

/// Category 2 (crossed duplicates): the node dials a peer while the same peer identity (node id) connects to it
/// from another endpoint; exactly one of the two connections survives (#932). Of the two ends, the one with the
/// lower id rejects the second hello it gets as already_connected. When that is the peer, the node accepts both
/// hellos and leaves the choice to it; the two test peers play that part of a real node.
BOOST_AUTO_TEST_CASE( crossed_duplicate_connections_test )
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

    // ids compare bytewise: the next and the previous number order after and before the node's id
    node_id_t higher = node_id;
    for( size_t i = sizeof( higher.data ); i-- > 0 && ++reinterpret_cast< unsigned char& >( higher.data[ i ] ) == 0; )
      ;
    node_id_t lower = node_id;
    for( size_t i = sizeof( lower.data ); i-- > 0 && reinterpret_cast< unsigned char& >( lower.data[ i ] )-- == 0; )
      ;
    BOOST_REQUIRE( lower < node_id && node_id < higher );

    std::vector< std::unique_ptr< test_peer > > survivors;
    for( const node_id_t& id : { lower, higher } )
      BOOST_TEST_CONTEXT( ( id < node_id ? "lower" : "higher" ) << " id than the node's" )
      {
        auto dialed = make_peer( "dialed identity" );
        dialed->hello().node_id = id;
        auto dialing = make_peer( "dialing identity" );
        dialing->hello().node_id = id;
        // both peers live on the peer thread, so the node's hellos reach them one at a time
        auto hellos_accepted = std::make_shared< int >( 0 );
        const bool peer_chooses = id < node_id;
        for( test_peer* peer : { dialed.get(), dialing.get() } )
          peer->on( hello_message_type, [hellos_accepted, peer_chooses]( test_peer& self, const message& m )
          {
            if( peer_chooses && *hellos_accepted != 0 )
              self.send( connection_rejected_message( "test peer", GRAPHENE_NET_PROTOCOL_VERSION, self.endpoint(),
                                                      rejection_reason_code::already_connected, "already connected" ) );
            else
            {
              ++*hellos_accepted;
              self.answer( m );
            }
          } );
        const size_t before = number_of_connections();
        p2p().add_node( dialed->listen() );
        dialing->connect( node_endpoint() );
        BOOST_REQUIRE( wait_until( [&]() { return dialed->connected(); }, fc::seconds( 30 ) ) );
        BOOST_REQUIRE( wait_until( [&]() { return dialed->closed() || dialing->closed(); }, reply_timeout ) );
        fc::usleep( quiet_period );
        BOOST_CHECK_EQUAL( int( dialed->closed() ) + int( dialing->closed() ), 1 );
        BOOST_CHECK_EQUAL( number_of_connections(), before + 1 );
        test_peer& survivor = dialed->closed() ? *dialing : *dialed;
        BOOST_CHECK( survivor.accepted() );
        survivors.push_back( dialed->closed() ? std::move( dialing ) : std::move( dialed ) );
      }
  } );
}

/// Category 2 (address exchange): endpoints a peer sends in an address message go to the node's peer database,
/// so the node dials them. Asked for addresses, the node lists the active peers it can name an endpoint for: an
/// inbound peer that says where it listens, but not one that doesn't (firewalled).
BOOST_AUTO_TEST_CASE( address_exchange_test )
{
  start_node();
  run( [&]()
  {
    auto advertised = make_peer( "advertised peer" );
    const fc::ip::endpoint advertised_endpoint = advertised->listen();

    // the informant says it listens; nothing has to answer there for the node to list it
    fc::tcp_server informant_server;
    informant_server.listen( fc::ip::endpoint( fc::ip::address( "127.0.0.1" ), 0 ) );
    auto informant = make_peer( "informant" );
    informant->hello().inbound_port = informant_server.get_local_endpoint().port();
    connect_active( *informant );
    auto firewalled = make_peer( "firewalled peer" );
    connect_active( *firewalled );

    address_message addresses;
    addresses.addresses.emplace_back( advertised_endpoint, fc::time_point_sec( fc::time_point::now() ), fc::microseconds(), advertised->node_id(),
                                      peer_connection_direction::outbound, firewalled_state::not_firewalled );
    informant->send( addresses );
    BOOST_REQUIRE_MESSAGE( advertised->wait_for( fetch_blockchain_item_ids_message_type, fc::seconds( 30 ) ), "the node did not dial the advertised peer" );

    auto asking = make_peer( "asking peer" );
    connect_active( *asking );
    const address_message reply = request< address_message >( *asking, address_request_message() );
    const auto lists = [&]( const node_id_t& id ) -> fc::optional< address_info >
    {
      for( const address_info& a : reply.addresses )
        if( a.node_id == id )
          return a;
      return {};
    };
    const fc::optional< address_info > informant_address = lists( informant->node_id() );
    BOOST_REQUIRE( informant_address );
    BOOST_CHECK( informant_address->remote_endpoint == fc::ip::endpoint( fc::ip::address( "127.0.0.1" ), informant_server.get_local_endpoint().port() ) );
    BOOST_CHECK_EQUAL( static_cast< int >( informant_address->firewalled.value ), static_cast< int >( firewalled_state::not_firewalled ) );
    const fc::optional< address_info > advertised_address = lists( advertised->node_id() );
    BOOST_REQUIRE( advertised_address );
    BOOST_CHECK( advertised_address->remote_endpoint == advertised_endpoint );
    BOOST_CHECK( !lists( firewalled->node_id() ) );
    BOOST_CHECK( !lists( asking->node_id() ) );
  } );
}

/// Category 2 (firewall check): a peer that asks the node to check whether it is reachable is answered
/// unable_to_check while no other peer could do the check; once another peer is connected, the node forwards
/// the check to it and relays its answer
BOOST_AUTO_TEST_CASE( firewall_check_forwarding_test )
{
  start_node();
  run( [&]()
  {
    auto asking = make_peer( "peer asking to be checked" );
    connect_active( *asking );
    check_firewall_reply_message reply = request< check_firewall_reply_message >( *asking, check_firewall_message() );
    BOOST_CHECK_EQUAL( static_cast< int >( reply.result.value ), static_cast< int >( firewall_check_result::unable_to_check ) );

    auto checker = make_peer( "checking peer" );
    checker->on( check_firewall_message_type, []( test_peer& self, const message& m )
    {
      const check_firewall_message check = m.as< check_firewall_message >();
      check_firewall_reply_message answer;
      answer.node_id = check.node_id;
      answer.endpoint_checked = check.endpoint_to_check;
      answer.result = firewall_check_result::connection_successful;
      self.send( answer );
    } );
    connect_active( *checker );
    reply = request< check_firewall_reply_message >( *asking, check_firewall_message() );
    BOOST_REQUIRE_EQUAL( checker->count( check_firewall_message_type ), 1u );
    const check_firewall_message forwarded = checker->received_as< check_firewall_message >().front();
    BOOST_CHECK( forwarded.node_id == asking->node_id() );
    BOOST_CHECK( forwarded.endpoint_to_check.get_address() == asking->endpoint().get_address() );
    BOOST_CHECK_EQUAL( static_cast< int >( reply.result.value ), static_cast< int >( firewall_check_result::connection_successful ) );
    BOOST_CHECK( reply.endpoint_checked == forwarded.endpoint_to_check );
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

/// E12: a peer whose chain forked from the node's below the node's last irreversible block shares no block of
/// the synopsis the node sends (it starts there), so it has nothing for the node: none of its blocks is fetched,
/// the node keeps its own chain and the connection. Asked by that peer for the blocks after one of its fork, the
/// node offers none, again without a disconnect. The rest of the catalogue item is not reachable: handle_block()
/// never reports block_older_than_undo_history (a TODO in p2p_plugin), so fetching from such a peer is never
/// inhibited, and "you are on a fork I'm unable to switch to" needs the node to take a reference point on the
/// other fork, which it takes only from blocks it accepted and ids it has.
BOOST_AUTO_TEST_CASE( fork_below_last_irreversible_test )
{
  // early in the chain a block becomes irreversible HIVE_MAX_WITNESSES blocks after it
  const uint32_t length = HIVE_MAX_WITNESSES + 5;
  const std::vector< block_ptr > fork = generate_chain( length + 8, 3 );
  const std::vector< block_ptr > chain = generate_chain( length );
  BOOST_REQUIRE( fork[ 1 ]->get_block_id() == chain[ 1 ]->get_block_id() );
  BOOST_REQUIRE( fork[ 2 ]->get_block_id() != chain[ 2 ]->get_block_id() );
  start_node();
  run( [&]()
  {
    sync_node( chain, length );
    BOOST_REQUIRE_GE( last_irreversible_block_num(), 3u );

    auto forked = make_peer( "peer on an old fork" );
    forked->serve_blocks( fork );
    forked->connect( node_endpoint() );
    BOOST_REQUIRE( forked->wait_for( fetch_blockchain_item_ids_message_type, reply_timeout ) );
    BOOST_REQUIRE( wait_until( [&]() { return forked->in_sync(); }, reply_timeout ) );
    const blockchain_item_ids_inventory_message reply =
      request< blockchain_item_ids_inventory_message >( *forked, fetch_blockchain_item_ids_message( block_message_type, { fork[ length + 4 ]->get_block_id() } ) );
    BOOST_CHECK( reply.item_hashes_available.empty() );
    require_kept( *forked );
    BOOST_CHECK_EQUAL( forked->count( fetch_items_message_type ), 0u );
    BOOST_CHECK( head_block_id() == chain.back()->get_block_id() );
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

/// Category 2 (sync from several peers): sync requests are spread over the peers that have the blocks, and when
/// one of them leaves in the middle of the sync the other completes it. Both deliver each batch 300 ms after the
/// request, so the second peer joins before the first could deliver everything.
BOOST_AUTO_TEST_CASE( sync_from_several_peers_test )
{
  const uint32_t count = 40;
  const std::vector< block_ptr > chain = generate_chain( count );
  start_node( R"("maximum_blocks_per_peer_during_syncing":5)" );
  run( [&]()
  {
    const fc::microseconds delay = fc::milliseconds( 300 );
    auto leaving = make_peer( "source leaving after two batches" );
    leaving->serve_blocks( chain );
    leaving->on( fetch_items_message_type, [delay]( test_peer& self, const message& m )
    {
      self.later( delay, [&self, m]()
      {
        self.answer( m );
        if( self.count( fetch_items_message_type ) >= 2 )
          self.later( fc::milliseconds( 50 ), [&self]() { self.close(); } );
      } );
    } );
    auto staying = make_peer( "staying source" );
    staying->serve_blocks( chain );
    staying->answer_after( fetch_items_message_type, delay );
    leaving->connect( node_endpoint() );
    staying->connect( node_endpoint() );

    BOOST_REQUIRE_MESSAGE( head_reaches( count, fc::seconds( 60 ) ), "head is " << get_block_num() );
    BOOST_CHECK( head_block_id() == chain.back()->get_block_id() );
    BOOST_CHECK( leaving->closed() );
    BOOST_CHECK_GE( leaving->count( fetch_items_message_type ), 1u );
    BOOST_CHECK_GE( staying->count( fetch_items_message_type ), 1u );
    require_kept( *staying );
  } );
}

/// Category 2 (stalling peer): a sync source that stops delivering doesn't hold the sync up for longer than
/// active_ignored_request_timeout (2 s here): it is disconnected and an honest source delivers its blocks. In
/// live mode, a peer advertising a run of block ids it then withholds doesn't keep the node from fetching the
/// real next block from an honest peer.
BOOST_AUTO_TEST_CASE( stalling_peer_test )
{
  const fc::microseconds timeout = fc::seconds( 2 );
  const uint32_t count = 20;
  const std::vector< block_ptr > chain = generate_chain( count + 1 );
  const std::vector< block_ptr > sync_part( chain.begin(), chain.begin() + count );
  start_node( R"("active_ignored_request_timeout_microseconds":)" + std::to_string( timeout.count() ) +
              R"(,"maximum_blocks_per_peer_during_syncing":5)" );
  run( [&]()
  {
    auto staller = make_peer( "stalling source" );
    staller->serve_blocks( sync_part );
    staller->ignore( fetch_items_message_type );
    staller->connect( node_endpoint() );
    BOOST_REQUIRE( staller->wait_for( fetch_items_message_type, reply_timeout ) );
    auto honest = make_peer( "honest source" );
    honest->serve_blocks( sync_part );
    honest->connect( node_endpoint() );
    BOOST_REQUIRE_MESSAGE( head_reaches( count, timeout + fc::seconds( 1 + 10 + 10 ) ), "head is " << get_block_num() );
    BOOST_CHECK( staller->closed() );
    BOOST_REQUIRE( wait_until( [&]() { return honest->in_sync(); }, reply_timeout ) );

    honest->provide_blocks( chain );
    auto flooder = make_peer( "withholding advertiser" );
    connect_active( *flooder );
    std::vector< item_hash_t > fakes;
    for( uint32_t num = count + 1; num <= count + GRAPHENE_NET_FUTURE_BLOCK_IDS_GRACE_BLOCKS; ++num )
      fakes.push_back( fake_block_id( num, "withheld block" ) );
    flooder->advertise( block_message_type, fakes );
    BOOST_REQUIRE( flooder->wait_for( fetch_items_message_type, reply_timeout ) );
    honest->advertise( block_message_type, { chain.back()->get_block_id() } );
    BOOST_REQUIRE_MESSAGE( head_reaches( count + 1 ), "head is " << get_block_num() );
    BOOST_CHECK( head_block_id() == chain.back()->get_block_id() );
    BOOST_CHECK( !honest->closed() );
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

/// F3 (T): an outbound peer that rejected the node, or closed the connection during the handshake, is dialed
/// again only once the retry delay has passed: peer_connection_retry_timeout (2 s here) times the number of failed
/// attempts plus one, so 4 s after a rejection (which counts as a failed attempt) and 2 s after the closed
/// handshake. The connect loop runs at least every GRAPHENE_NET_CONNECT_LOOP_MAXIMUM_WAIT_SEC (10 s), so the redial
/// comes by then, plus slack. The second dial is answered by a new peer listening on the same port.
BOOST_AUTO_TEST_CASE( outbound_retry_delay_test )
{
  const uint32_t retry = 2;
  start_node( R"("peer_connection_retry_timeout":)" + std::to_string( retry ) );
  run( [&]()
  {
    const auto redial_delay = [&]( std::unique_ptr< test_peer > first, const std::function< void( test_peer& ) >& fail )
    {
      const fc::ip::endpoint endpoint = first->listen();
      p2p().add_node( endpoint );
      BOOST_REQUIRE_MESSAGE( wait_until( [&]() { return first->connected(); }, fc::seconds( 30 ) ), "the node did not dial the peer" );
      const fc::time_point dialed = first->connected_at();
      fail( *first );
      BOOST_REQUIRE( first->wait_closed( reply_timeout ) );
      first.reset();
      auto again = make_peer( "peer listening again" );
      again->listen( endpoint.port() );
      BOOST_REQUIRE_MESSAGE( again->wait_for( fetch_blockchain_item_ids_message_type, fc::seconds( 2 * retry + GRAPHENE_NET_CONNECT_LOOP_MAXIMUM_WAIT_SEC + 10 ) ),
                             "the node did not dial the peer again" );
      return again->connected_at() - dialed;
    };
    // the node records the attempt in whole seconds
    const fc::microseconds rounding = fc::seconds( 1 );

    auto rejecting = make_peer( "rejecting peer" );
    rejecting->on( hello_message_type, []( test_peer& self, const message& )
    {
      self.send( connection_rejected_message( "test peer", GRAPHENE_NET_PROTOCOL_VERSION, self.endpoint(),
                                              rejection_reason_code::not_accepting_connections, "rejected by the test" ) );
    } );
    const fc::microseconds after_rejection = redial_delay( std::move( rejecting ), []( test_peer& ) {} );
    BOOST_CHECK_GE( after_rejection.count(), ( fc::seconds( 2 * retry ) - rounding ).count() );

    auto closing = make_peer( "peer closing the handshake" );
    closing->hello().count = 0;
    closing->ignore( hello_message_type );
    closing->ignore( legacy_hello_message_type );
    const fc::microseconds after_closing = redial_delay( std::move( closing ), []( test_peer& peer ) { peer.close(); } );
    BOOST_CHECK_GE( after_closing.count(), ( fc::seconds( retry ) - rounding ).count() );
  } );
}

BOOST_AUTO_TEST_SUITE_END()

#endif
