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
#include <fc/io/json.hpp>

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <functional>
#include <map>
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
 * producing node (the witness_fixture default, genesis 3 s ahead and a block every 3 s); those that need its pending
 * pool to hold still while they check it stop its block production (stop_block_production()). Blocks an idle node
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

  /// What the node reports of its connection to @p peer; requires that it lists it
  fc::variant_object peer_info( const test_peer& peer ) { return peer_info( peer.endpoint() ); }

  /// What the node reports of its connection from @p endpoint; requires that it lists it
  fc::variant_object peer_info( const fc::ip::endpoint& endpoint )
  {
    for( const hive::plugins::p2p::api_peer_status& status : p2p().get_connected_peers() )
      if( status.host == endpoint )
        return status.info;
    BOOST_FAIL( "the node does not list the peer" );
    return fc::variant_object();
  }

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

  /**
   * Connects through @p link a peer that delivers the blocks of @p chain it is asked for, and has it deliver block
   * @p delivered_num, so the node asks it for blocks before peers that never delivered one
   */
  std::unique_ptr< test_peer > connect_proven_peer_through( throttling_relay& link, const std::string& name,
                                                            const std::vector< block_ptr >& chain, uint32_t delivered_num )
  {
    auto peer = make_peer( name );
    peer->provide_blocks( chain );
    peer->connect( link.listen() );
    BOOST_REQUIRE_MESSAGE( peer->wait_for( fetch_blockchain_item_ids_message_type, reply_timeout ), "the node did not take up the peer" );
    peer->advertise( block_message_type, { item_hash_t( chain[ delivered_num - 1 ]->get_block_id() ) } );
    BOOST_REQUIRE_MESSAGE( head_reaches( delivered_num ), "head is " << get_block_num() );
    return peer;
  }

  /**
   * Throttles @p link so that a message carrying @p block reaches the node one AES block at a time, spread over @p time.
   * The read the relay already waits on is not throttled: the next message the peer sends (an advertisement) passes
   * at once, and the ones after it, the block, are throttled
   */
  static void stream_through( throttling_relay& link, const block_ptr& block, fc::microseconds time )
  {
    const message delivery{ block_message( block ) };
    const size_t aes_blocks = ( sizeof( message_header ) + delivery.size + 15 ) / 16;
    link.throttle( 16, fc::microseconds( time.count() / int64_t( aes_blocks ) ) );
  }

  /**
   * Stops a producing node's block production for the rest of the case, so that what the case puts in the pending
   * pool stays there however long its checks take. The node stays at live sync for a minute after its last block.
   * A block the witness plugin began producing before the stop can still land after this returns (it waits a block
   * interval for one), but no later one does.
   */
  void stop_block_production()
  {
    // under the write lock: the witness plugin reads its witnesses while it applies a block
    db->with_write_lock( [&]() { witness_plugin->set_witnesses( {} ); } );
    fc::usleep( fc::seconds( HIVE_BLOCK_INTERVAL ) );
  }

  /// Waits until the pending pool size the node's fetch gate reads exceeds @p threshold or a block comes; requires one of them
  void wait_for_pool_over( uint64_t threshold, uint32_t head )
  {
    BOOST_REQUIRE_MESSAGE( wait_until( [&]()
    {
      return get_chain_plugin().get_published_pending_transactions_size() > threshold || get_block_num() != head;
    }, reply_timeout ), "the pending pool is " << get_chain_plugin().get_published_pending_transactions_size() << " bytes" );
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

/// A current_time_request as a raw_connection writes it, which the node answers before a hello
std::vector< char > time_request_frame()
{
  const message request( current_time_request_message( fc::time_point::now() ) );
  return raw_connection::frame( request.size, request.msg_type, request.data );
}

/// Connects @p count raw connections to @p node with the key exchange, one after another, and returns those the
/// node took on; the others it closed at accept, so their key exchange failed. Every tenth connection, each one
/// taken on sends a time request, which keeps the handshake inactivity timeout from closing it.
std::vector< std::unique_ptr< raw_connection > > connect_without_hello( fc::thread& thread, const fc::ip::endpoint& node, size_t count )
{
  std::vector< std::unique_ptr< raw_connection > > taken_on;
  for( size_t i = 0; i < count; ++i )
  {
    auto connection = std::make_unique< raw_connection >( thread );
    try
    {
      connection->connect_secure( node );
      taken_on.push_back( std::move( connection ) );
    }
    catch( const fc::exception& )
    {
      // closed at accept: the node sent no key
    }
    if( i % 10 == 9 )
      for( const auto& held : taken_on )
        held->write( time_request_frame() );
  }
  return taken_on;
}

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

/**
 * Holds the chain's write thread inside the application of block @p block_num until open() (or destruction), as a
 * plugin applying backpressure (HAF's block buffer) does: the p2p thread keeps running, but nothing the node hands
 * the chain completes. While it holds, nothing may wait for the chain's read lock.
 */
class chain_stall
{
public:
  chain_stall( hive::chain::database& db, hive::plugins::chain::chain_plugin& chain, uint32_t block_num )
  {
    _connection = db.add_pre_apply_block_handler( [this, block_num]( const hive::chain::block_notification& note )
    {
      if( note.block_num != block_num )
        return;
      std::unique_lock< std::mutex > lock( _mutex );
      _holding = true;
      _released.wait( lock, [this]() { return _open; } );
      _holding = false;
    }, chain, 0 );
  }
  ~chain_stall()
  {
    open();
    hive::utilities::disconnect_signal( _connection );
  }

  /// whether the write thread is held now
  bool holding() const { std::lock_guard< std::mutex > guard( _mutex ); return _holding; }

  void open()
  {
    {
      std::lock_guard< std::mutex > guard( _mutex );
      _open = true;
    }
    _released.notify_all();
  }

private:
  mutable std::mutex _mutex;
  std::condition_variable _released;
  bool _holding = false;
  bool _open = false;
  hive::chain::database::signal_connection_ptr _connection;
};

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
      rejection_reason_code::invalid_hello_message, rejection_reason_code::client_too_old,
      rejection_reason_code::too_many_connections_from_host };
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

/// A14 (T): 200 connections from one host (loopback, exempt from the per-host limit) that do the key exchange and
/// never send a hello, though they send a time request every second, which the node answers: the node takes on
/// GRAPHENE_NET_MAXIMUM_HANDSHAKING_INBOUND_CONNECTIONS of them and closes the rest at accept. They hold no
/// connection slot, so meanwhile the node, short of its desired 2 connections, dials a peer another peer tells it
/// about. The time requests don't keep them open: each is closed GRAPHENE_NET_HANDSHAKE_DEADLINE_SEC after it
/// started, not before. Then a peer connecting is taken up.
BOOST_AUTO_TEST_CASE( pre_hello_flood_test )
{
  start_node( R"("desired_number_of_connections":2)" );
  run( [&]()
  {
    auto relay = make_peer( "address relay" );
    connect_active( *relay );

    const fc::time_point flood_started = fc::time_point::now();
    std::vector< std::unique_ptr< raw_connection > > held = connect_without_hello( peer_thread, node_endpoint(), 200 );
    const fc::time_point flood_done = fc::time_point::now();
    BOOST_REQUIRE_EQUAL( held.size(), size_t( GRAPHENE_NET_MAXIMUM_HANDSHAKING_INBOUND_CONNECTIONS ) );
    const auto open_count = [&]() { return size_t( std::count_if( held.begin(), held.end(), []( const auto& c ) { return !c->closed(); } ) ); };
    BOOST_REQUIRE_EQUAL( open_count(), held.size() );

    // add_node() would dial however many connections the node has; a learned address is dialed only while it wants more
    auto dialed = make_peer( "peer the node dials during the flood" );
    const fc::ip::endpoint dialed_endpoint = dialed->listen();
    const fc::time_point dial_deadline = fc::time_point::now() + reply_timeout;
    while( !dialed->connected() && fc::time_point::now() < dial_deadline )
    {
      address_message relayed;
      relayed.addresses.emplace_back( dialed_endpoint, fc::time_point::now(), fc::microseconds( 0 ), node_id_t(),
                                      peer_connection_direction::inbound, firewalled_state::not_firewalled );
      relay->send( relayed );
      fc::usleep( fc::milliseconds( 250 ) );
    }
    BOOST_CHECK_MESSAGE( dialed->wait_for( fetch_blockchain_item_ids_message_type, reply_timeout ), "the node did not take up the peer it dialed" );
    BOOST_CHECK( node_lists( *dialed ) );
    BOOST_REQUIRE_MESSAGE( fc::time_point::now() < flood_started + fc::seconds( GRAPHENE_NET_HANDSHAKE_DEADLINE_SEC - 2 ),
                           "the dial took too long to tell whether the flood was still held" );
    BOOST_CHECK_EQUAL( open_count(), held.size() );

    // the node checks once a second; the slack is for a loaded host
    const fc::time_point latest = flood_done + fc::seconds( GRAPHENE_NET_HANDSHAKE_DEADLINE_SEC + 1 + 10 );
    while( open_count() > 0 && fc::time_point::now() < latest )
    {
      for( const auto& connection : held )
        if( !connection->closed() )
        {
          try
          {
            connection->write( time_request_frame() );
          }
          catch( const fc::exception& )
          {
            // the node closed it since closed() was read
          }
        }
      fc::usleep( fc::seconds( 1 ) );
    }
    BOOST_REQUIRE_EQUAL( open_count(), 0u );
    // all of them started after the flood did; the slack is for the node checking once a second
    const fc::time_point earliest = flood_started + fc::seconds( GRAPHENE_NET_HANDSHAKE_DEADLINE_SEC ) - fc::seconds( 1 );
    for( const auto& connection : held )
      BOOST_CHECK_GE( connection->closed_at().time_since_epoch().count(), earliest.time_since_epoch().count() );

    auto after = make_peer( "peer after the flood" );
    connect_active( *after );
    require_kept( *dialed );
    require_kept( *relay );
  } );
}

/// A15: with loopback not exempt from the per-host limit, connections from 127.0.0.1 that do the key exchange and
/// send no hello are taken on only GRAPHENE_NET_MAXIMUM_HANDSHAKING_CONNECTIONS_PER_HOST at a time: the rest, and a
/// peer on that host that would send its hello, are closed at accept. A peer on another host (127.0.0.2) is taken up.
BOOST_AUTO_TEST_CASE( pre_hello_connections_per_host_test )
{
  start_node( R"("host_limit_exempt_subnets":[],"peer_inactivity_timeout":60)" );
  run( [&]()
  {
    std::vector< std::unique_ptr< raw_connection > > held = connect_without_hello( peer_thread, node_endpoint(), 20 );
    BOOST_CHECK_EQUAL( held.size(), size_t( GRAPHENE_NET_MAXIMUM_HANDSHAKING_CONNECTIONS_PER_HOST ) );

    auto same_host = make_peer( "peer on the flooding host" );
    BOOST_CHECK_THROW( same_host->connect( node_endpoint() ), fc::exception );

    auto other_host = make_peer( "peer on another host" );
    other_host->connect( node_endpoint(), "127.0.0.2" );
    BOOST_REQUIRE_MESSAGE( other_host->wait_for( fetch_blockchain_item_ids_message_type, reply_timeout ), "the node did not take up the peer" );
    BOOST_CHECK( node_lists( *other_host ) );
    for( const auto& connection : held )
      BOOST_CHECK( !connection->closed() );
  } );
}

/// A16: until the node accepts a peer's hello, a message header announcing more than
/// GRAPHENE_NET_MAXIMUM_HANDSHAKE_MESSAGE_SIZE ends the connection as soon as it is read, while one announcing less
/// waits for its bytes; once the hello is accepted, a larger message (of a type the node ignores) is read whole
BOOST_AUTO_TEST_CASE( pre_hello_message_size_test )
{
  start_node();
  run( [&]()
  {
    raw_connection oversized( peer_thread );
    oversized.connect_secure( node_endpoint() );
    oversized.write( raw_connection::frame( GRAPHENE_NET_MAXIMUM_HANDSHAKE_MESSAGE_SIZE + 1, current_time_request_message_type ) );

    raw_connection within_limit( peer_thread );
    within_limit.connect_secure( node_endpoint() );
    within_limit.write( raw_connection::frame( GRAPHENE_NET_MAXIMUM_HANDSHAKE_MESSAGE_SIZE, current_time_request_message_type ) );

    BOOST_CHECK( oversized.wait_closed( reply_timeout ) );
    fc::usleep( quiet_period );
    BOOST_CHECK( !within_limit.closed() );

    auto active = make_peer( "active peer" );
    connect_active( *active );
    active->send_raw( 1234, garbage( 2 * GRAPHENE_NET_MAXIMUM_HANDSHAKE_MESSAGE_SIZE ) );
    require_kept( *active );
    request< current_time_reply_message >( *active, current_time_request_message( fc::time_point::now() ) );
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
      item_not_available_message_type, item_busy_message_type, current_time_request_message_type, current_time_reply_message_type,
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
    // the node lists the connection for a moment after closing it on the overflow; listing it must not fail meanwhile
    const fetch_blockchain_item_ids_message request( block_message_type, {} );
    bool dropped = false;
    for( int sent = 0; sent < 20000 && !dropped; sent += 500 )
    {
      for( int i = 0; i < 500; ++i )
        stalled->send( request );
      dropped = wait_until( [&]() { return !node_lists( *stalled ); }, fc::milliseconds( 200 ) );
    }
    BOOST_REQUIRE_MESSAGE( dropped, "the node kept a peer that doesn't read" );
    resume = true;
    BOOST_CHECK( stalled->wait_closed( reply_timeout ) );
    require_kept( *bystander );
    BOOST_CHECK( node_lists( *bystander ) );
  } );
}

/// B7 (T): a peer that stops reading while the node has replies for it, and keeps sending it time requests so it
/// doesn't look idle, is disconnected once nothing has been written to it for GRAPHENE_NET_SEND_STALL_DISCONNECT_SEC,
/// though its send queue never overflows. A peer that stops reading as long, less a few seconds, and then reads again
/// is kept. Each stops reading by not returning from its handler of a reply, and asks for the ids of all 300 blocks
/// (6 kB a reply) a few at a time until the node has written nothing to it for half a second, which leaves far less
/// than GRAPHENE_NET_MAXIMUM_QUEUED_MESSAGES_IN_BYTES queued.
BOOST_AUTO_TEST_CASE( slow_reader_test )
{
  start_node();
  run( [&]()
  {
    schedule_blocks( 300 );
    auto stalled = make_peer( "peer that stops reading" );
    auto brief = make_peer( "peer that stops reading for a while" );
    std::atomic< bool > stalled_reads = false;
    std::atomic< bool > brief_reads = false;
    BOOST_SCOPE_EXIT( &stalled_reads, &brief_reads ) { stalled_reads = true; brief_reads = true; } BOOST_SCOPE_EXIT_END
    const auto stop_reading = []( test_peer& peer, std::atomic< bool >& reads )
    {
      peer.on( blockchain_item_ids_inventory_message_type, [&reads]( test_peer&, const message& )
      {
        while( !reads )
          fc::usleep( fc::milliseconds( 20 ) );
      } );
    };
    connect_active( *stalled );
    connect_active( *brief );
    BOOST_REQUIRE( wait_until( [&]() { return node_lists( *stalled ) && node_lists( *brief ); }, reply_timeout ) );
    stop_reading( *stalled, stalled_reads );
    stop_reading( *brief, brief_reads );

    const auto bytes_sent_to = [&]( const test_peer& peer )
    {
      for( const hive::plugins::p2p::api_peer_status& status : p2p().get_connected_peers() )
        if( status.host == peer.endpoint() )
          return status.info[ "bytessent" ].as_uint64();
      BOOST_FAIL( "the node does not list the peer" );
      return uint64_t( 0 );
    };
    const fetch_blockchain_item_ids_message ids_request( block_message_type, {} );
    std::vector< test_peer* > filling = { stalled.get(), brief.get() };
    std::map< test_peer*, uint64_t > last_sent;
    std::map< test_peer*, int > unchanged;
    std::map< test_peer*, fc::time_point > stalled_at;
    const fc::time_point fill_deadline = fc::time_point::now() + fc::seconds( 60 );
    while( !filling.empty() && fc::time_point::now() < fill_deadline )
    {
      for( test_peer* peer : filling )
        for( int i = 0; i < 10; ++i )
          peer->send( ids_request );
      fc::usleep( fc::milliseconds( 100 ) );
      for( auto peer = filling.begin(); peer != filling.end(); )
      {
        const uint64_t sent = bytes_sent_to( **peer );
        unchanged[ *peer ] = sent == last_sent[ *peer ] ? unchanged[ *peer ] + 1 : 0;
        last_sent[ *peer ] = sent;
        if( unchanged[ *peer ] < 5 )
          ++peer;
        else
        {
          stalled_at[ *peer ] = fc::time_point::now();
          peer = filling.erase( peer );
        }
      }
    }
    BOOST_REQUIRE_MESSAGE( filling.empty(), "the node kept writing to a peer that doesn't read" );

    // the stalled peer's read loop waits in its handler, so it is the node that tells when it dropped the peer
    const fc::time_point latest = stalled_at[ stalled.get() ] + fc::seconds( GRAPHENE_NET_SEND_STALL_DISCONNECT_SEC + 15 );
    const fc::time_point brief_resumes = stalled_at[ brief.get() ] + fc::seconds( GRAPHENE_NET_SEND_STALL_DISCONNECT_SEC - 10 );
    while( node_lists( *stalled ) && fc::time_point::now() < latest )
    {
      stalled->send( current_time_request_message( fc::time_point::now() ) );
      brief->send( current_time_request_message( fc::time_point::now() ) );
      if( fc::time_point::now() >= brief_resumes )
        brief_reads = true;
      fc::usleep( fc::seconds( 1 ) );
    }
    const fc::time_point dropped_at = fc::time_point::now();
    BOOST_REQUIRE_MESSAGE( !node_lists( *stalled ), "the node kept a peer that doesn't read" );
    BOOST_CHECK_GE( ( dropped_at - stalled_at[ stalled.get() ] ).count(), fc::seconds( GRAPHENE_NET_SEND_STALL_DISCONNECT_SEC - 5 ).count() );
    stalled_reads = true;
    BOOST_CHECK( stalled->wait_closed( reply_timeout ) );
    BOOST_CHECK( brief_reads );
    require_kept( *brief );
    BOOST_CHECK( node_lists( *brief ) );
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

/// C4: a fetch_items asking for more blocks than GRAPHENE_NET_MAX_BLOCKS_PER_FETCH_REQUEST or more transactions than
/// GRAPHENE_NET_MAX_TRANSACTIONS_PER_FETCH_REQUEST ends the connection; a request of exactly that size is served
BOOST_AUTO_TEST_CASE( fetch_items_cap_test )
{
  start_node();
  run( [&]()
  {
    schedule_blocks( 3 );
    const auto unknown_ids = []( size_t count )
    {
      std::vector< item_hash_t > ids;
      for( size_t i = 0; i < count; ++i )
        ids.push_back( fc::ripemd160::hash( "unknown item " + std::to_string( i ) ) );
      return ids;
    };

    auto blocks_at_cap = make_peer( "peer asking for the most blocks" );
    connect_active( *blocks_at_cap );
    std::vector< item_hash_t > blocks = unknown_ids( GRAPHENE_NET_MAX_BLOCKS_PER_FETCH_REQUEST - 1 );
    blocks.insert( blocks.begin(), block_id( 2 ) );
    blocks_at_cap->send( fetch_items_message( block_message_type, blocks ) );
    BOOST_REQUIRE( blocks_at_cap->wait_for( compressed_block_message_type, reply_timeout ) );
    BOOST_REQUIRE( blocks_at_cap->wait_for( item_not_available_message_type, reply_timeout, GRAPHENE_NET_MAX_BLOCKS_PER_FETCH_REQUEST - 1 ) );

    auto transactions_at_cap = make_peer( "peer asking for the most transactions" );
    connect_active( *transactions_at_cap );
    transactions_at_cap->send( fetch_items_message( trx_message_type, unknown_ids( GRAPHENE_NET_MAX_TRANSACTIONS_PER_FETCH_REQUEST ) ) );
    BOOST_REQUIRE( transactions_at_cap->wait_for( item_not_available_message_type, reply_timeout, GRAPHENE_NET_MAX_TRANSACTIONS_PER_FETCH_REQUEST ) );

    auto too_many_blocks = make_peer( "peer asking for too many blocks" );
    connect_active( *too_many_blocks );
    too_many_blocks->send( fetch_items_message( block_message_type, unknown_ids( GRAPHENE_NET_MAX_BLOCKS_PER_FETCH_REQUEST + 1 ) ) );
    require_closed_with( *too_many_blocks, "You asked for more items in one request than allowed" );
    BOOST_CHECK_EQUAL( too_many_blocks->count( item_not_available_message_type ), 0u );

    auto too_many_transactions = make_peer( "peer asking for too many transactions" );
    connect_active( *too_many_transactions );
    too_many_transactions->send( fetch_items_message( trx_message_type, unknown_ids( GRAPHENE_NET_MAX_TRANSACTIONS_PER_FETCH_REQUEST + 1 ) ) );
    require_closed_with( *too_many_transactions, "You asked for more items in one request than allowed" );
    BOOST_CHECK_EQUAL( too_many_transactions->count( item_not_available_message_type ), 0u );

    require_kept( *blocks_at_cap );
    require_kept( *transactions_at_cap );
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
    // a head no higher than the node's keeps the restart without any advertisement (stalled_head_restarts_sync_test) out of the way
    peer->hello().last_known_block_number = 5;
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

/// D7b: every connection the node has was made while its chain was stalled: the write thread is held inside the
/// application of a sync block, with more blocks of the peer that delivered them handed to the chain than it takes at
/// once (maximum_number_of_blocks_to_handle_at_one_time, 4 here), and that peer left. The new peers are asked for
/// blocks the chain then applies from the copies of the peer that left. Once the chain resumes, the node goes on
/// syncing from them at once, although none of them advertises anything.
BOOST_AUTO_TEST_CASE( stall_with_new_connections_test )
{
  const uint32_t count = 30;
  const uint32_t stall_at = 11;
  const std::vector< block_ptr > chain = generate_chain( count );
  start_node( R"("maximum_blocks_per_peer_during_syncing":5,"maximum_number_of_blocks_to_handle_at_one_time":4)" );
  run( [&]()
  {
    sync_node( chain, stall_at - 1 );
    BOOST_REQUIRE( wait_until( [&]() { return number_of_connections() == 0; }, reply_timeout ) );
    chain_stall stall( *db, get_chain_plugin(), stall_at );

    auto before = make_peer( "source before the stall" );
    before->serve_blocks( chain );
    connect_active( *before );
    BOOST_REQUIRE_MESSAGE( wait_until( [&]() { return stall.holding(); }, reply_timeout ), "the chain did not stall" );
    // it delivers the rest of the chain, which waits for the chain to take more
    BOOST_REQUIRE( wait_until( [&]() { return times_requested( *before, chain.back()->get_block_id() ) != 0; }, reply_timeout ) );
    fc::usleep( fc::seconds( 1 ) );
    before->close();
    BOOST_REQUIRE( wait_until( [&]() { return number_of_connections() == 0; }, reply_timeout ) );

    std::vector< std::unique_ptr< test_peer > > during;
    for( const char* name : { "source connected during the stall", "second source connected during the stall" } )
    {
      during.push_back( make_peer( name ) );
      during.back()->serve_blocks( chain );
      connect_active( *during.back() );
    }
    BOOST_REQUIRE( wait_until( [&]() { return during.front()->count( fetch_items_message_type ) != 0; }, reply_timeout ) );
    fc::usleep( quiet_period );
    BOOST_REQUIRE( stall.holding() );
    stall.open();

    BOOST_REQUIRE_MESSAGE( head_reaches( count ), "head is " << get_block_num() );
    BOOST_CHECK( head_block_id() == chain.back()->get_block_id() );
    for( const auto& peer : during )
      BOOST_CHECK( !peer->closed() );
  } );
}

/// D7c (T): a peer that is ahead of the node but advertises nothing (it answered the node's request for block ids
/// before it had the blocks, and it doesn't advertise to a node it knows to be behind) is asked for block ids again
/// once the node's head has not advanced for GRAPHENE_NET_STALLED_HEAD_RESYNC_SEC (60 s), and the node syncs from
/// it. Once the node caught up, a quiet chain doesn't make it ask again. The interval is hard-coded, so the case
/// takes over two minutes.
BOOST_AUTO_TEST_CASE( stalled_head_restarts_sync_test )
{
  const uint32_t count = 20;
  const uint32_t synced = 10;
  const std::vector< block_ptr > chain = generate_chain( count );
  start_node();
  run( [&]()
  {
    sync_node( chain, synced );
    // the node saw its head advance at the latest now
    const fc::time_point head_advanced = fc::time_point::now();
    auto ahead = make_peer( "silent peer ahead" );
    ahead->hello().last_known_block_number = count;
    connect_active( *ahead );
    BOOST_REQUIRE( wait_until( [&]() { return ahead->in_sync(); }, reply_timeout ) );
    ahead->serve_blocks( chain );
    const size_t id_requests = ahead->count( fetch_blockchain_item_ids_message_type );

    BOOST_REQUIRE_MESSAGE( head_reaches( count, fc::seconds( GRAPHENE_NET_STALLED_HEAD_RESYNC_SEC + 1 + 10 ) ), "head is " << get_block_num() );
    BOOST_CHECK( head_block_id() == chain.back()->get_block_id() );
    const fc::time_point asked = ahead->received( fetch_blockchain_item_ids_message_type )[ id_requests ].time;
    BOOST_CHECK_GE( ( asked - head_advanced ).count(), fc::seconds( GRAPHENE_NET_STALLED_HEAD_RESYNC_SEC - 1 ).count() );

    fc::usleep( quiet_period );
    const size_t id_requests_in_sync = ahead->count( fetch_blockchain_item_ids_message_type );
    fc::usleep( fc::seconds( GRAPHENE_NET_STALLED_HEAD_RESYNC_SEC + 5 ) );
    BOOST_CHECK_EQUAL( ahead->count( fetch_blockchain_item_ids_message_type ), id_requests_in_sync );
    require_kept( *ahead );
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

/// D15: a peer advertising as many transaction ids as its inventory takes in a minute
/// (GRAPHENE_NET_MAX_TRANSACTION_INVENTORY_PER_PEER), all sharing their first 8 bytes, doesn't hold up the p2p thread:
/// a block another peer advertises right after is asked for within a block interval.  An inventory message with more
/// ids than that ends the connection (the node no longer lists the peer).  The flooder withholds everything it is asked for, so the ignored-request
/// timeout is raised out of the way.
BOOST_AUTO_TEST_CASE( colliding_transaction_ids_test )
{
  start_node( R"("active_ignored_request_timeout_microseconds":)" + std::to_string( fc::seconds( 120 ).count() ), true );
  run( [&]()
  {
    fc::sleep_until( get_genesis_time() + HIVE_BLOCK_INTERVAL );
    wait_for_block_num( 1 );
    const auto colliding_ids = []( uint32_t count )
    {
      std::vector< item_hash_t > ids;
      for( uint32_t n = 0; n < count; ++n )
      {
        item_hash_t id;
        id._hash[ 0 ] = 0x12345678;
        id._hash[ 1 ] = 0x9abcdef0;
        id._hash[ 2 ] = n;
        id._hash[ 3 ] = ~n;
        ids.push_back( id );
      }
      return ids;
    };

    auto flooder = make_peer( "flooding peer" );
    connect_active( *flooder );
    auto block_source = make_peer( "block source" );
    connect_active( *block_source );
    flooder->advertise( colliding_ids( GRAPHENE_NET_MAX_TRANSACTION_INVENTORY_PER_PEER ) );
    const block_id_type block = fake_block_id( get_block_num() + 1, "advertised block" );
    const fc::time_point advertised = fc::time_point::now();
    block_source->advertise( block_message_type, { block } );
    BOOST_REQUIRE( wait_until( [&]() { return times_requested( *block_source, block ) != 0; }, reply_timeout ) );
    const fc::microseconds latency = first_request_time( *block_source, block ) - advertised;
    BOOST_TEST_MESSAGE( "block requested " << latency.count() / 1000 << " ms after it was advertised" );
    BOOST_CHECK_LT( latency.count(), fc::seconds( HIVE_BLOCK_INTERVAL ).count() );
    BOOST_CHECK( flooder->wait_for( fetch_items_message_type, reply_timeout ) );
    BOOST_CHECK( !flooder->closed() );

    // a test_peer's own send queue doesn't take a message that large, so it goes out on a raw connection
    raw_connection oversized( peer_thread );
    oversized.connect_secure( node_endpoint() );
    oversized.handshake( db->get_chain_id() );
    const fc::ip::endpoint oversized_endpoint = oversized.local_endpoint();
    const auto node_lists_oversized = [&]()
    {
      const std::vector< hive::plugins::p2p::api_peer_status > peers = p2p().get_connected_peers();
      return std::any_of( peers.begin(), peers.end(), [&]( const hive::plugins::p2p::api_peer_status& p ) { return p.host == oversized_endpoint; } );
    };
    BOOST_REQUIRE( wait_until( node_lists_oversized, reply_timeout ) );
    oversized.send( item_ids_inventory_message( trx_message_type, colliding_ids( GRAPHENE_NET_MAX_ITEM_IDS_PER_INVENTORY_MESSAGE + 1 ) ) );
    BOOST_CHECK( wait_until( [&]() { return !node_lists_oversized(); }, reply_timeout ) );
    BOOST_CHECK( node_lists( *flooder ) );
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

/// D17: while the pending pool holds more than a third of max-mempool-size the node asks its peers for no more than
/// GRAPHENE_NET_MIN_TRANSACTIONS_PER_GATED_FETCH transactions per GRAPHENE_NET_TRANSACTION_FETCH_GATE_RECHECK_MS, and for
/// blocks as always; so a fast-confirm one peer advertises while another floods it is still fetched and relayed to the
/// peers in sync.  The flooder's own window doesn't bound it: it has delivered small transactions, so one request may
/// ask it for its whole flood, and it delivers each at once, so none stays outstanding.  (One answering that it doesn't
/// have transactions it advertised would be demoted for that, see unavailable_advertised_items_test.)
BOOST_AUTO_TEST_CASE( transaction_fetch_gate_test )
{
  const uint64_t max_mempool_size = 6 * 1024;
  const uint64_t threshold = transaction_fetch_gate_threshold( max_mempool_size );
  const size_t flood_size = 50;
  start_node( "", true, { config_line_t( { "max-mempool-size", { std::to_string( max_mempool_size ) } } ) } );
  run( [&]()
  {
    fc::sleep_until( get_genesis_time() + HIVE_BLOCK_INTERVAL );
    wait_for_block_num( 1 );

    auto flooder = make_peer( "flooding peer" );
    connect_active( *flooder );
    auto source = make_peer( "fast-confirm source" );
    connect_active( *source );
    auto block_source = make_peer( "block source" );
    connect_active( *block_source );
    auto observer = make_peer( "observer" );
    connect_in_sync( *observer );

    // the flooder delivers small transactions until the node's estimate of its transaction size, followed here,
    // lets one request ask it for the whole flood
    transaction_size_estimate flooder_estimate;
    std::vector< full_transaction_ptr > warm_up;
    while( flooder_estimate.transactions_to_request( 0 ) < flood_size )
    {
      warm_up.push_back( build_transfer( HIVE_INIT_MINER_NAME, HIVE_NULL_ACCOUNT, ASSET( "0.001 TESTS" ),
                                         "warm-up " + std::to_string( warm_up.size() ), get_default_expiration() ) );
      flooder_estimate.record( warm_up.back()->get_transaction_size() );
    }
    std::vector< item_hash_t > warm_up_hashes;
    for( const full_transaction_ptr& transaction : warm_up )
      warm_up_hashes.push_back( transaction->get_legacy_transaction_message_hash() );
    flooder->provide( warm_up );
    flooder->advertise( warm_up_hashes );
    BOOST_REQUIRE( wait_until( [&]()
    {
      return std::all_of( warm_up_hashes.begin(), warm_up_hashes.end(), [&]( const item_hash_t& hash ) { return observer->was_advertised( hash ); } );
    }, reply_timeout ) );

    // a block would include what takes the pool over the threshold; one that was under way when production stopped
    // can still come, and then the checks are made again
    stop_block_production();
    bool checked_while_gated = false;
    fc::time_point gated_since;
    std::vector< item_hash_t > flood;
    for( int attempt = 0; attempt < 3 && !checked_while_gated; ++attempt )
    {
      const uint32_t head = get_block_num();
      gated_since = fc::time_point::now();
      uint64_t filled = 0;
      for( int i = 0; filled <= threshold; ++i )
      {
        const full_transaction_ptr filler = build_transfer( HIVE_INIT_MINER_NAME, HIVE_NULL_ACCOUNT, ASSET( "0.001 TESTS" ),
                                                            "filler " + std::to_string( attempt ) + "/" + std::to_string( i ),
                                                            get_default_expiration() );
        schedule_transaction( filler );
        filled += filler->get_transaction_size();
      }
      wait_for_pool_over( threshold, head );
      if( get_block_num() != head )
        continue;

      flood.clear();
      std::vector< full_transaction_ptr > flooded_transactions;
      for( size_t i = 0; i < flood_size; ++i )
      {
        flooded_transactions.push_back( build_transfer( HIVE_INIT_MINER_NAME, HIVE_NULL_ACCOUNT, ASSET( "0.001 TESTS" ),
                                                        "flooded " + std::to_string( attempt ) + "/" + std::to_string( i ),
                                                        get_default_expiration() ) );
        flood.push_back( flooded_transactions.back()->get_legacy_transaction_message_hash() );
      }
      flooder->provide( flooded_transactions );
      flooder->advertise( flood );

      witness_block_approve_operation approve;
      approve.witness = HIVE_INIT_MINER_NAME;
      approve.block_id = head_block_id();
      const full_transaction_ptr fast_confirm = build_transaction( approve, get_default_expiration() );
      const item_hash_t hash = fast_confirm->get_legacy_transaction_message_hash();
      source->provide( { fast_confirm } );
      source->advertise( { hash } );
      const block_id_type next_block = fake_block_id( head + 1, "advertised while gated " + std::to_string( attempt ) );
      block_source->advertise( block_message_type, { next_block } );

      const bool fetched = wait_until( [&]()
      {
        return ( source->was_requested( hash ) && times_requested( *block_source, next_block ) != 0 && observer->was_advertised( hash ) &&
                 std::all_of( flood.begin(), flood.end(), [&]( const item_hash_t& flooded ) { return flooder->was_requested( flooded ); } ) ) ||
               get_block_num() != head;
      }, reply_timeout );
      BOOST_REQUIRE( fetched );
      checked_while_gated = get_block_num() == head;
    }
    BOOST_REQUIRE_MESSAGE( checked_while_gated, "a block came before the checks of the gated pool ended, every time" );
    BOOST_REQUIRE_GT( get_chain_plugin().get_published_pending_transactions_size(), threshold );
    // the flood the flooder was asked for while the pool was over the threshold came no more than the minimum per
    // recheck interval (less what delivering a request may have taken), so in at least three passes
    const fc::time_point checked_at = fc::time_point::now();
    std::vector< std::pair< fc::time_point, size_t > > requests;
    for( const received_message& r : flooder->received( fetch_items_message_type ) )
      if( r.time >= gated_since && r.time <= checked_at )
      {
        const fetch_items_message request = r.msg.as< fetch_items_message >();
        BOOST_CHECK_LE( request.items_to_fetch.size(), size_t( GRAPHENE_NET_MIN_TRANSACTIONS_PER_GATED_FETCH ) );
        const size_t flooded = std::count_if( request.items_to_fetch.begin(), request.items_to_fetch.end(),
                                              [&]( const item_hash_t& item ) { return std::find( flood.begin(), flood.end(), item ) != flood.end(); } );
        if( flooded != 0 )
          requests.emplace_back( r.time, flooded );
      }
    BOOST_REQUIRE( !requests.empty() );
    const fc::microseconds pass_spacing = fc::milliseconds( GRAPHENE_NET_TRANSACTION_FETCH_GATE_RECHECK_MS * 4 / 5 );
    size_t requested_while_gated = 0;
    for( const auto& [ requested_at, requested ] : requests )
    {
      size_t requested_in_pass = 0;
      for( const auto& [ earlier_at, earlier ] : requests )
        if( earlier_at <= requested_at && requested_at - earlier_at < pass_spacing )
          requested_in_pass += earlier;
      BOOST_CHECK_LE( requested_in_pass, size_t( GRAPHENE_NET_MIN_TRANSACTIONS_PER_GATED_FETCH ) );
      requested_while_gated += requested;
    }
    BOOST_CHECK_EQUAL( requested_while_gated, flood_size );
    BOOST_CHECK_GE( ( requests.back().first - requests.front().first ).count(),
                    pass_spacing.count() * int64_t( ( flood_size - 1 ) / GRAPHENE_NET_MIN_TRANSACTIONS_PER_GATED_FETCH ) );
    require_kept( *source );
  } );
}

/// D17: while the gate is closed, a fast-confirm a peer advertises behind a backlog of flooded transactions is in the
/// first request the node sends that peer: the gate's minimum takes the newest candidates first.  The peer withholds
/// the backlog, so a node that took the oldest first (or did not gate) would fill the peer's window with those and
/// never ask for the fast-confirm
BOOST_AUTO_TEST_CASE( fast_confirm_behind_a_backlog_test )
{
  const uint64_t max_mempool_size = 6 * 1024;
  const uint64_t threshold = transaction_fetch_gate_threshold( max_mempool_size );
  start_node( "", true, { config_line_t( { "max-mempool-size", { std::to_string( max_mempool_size ) } } ) } );
  run( [&]()
  {
    fc::sleep_until( get_genesis_time() + HIVE_BLOCK_INTERVAL );
    wait_for_block_num( 1 );

    // as in transaction_fetch_gate_test, the checks are made again after a block that was under way when production stopped
    stop_block_production();
    bool checked_while_gated = false;
    for( int attempt = 0; attempt < 3 && !checked_while_gated; ++attempt )
    {
      const uint32_t head = get_block_num();
      uint64_t filled = 0;
      for( int i = 0; filled <= threshold; ++i )
      {
        const full_transaction_ptr filler = build_transfer( HIVE_INIT_MINER_NAME, HIVE_NULL_ACCOUNT, ASSET( "0.001 TESTS" ),
                                                            "backlog filler " + std::to_string( attempt ) + "/" + std::to_string( i ),
                                                            get_default_expiration() );
        schedule_transaction( filler );
        filled += filler->get_transaction_size();
      }
      wait_for_pool_over( threshold, head );
      if( get_block_num() != head )
        continue;

      auto relay = make_peer( "relay " + std::to_string( attempt ) );
      connect_active( *relay );
      witness_block_approve_operation approve;
      approve.witness = HIVE_INIT_MINER_NAME;
      approve.block_id = head_block_id();
      const full_transaction_ptr fast_confirm = build_transaction( approve, get_default_expiration() );
      const item_hash_t hash = fast_confirm->get_legacy_transaction_message_hash();
      relay->provide( { fast_confirm } );
      // one inventory: the backlog, then the fast-confirm
      std::vector< item_hash_t > inventory;
      for( int i = 0; i < 200; ++i )
        inventory.push_back( fc::ripemd160::hash( "backlog " + std::to_string( attempt ) + "/" + std::to_string( i ) ) );
      inventory.push_back( hash );
      relay->advertise( inventory );

      BOOST_REQUIRE( wait_until( [&]() { return relay->was_requested( hash ) || get_block_num() != head; }, reply_timeout ) );
      checked_while_gated = get_block_num() == head;
      if( !checked_while_gated )
        continue;
      BOOST_REQUIRE_GT( get_chain_plugin().get_published_pending_transactions_size(), threshold );
      std::vector< fetch_items_message > transaction_requests;
      for( const fetch_items_message& request : relay->received_as< fetch_items_message >() )
        if( request.item_type == trx_message_type )
          transaction_requests.push_back( request );
      BOOST_REQUIRE( !transaction_requests.empty() );
      const std::vector< item_hash_t >& first = transaction_requests.front().items_to_fetch;
      BOOST_CHECK( std::find( first.begin(), first.end(), hash ) != first.end() );
      BOOST_CHECK_LE( first.size(), size_t( GRAPHENE_NET_MIN_TRANSACTIONS_PER_GATED_FETCH ) );
      require_kept( *relay );
    }
    BOOST_REQUIRE_MESSAGE( checked_while_gated, "a block came before the checks of the gated pool ended, every time" );
  } );
}

/// D17: below the threshold the node asks for no more transactions than fit under it with those already on their
/// way: a peer that has sent none yet is expected to send ones of the mean size of those peers sent that validated,
/// HIVE_MAX_TRANSACTION_SIZE while there are none (as here), so with a threshold smaller than that the headroom is used
/// up by the first transaction requested.  What it expects on their way
/// doesn't stop the empty pool fetching: the gate's minimum asks for the rest, but no sooner than
/// GRAPHENE_NET_TRANSACTION_FETCH_GATE_RECHECK_MS after the last pass that asked for some of its minimum, however
/// soon more is advertised
BOOST_AUTO_TEST_CASE( transaction_fetch_headroom_test )
{
  const uint64_t max_mempool_size = 6 * 1024;
  BOOST_REQUIRE_LT( transaction_fetch_gate_threshold( max_mempool_size ), uint64_t( HIVE_MAX_TRANSACTION_SIZE ) );
  start_node( "", true, { config_line_t( { "max-mempool-size", { std::to_string( max_mempool_size ) } } ) } );
  run( [&]()
  {
    fc::sleep_until( get_genesis_time() + HIVE_BLOCK_INTERVAL );
    wait_for_block_num( 1 );

    auto source = make_peer( "transaction source" );
    connect_active( *source );
    const auto withheld = []( int first, int count )
    {
      std::vector< item_hash_t > hashes;
      for( int i = first; i < first + count; ++i )
        hashes.push_back( fc::ripemd160::hash( "withheld transaction " + std::to_string( i ) ) );
      return hashes;
    };
    const auto all_requested = [&]( const std::vector< item_hash_t >& hashes )
    {
      return std::all_of( hashes.begin(), hashes.end(), [&]( const item_hash_t& hash ) { return source->was_requested( hash ); } );
    };
    const std::vector< item_hash_t > first = withheld( 0, 3 );
    source->advertise( first );
    BOOST_REQUIRE( wait_until( [&]() { return all_requested( first ); }, reply_timeout ) );
    const std::vector< item_hash_t > second = withheld( 3, 3 );
    source->advertise( second );
    BOOST_REQUIRE( wait_until( [&]() { return all_requested( second ); }, reply_timeout ) );
    BOOST_CHECK_EQUAL( get_chain_plugin().get_published_pending_transactions_size(), 0u );

    std::vector< fc::time_point > request_times;
    for( const received_message& r : source->received( fetch_items_message_type ) )
      if( r.msg.as< fetch_items_message >().item_type == trx_message_type )
        request_times.push_back( r.time );
    BOOST_REQUIRE_EQUAL( request_times.size(), 2u );
    // the interval less what delivering the first request may have taken
    BOOST_CHECK_GE( ( request_times[ 1 ] - request_times[ 0 ] ).count(),
                    fc::milliseconds( GRAPHENE_NET_TRANSACTION_FETCH_GATE_RECHECK_MS * 4 / 5 ).count() );
    for( const item_hash_t& hash : first )
      BOOST_CHECK_EQUAL( times_requested( *source, hash ), 1u );
    for( const item_hash_t& hash : second )
      BOOST_CHECK_EQUAL( times_requested( *source, hash ), 1u );
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

/// D17 (T): a peer that answers item_not_available for transactions it advertised is demoted: for
/// GRAPHENE_NET_UNAVAILABLE_ITEM_DEMOTION_SEC (10 s) it is asked for no transactions, however many more it advertises,
/// and the GRAPHENE_NET_MAX_UNAVAILABLE_ITEM_STRIKES-th (3rd) time it answers so it is disconnected. So it holds the
/// node's requests, and the fetch gate's headroom, for nothing only a few times. A peer that delivers what it
/// advertises is asked for each transaction meanwhile
BOOST_AUTO_TEST_CASE( unavailable_advertised_items_test )
{
  start_node( "", true );
  run( [&]()
  {
    fc::sleep_until( get_genesis_time() + HIVE_BLOCK_INTERVAL );
    wait_for_block_num( 1 );

    auto empty_handed = make_peer( "peer advertising transactions it doesn't have" );
    empty_handed->stop_withholding();
    connect_active( *empty_handed );
    auto delivering = make_peer( "peer delivering what it advertises" );
    connect_active( *delivering );

    const fc::microseconds demotion = fc::seconds( GRAPHENE_NET_UNAVAILABLE_ITEM_DEMOTION_SEC );
    const fc::time_point deadline = fc::time_point::now() + fc::seconds( GRAPHENE_NET_UNAVAILABLE_ITEM_DEMOTION_SEC * GRAPHENE_NET_MAX_UNAVAILABLE_ITEM_STRIKES ) +
                                    reply_timeout;
    std::vector< item_hash_t > delivered;
    // both advertise new transactions every half second
    for( int round = 0; !empty_handed->closed() && fc::time_point::now() < deadline; ++round )
    {
      std::vector< item_hash_t > not_had;
      for( int i = 0; i < 3; ++i )
        not_had.push_back( fc::ripemd160::hash( "not had " + std::to_string( round ) + "/" + std::to_string( i ) ) );
      empty_handed->advertise( not_had );
      const full_transaction_ptr transaction = build_transfer( HIVE_INIT_MINER_NAME, HIVE_NULL_ACCOUNT, ASSET( "0.001 TESTS" ),
                                                              "delivered " + std::to_string( round ), get_default_expiration() );
      delivering->provide( { transaction } );
      delivering->advertise( { transaction->get_legacy_transaction_message_hash() } );
      delivered.push_back( transaction->get_legacy_transaction_message_hash() );
      fc::usleep( fc::milliseconds( 500 ) );
    }
    require_closed_with( *empty_handed, "advertising items you don't have" );

    // its transaction requests came in one burst per strike (each answered at once), a demotion apart
    std::vector< fc::time_point > bursts;
    for( const received_message& r : empty_handed->received( fetch_items_message_type ) )
      if( r.msg.as< fetch_items_message >().item_type == trx_message_type && ( bursts.empty() || r.time - bursts.back() > fc::seconds( 1 ) ) )
        bursts.push_back( r.time );
    BOOST_CHECK_EQUAL( bursts.size(), size_t( GRAPHENE_NET_MAX_UNAVAILABLE_ITEM_STRIKES ) );
    for( size_t k = 1; k < bursts.size(); ++k )
      BOOST_CHECK_MESSAGE( bursts[ k ] - bursts[ k - 1 ] >= demotion - fc::milliseconds( 100 ),
                           "asked again " << ( bursts[ k ] - bursts[ k - 1 ] ).count() / 1000 << " ms after strike " << k );
    BOOST_REQUIRE( wait_until( [&]()
    {
      return std::all_of( delivered.begin(), delivered.end(), [&]( const item_hash_t& hash ) { return delivering->was_requested( hash ); } );
    }, reply_timeout ) );
    require_kept( *delivering );
  } );
}

/// D17: peers that advertise transactions and never deliver them can't use up the fetch gate's headroom for free: a
/// request to a peer that has sent no transaction counts as one of the mean size of those peers sent that validated,
/// not of HIVE_MAX_TRANSACTION_SIZE. With 66 such peers each holding a full window of requests (8, by their own
/// estimate: worth more than the threshold at the largest size), a peer that delivers is still asked for its whole
/// new inventory in one request, not for the gate's minimum per recheck
BOOST_AUTO_TEST_CASE( withholding_peers_headroom_test )
{
  const uint64_t max_mempool_size = 12 * 1024 * 1024;
  const uint64_t threshold = transaction_fetch_gate_threshold( max_mempool_size );
  const size_t withholding_count = 66;
  const size_t window = GRAPHENE_NET_MAX_TRANSACTION_BYTES_PER_FETCH / HIVE_MAX_TRANSACTION_SIZE;
  const size_t batch_size = 100;
  BOOST_REQUIRE_GT( uint64_t( withholding_count * window ) * HIVE_MAX_TRANSACTION_SIZE, threshold );
  start_node( "", true, { config_line_t( { "max-mempool-size", { std::to_string( max_mempool_size ) } } ) } );
  run( [&]()
  {
    fc::sleep_until( get_genesis_time() + HIVE_BLOCK_INTERVAL );
    wait_for_block_num( 1 );

    auto source = make_peer( "delivering peer" );
    connect_active( *source );
    auto observer = make_peer( "observer" );
    connect_in_sync( *observer );
    const auto transfers = [&]( const std::string& memo, size_t count )
    {
      std::vector< full_transaction_ptr > transactions;
      for( size_t i = 0; i < count; ++i )
        transactions.push_back( build_transfer( HIVE_INIT_MINER_NAME, HIVE_NULL_ACCOUNT, ASSET( "0.001 TESTS" ),
                                                memo + " " + std::to_string( i ), get_default_expiration() ) );
      return transactions;
    };
    const auto hashes_of = []( const std::vector< full_transaction_ptr >& transactions )
    {
      std::vector< item_hash_t > hashes;
      for( const full_transaction_ptr& transaction : transactions )
        hashes.push_back( transaction->get_legacy_transaction_message_hash() );
      return hashes;
    };

    // the node validates transactions the source delivers, which sets the size it expects of a new peer's
    const std::vector< full_transaction_ptr > warm_up = transfers( "warm-up", 100 );
    const std::vector< item_hash_t > warm_up_hashes = hashes_of( warm_up );
    source->provide( warm_up );
    source->advertise( warm_up_hashes );
    BOOST_REQUIRE( wait_until( [&]()
    {
      return std::all_of( warm_up_hashes.begin(), warm_up_hashes.end(), [&]( const item_hash_t& hash ) { return observer->was_advertised( hash ); } );
    }, reply_timeout ) );

    std::vector< std::unique_ptr< test_peer > > withholding;
    std::vector< std::vector< item_hash_t > > withheld( withholding_count );
    for( size_t i = 0; i < withholding_count; ++i )
    {
      withholding.push_back( make_peer( "withholding peer " + std::to_string( i ) ) );
      connect_active( *withholding.back() );
      for( size_t j = 0; j < window; ++j )
        withheld[ i ].push_back( fc::ripemd160::hash( "withheld " + std::to_string( i ) + "/" + std::to_string( j ) ) );
    }
    for( size_t i = 0; i < withholding_count; ++i )
      withholding[ i ]->advertise( withheld[ i ] );
    BOOST_REQUIRE( wait_until( [&]()
    {
      for( size_t i = 0; i < withholding_count; ++i )
        if( !std::all_of( withheld[ i ].begin(), withheld[ i ].end(), [&]( const item_hash_t& hash ) { return withholding[ i ]->was_requested( hash ); } ) )
          return false;
      return true;
    }, reply_timeout ) );

    // while their requests are outstanding (until active_ignored_request_timeout, 6 s), the source advertises more
    const std::vector< full_transaction_ptr > batch = transfers( "batch", batch_size );
    const std::vector< item_hash_t > batch_hashes = hashes_of( batch );
    source->provide( batch );
    source->advertise( batch_hashes );
    BOOST_REQUIRE( wait_until( [&]()
    {
      return std::all_of( batch_hashes.begin(), batch_hashes.end(), [&]( const item_hash_t& hash ) { return source->was_requested( hash ); } );
    }, reply_timeout ) );
    for( const fetch_items_message& request : source->received_as< fetch_items_message >() )
      if( std::find( request.items_to_fetch.begin(), request.items_to_fetch.end(), batch_hashes.front() ) != request.items_to_fetch.end() )
      {
        BOOST_CHECK_GT( request.items_to_fetch.size(), size_t( GRAPHENE_NET_MIN_TRANSACTIONS_PER_GATED_FETCH ) );
        BOOST_CHECK_EQUAL( request.items_to_fetch.size(), batch_size );
        break;
      }
  } );
}

/// Category 2 (fallback): peers that advertise blocks and withhold them delay a block about
/// GRAPHENE_NET_BLOCK_REREQUEST_TIMEOUT_MS (1 s) in all. A withholding peer that advertises a block first is asked
/// for it; once it has kept the node waiting that long, the node asks another advertiser, and of the several idle ones
/// it asks the one that delivered it a block before, not another withholding peer that never did. The default
/// active_ignored_request_timeout (6 s) ends the first one's connection only later, when its request times out
BOOST_AUTO_TEST_CASE( withholding_block_advertisers_test )
{
  const std::vector< block_ptr > chain = generate_chain( 7 );
  start_node();
  run( [&]()
  {
    sync_node( chain, 5 );
    const auto id = [&]( uint32_t num ) { return item_hash_t( chain[ num - 1 ]->get_block_id() ); };
    auto proven = connect_live_peer( "peer that delivered a block", chain );
    proven->advertise( block_message_type, { id( 6 ) } );
    BOOST_REQUIRE_MESSAGE( head_reaches( 6 ), "head is " << get_block_num() );

    std::vector< std::unique_ptr< test_peer > > withholding;
    for( int i = 0; i < 3; ++i )
    {
      withholding.push_back( make_peer( "withholding advertiser " + std::to_string( i ) ) );
      withholding.back()->ignore( fetch_items_message_type );
      connect_active( *withholding.back() );
    }
    withholding.front()->advertise( block_message_type, { id( 7 ) } );
    BOOST_REQUIRE( wait_until( [&]() { return times_requested( *withholding.front(), id( 7 ) ) != 0; }, reply_timeout ) );
    for( size_t i = 1; i < withholding.size(); ++i )
      withholding[ i ]->advertise( block_message_type, { id( 7 ) } );
    proven->advertise( block_message_type, { id( 7 ) } );
    BOOST_REQUIRE_MESSAGE( head_reaches( 7 ), "head is " << get_block_num() );
    const fc::microseconds asked_after = first_request_time( *proven, id( 7 ) ) - first_request_time( *withholding.front(), id( 7 ) );
    BOOST_CHECK_GE( asked_after.count(), fc::milliseconds( GRAPHENE_NET_BLOCK_REREQUEST_TIMEOUT_MS - 100 ).count() );
    BOOST_CHECK_MESSAGE( asked_after < fc::milliseconds( GRAPHENE_NET_BLOCK_REREQUEST_TIMEOUT_MS + 1000 ),
                         "the peer that delivered before was asked " << asked_after.count() / 1000 << " ms after the first one" );
    for( size_t i = 1; i < withholding.size(); ++i )
      BOOST_CHECK_EQUAL( times_requested( *withholding[ i ], id( 7 ) ), 0u );
    BOOST_CHECK( !withholding.front()->closed() );
    const fc::microseconds timeout = fc::seconds( GRAPHENE_NET_ACTIVE_IGNORED_REQUEST_TIMEOUT_SECONDS );
    BOOST_REQUIRE_MESSAGE( withholding.front()->wait_closed( timeout + fc::seconds( 3 ) ), "the node kept the withholding peer" );
    const fc::microseconds closed_after = fc::time_point::now() - first_request_time( *withholding.front(), id( 7 ) );
    BOOST_CHECK_GE( closed_after.count(), ( timeout - fc::milliseconds( 100 ) ).count() );
    for( size_t i = 1; i < withholding.size(); ++i )
      BOOST_CHECK( !withholding[ i ]->closed() );
    require_kept( *proven );
  } );
}

/// A block that takes a slow peer about 3 s to deliver (a large block on a slow link) costs at most two more copies:
/// it is asked of another advertiser at the first request's deadline, 1 s, and of a third at the second request's,
/// 2 s later, if the slow copy hasn't come by then. With those not delivering either, the node waits for a copy. The
/// last advertiser is never asked
BOOST_AUTO_TEST_CASE( slow_block_rerequested_at_most_twice_test )
{
  const std::vector< block_ptr > chain = generate_chain( 7 );
  const fc::microseconds transfer_time = fc::seconds( 3 );
  start_node();
  run( [&]()
  {
    sync_node( chain, 5 );
    const auto id = [&]( uint32_t num ) { return item_hash_t( chain[ num - 1 ]->get_block_id() ); };
    auto slow = connect_live_peer( "slow peer", chain );
    slow->advertise( block_message_type, { id( 6 ) } );
    BOOST_REQUIRE_MESSAGE( head_reaches( 6 ), "head is " << get_block_num() );

    std::vector< std::unique_ptr< test_peer > > others;
    for( int i = 0; i < 3; ++i )
    {
      others.push_back( make_peer( "other advertiser " + std::to_string( i ) ) );
      connect_active( *others.back() );
    }
    slow->answer_after( fetch_items_message_type, transfer_time );
    slow->advertise( block_message_type, { id( 7 ) } );
    BOOST_REQUIRE( wait_until( [&]() { return times_requested( *slow, id( 7 ) ) != 0; }, reply_timeout ) );
    for( const auto& other : others )
      other->advertise( block_message_type, { id( 7 ) } );
    BOOST_REQUIRE_MESSAGE( head_reaches( 7 ), "head is " << get_block_num() );
    size_t rerequests = 0;
    size_t never_asked = 0;
    for( const auto& other : others )
    {
      rerequests += times_requested( *other, id( 7 ) );
      never_asked += times_requested( *other, id( 7 ) ) == 0;
    }
    BOOST_CHECK_GE( rerequests, 1u );
    BOOST_CHECK_LE( rerequests, 2u );
    BOOST_CHECK_GE( never_asked, 1u );
    BOOST_CHECK_EQUAL( times_requested( *slow, id( 7 ) ), 1u );
    BOOST_CHECK_GE( ( fc::time_point::now() - first_request_time( *slow, id( 7 ) ) ).count(), transfer_time.count() );
    BOOST_CHECK_EQUAL( peer_info( *slow )[ "withholding_strikes" ].as_uint64(), 0u );
    require_kept( *slow );
  } );
}

/// Two peers that withhold a block and deliver it 4.5 s after their requests, before they would time out
/// (active_ignored_request_timeout, 6 s), asked for it first and second, hold it only for their deadlines, 1 s and 2 s:
/// the advertiser asked third, 3 s after the first one, delivers it. Their copies come past their deadlines, so they
/// restore nothing: both stay deprioritized, but a late copy is no strike, as it can't be told from a slow one
BOOST_AUTO_TEST_CASE( late_withholders_rerequested_not_struck_test )
{
  const std::vector< block_ptr > chain = generate_chain( 8 );
  const fc::microseconds withheld_for = fc::milliseconds( 4500 );
  start_node();
  run( [&]()
  {
    sync_node( chain, 5 );
    const auto id = [&]( uint32_t num ) { return item_hash_t( chain[ num - 1 ]->get_block_id() ); };
    // each delivers a block, so both are asked before the honest peer, which never did
    auto first = connect_live_peer( "first withholder", chain );
    first->advertise( block_message_type, { id( 6 ) } );
    BOOST_REQUIRE_MESSAGE( head_reaches( 6 ), "head is " << get_block_num() );
    auto second = connect_live_peer( "second withholder", chain );
    second->advertise( block_message_type, { id( 7 ) } );
    BOOST_REQUIRE_MESSAGE( head_reaches( 7 ), "head is " << get_block_num() );
    auto honest = connect_live_peer( "honest advertiser", chain );

    first->answer_after( fetch_items_message_type, withheld_for );
    second->answer_after( fetch_items_message_type, withheld_for );
    first->advertise( block_message_type, { id( 8 ) } );
    BOOST_REQUIRE( wait_until( [&]() { return times_requested( *first, id( 8 ) ) != 0; }, reply_timeout ) );
    second->advertise( block_message_type, { id( 8 ) } );
    honest->advertise( block_message_type, { id( 8 ) } );
    BOOST_REQUIRE_MESSAGE( head_reaches( 8 ), "head is " << get_block_num() );

    const fc::time_point first_asked = first_request_time( *first, id( 8 ) );
    const fc::microseconds second_asked_after = first_request_time( *second, id( 8 ) ) - first_asked;
    BOOST_CHECK_GE( second_asked_after.count(), fc::milliseconds( 900 ).count() );
    BOOST_CHECK_LT( second_asked_after.count(), fc::milliseconds( 1900 ).count() );
    const fc::microseconds honest_asked_after = first_request_time( *honest, id( 8 ) ) - first_asked;
    BOOST_CHECK_GE( honest_asked_after.count(), fc::milliseconds( 2900 ).count() );
    BOOST_CHECK_MESSAGE( honest_asked_after < fc::milliseconds( 4000 ),
                         "the honest peer was asked " << honest_asked_after.count() / 1000 << " ms after the first withholder" );
    BOOST_CHECK_EQUAL( times_requested( *honest, id( 8 ) ), 1u );

    // both late copies have come
    fc::usleep( first_request_time( *second, id( 8 ) ) + withheld_for + fc::milliseconds( 500 ) - fc::time_point::now() );
    for( const test_peer* withholder : { first.get(), second.get() } )
    {
      const fc::variant_object info = peer_info( *withholder );
      BOOST_CHECK( info[ "block_fetching_deprioritized" ].as_bool() );
      BOOST_CHECK_EQUAL( info[ "withholding_strikes" ].as_uint64(), 0u );
      require_kept( *withholder );
    }
    const fc::variant_object honest_info = peer_info( *honest );
    BOOST_CHECK( !honest_info[ "block_fetching_deprioritized" ].as_bool() );
    BOOST_CHECK_EQUAL( honest_info[ "withholding_strikes" ].as_uint64(), 0u );
  } );
}

/// An honest peer on the node's own slow download link (its bytes coming steadily for about 8 s, as a large block's do
/// on a slow link) asked for a block first: the node asks another advertiser at the first request's deadline, 1 s,
/// as for any late peer, and has the block from it then, well before the slow copy. The slow copy keeps coming past
/// the request timeout (active_ignored_request_timeout, 6 s), which a download still arriving extends up to the ceiling
/// (GRAPHENE_NET_MAX_BLOCK_REQUEST_TIMEOUTS_WHILE_DOWNLOADING, 12 s): when it completes the slow peer is only
/// deprioritized for blocks, neither struck nor disconnected. The advertiser after the one that delivered is never
/// asked
BOOST_AUTO_TEST_CASE( slow_link_peer_rerequested_not_struck_test )
{
  const std::vector< block_ptr > chain = generate_chain( 7 );
  const fc::microseconds download_time = fc::seconds( 8 );
  start_node();
  run( [&]()
  {
    sync_node( chain, 5 );
    const auto id = [&]( uint32_t num ) { return item_hash_t( chain[ num - 1 ]->get_block_id() ); };
    throttling_relay link( peer_thread, node_endpoint() );
    auto slow = connect_proven_peer_through( link, "peer on the node's slow link", chain, 6 );

    std::vector< std::unique_ptr< test_peer > > others;
    for( int i = 0; i < 2; ++i )
      others.push_back( connect_live_peer( "other advertiser " + std::to_string( i ), chain ) );
    stream_through( link, chain[ 6 ], download_time );
    slow->advertise( block_message_type, { id( 7 ) } );
    BOOST_REQUIRE( wait_until( [&]() { return times_requested( *slow, id( 7 ) ) != 0; }, reply_timeout ) );
    for( const auto& other : others )
      other->advertise( block_message_type, { id( 7 ) } );
    BOOST_REQUIRE_MESSAGE( head_reaches( 7 ), "head is " << get_block_num() );

    const fc::time_point asked = first_request_time( *slow, id( 7 ) );
    const fc::microseconds took = fc::time_point::now() - asked;
    BOOST_CHECK_MESSAGE( took < fc::milliseconds( GRAPHENE_NET_BLOCK_REREQUEST_TIMEOUT_MS + 1500 ),
                         "the block arrived " << took.count() / 1000 << " ms after the first request" );
    BOOST_CHECK_EQUAL( times_requested( *others[ 0 ], id( 7 ) ) + times_requested( *others[ 1 ], id( 7 ) ), 1u );

    // its copy completes about download_time after the request, past the request timeout and short of the ceiling
    BOOST_REQUIRE_LT( download_time.count(), fc::seconds( GRAPHENE_NET_ACTIVE_IGNORED_REQUEST_TIMEOUT_SECONDS *
                                                          GRAPHENE_NET_MAX_BLOCK_REQUEST_TIMEOUTS_WHILE_DOWNLOADING ).count() - 2000000 );
    fc::usleep( asked + download_time + fc::milliseconds( 1500 ) - fc::time_point::now() );
    const fc::variant_object info = peer_info( link.node_side_endpoint() );
    BOOST_CHECK( info[ "block_fetching_deprioritized" ].as_bool() );
    BOOST_CHECK_EQUAL( info[ "withholding_strikes" ].as_uint64(), 0u );
    BOOST_CHECK_EQUAL( times_requested( *slow, id( 7 ) ), 1u );
    require_kept( *slow );
    for( const auto& other : others )
      require_kept( *other );
  } );
}

/// Three peers that withhold a block silently, asked for it first, hold it for their deadlines in a row, 1 + 2 + 4 s:
/// then three requests are outstanding, the most a block gets. The honest advertiser is asked once the third one's
/// deadline passes, 7 s after the first request; the first one's request times out (6 s) before that and frees its
/// place. Without the cap's bound each would hold the block until its own request timed out
BOOST_AUTO_TEST_CASE( silent_withholders_rerequested_within_cap_test )
{
  const std::vector< block_ptr > chain = generate_chain( 9 );
  start_node();
  run( [&]()
  {
    sync_node( chain, 5 );
    const auto id = [&]( uint32_t num ) { return item_hash_t( chain[ num - 1 ]->get_block_id() ); };
    // each delivers a block, so all are asked before the honest peer, which never did
    std::vector< std::unique_ptr< test_peer > > withholding;
    for( uint32_t i = 0; i < 3; ++i )
    {
      withholding.push_back( connect_live_peer( "withholder " + std::to_string( i ), chain ) );
      withholding.back()->advertise( block_message_type, { id( 6 + i ) } );
      BOOST_REQUIRE_MESSAGE( head_reaches( 6 + i ), "head is " << get_block_num() );
      withholding.back()->ignore( fetch_items_message_type );
    }
    auto honest = connect_live_peer( "honest advertiser", chain );

    withholding.front()->advertise( block_message_type, { id( 9 ) } );
    BOOST_REQUIRE( wait_until( [&]() { return times_requested( *withholding.front(), id( 9 ) ) != 0; }, reply_timeout ) );
    for( size_t i = 1; i < withholding.size(); ++i )
      withholding[ i ]->advertise( block_message_type, { id( 9 ) } );
    honest->advertise( block_message_type, { id( 9 ) } );
    BOOST_REQUIRE_MESSAGE( head_reaches( 9 ), "head is " << get_block_num() );

    const fc::microseconds cap_total = fc::milliseconds( GRAPHENE_NET_BLOCK_REREQUEST_TIMEOUT_MS * ( 1 + 2 + 4 ) );
    const fc::microseconds honest_asked_after = first_request_time( *honest, id( 9 ) ) - first_request_time( *withholding.front(), id( 9 ) );
    BOOST_CHECK_GE( honest_asked_after.count(), ( cap_total - fc::milliseconds( 100 ) ).count() );
    BOOST_CHECK_MESSAGE( honest_asked_after < cap_total + fc::milliseconds( 1500 ),
                         "the honest peer was asked " << honest_asked_after.count() / 1000 << " ms after the first withholder" );
    for( const auto& withholder : withholding )
      BOOST_CHECK_EQUAL( times_requested( *withholder, id( 9 ) ), 1u );
  } );
}

/// A peer that sends every block it is asked for late, 4.5 s after the request (past its deadline, before the request
/// timeout), three times a strike's spacing apart (GRAPHENE_NET_UNAVAILABLE_ITEM_DEMOTION_SEC), is never struck and
/// never disconnected: a late copy only keeps it deprioritized, so the other advertiser is asked for each block at the
/// first request's deadline
BOOST_AUTO_TEST_CASE( repeated_late_block_copies_not_struck_test )
{
  const std::vector< block_ptr > chain = generate_chain( 8 );
  const fc::microseconds withheld_for = fc::milliseconds( 4500 );
  start_node();
  run( [&]()
  {
    sync_node( chain, 5 );
    const auto id = [&]( uint32_t num ) { return item_hash_t( chain[ num - 1 ]->get_block_id() ); };
    auto late = connect_live_peer( "peer sending blocks late", chain );
    auto honest = connect_live_peer( "honest advertiser", chain );
    late->answer_after( fetch_items_message_type, withheld_for );

    fc::time_point last_round = fc::time_point();
    for( uint32_t num = 6; num <= 8; ++num )
    {
      BOOST_TEST_CONTEXT( "block " << num )
      {
        if( last_round != fc::time_point() )
          fc::usleep( last_round + fc::seconds( GRAPHENE_NET_UNAVAILABLE_ITEM_DEMOTION_SEC + 1 ) - fc::time_point::now() );
        last_round = fc::time_point::now();
        late->advertise( block_message_type, { id( num ) } );
        BOOST_REQUIRE( wait_until( [&]() { return times_requested( *late, id( num ) ) != 0; }, reply_timeout ) );
        honest->advertise( block_message_type, { id( num ) } );
        BOOST_REQUIRE_MESSAGE( head_reaches( num ), "head is " << get_block_num() );
        BOOST_CHECK_EQUAL( times_requested( *honest, id( num ) ), 1u );
        // the late copy has come
        fc::usleep( first_request_time( *late, id( num ) ) + withheld_for + fc::milliseconds( 500 ) - fc::time_point::now() );
        const fc::variant_object info = peer_info( *late );
        BOOST_CHECK( info[ "block_fetching_deprioritized" ].as_bool() );
        BOOST_CHECK_EQUAL( info[ "withholding_strikes" ].as_uint64(), 0u );
      }
    }
    require_kept( *late );
    require_kept( *honest );
    BOOST_CHECK_EQUAL( peer_info( *honest )[ "withholding_strikes" ].as_uint64(), 0u );
  } );
}

/// A peer that delivers the transactions it advertised and now and then answers item_not_available for one (it left
/// its cache) is never disconnected for that: each delivery clears its strikes, so four such misses, a demotion
/// apart, leave it with at most the one since its last delivery
BOOST_AUTO_TEST_CASE( occasional_misses_of_a_delivering_peer_not_disconnected_test )
{
  start_node( "", true );
  run( [&]()
  {
    fc::sleep_until( get_genesis_time() + HIVE_BLOCK_INTERVAL );
    wait_for_block_num( 1 );

    auto relay = make_peer( "relay missing a transaction now and then" );
    relay->stop_withholding();
    connect_active( *relay );
    fc::time_point last_round = fc::time_point();
    for( int round = 0; round < 4; ++round )
    {
      BOOST_TEST_CONTEXT( "round " << round )
      {
        // demoted for transactions after a miss, it is asked again only once the demotion is over
        if( last_round != fc::time_point() )
          fc::usleep( last_round + fc::seconds( GRAPHENE_NET_UNAVAILABLE_ITEM_DEMOTION_SEC + 1 ) - fc::time_point::now() );
        last_round = fc::time_point::now();
        const full_transaction_ptr transaction = build_transfer( HIVE_INIT_MINER_NAME, HIVE_NULL_ACCOUNT, ASSET( "0.001 TESTS" ),
                                                                "steady " + std::to_string( round ), get_default_expiration() );
        const item_hash_t missed = fc::ripemd160::hash( "missed " + std::to_string( round ) );
        relay->provide( { transaction } );
        relay->advertise( { transaction->get_legacy_transaction_message_hash(), missed } );
        BOOST_REQUIRE( wait_until( [&]() { return relay->was_requested( missed ); }, reply_timeout ) );
        BOOST_REQUIRE( wait_until( [&]() { return relay->was_requested( transaction->get_legacy_transaction_message_hash() ); }, reply_timeout ) );
        fc::usleep( fc::milliseconds( 500 ) );
        BOOST_REQUIRE( !relay->closed() );
        BOOST_CHECK_LE( peer_info( *relay )[ "withholding_strikes" ].as_uint64(), 1u );
      }
    }
    require_kept( *relay );
  } );
}

/// A peer that has the transactions the node asks it for but whose replies to the node are backed up says it is busy
/// (item_busy_message): that is no withholding, so it is neither struck nor demoted, and remains an advertiser. The
/// node asks the other advertiser for the transaction, and asks the busy peer for no more transactions until it
/// delivers one; then it asks it at once, not a demotion (10 s) later. Three rounds of that leave it without a strike.
/// Like any node that answers so, the busy peer defers its replies, so the node keeps several requests outstanding with it
BOOST_AUTO_TEST_CASE( busy_answers_not_struck_test )
{
  start_node( "", true );
  run( [&]()
  {
    fc::sleep_until( get_genesis_time() + HIVE_BLOCK_INTERVAL );
    wait_for_block_num( 1 );

    const fc::microseconds backlog = fc::seconds( 2 );
    std::mutex busy_mutex;
    std::set< item_hash_t > busy_for;
    auto busy = make_peer( "peer with replies backed up" );
    busy->hello().understands_item_busy = true;
    busy->hello().defers_transaction_replies = true;
    // of each request, it answers the transactions in busy_for as busy at once, and the rest after the backlog
    busy->on( fetch_items_message_type, [&]( test_peer& self, const message& m )
    {
      const fetch_items_message request = m.as< fetch_items_message >();
      std::vector< item_hash_t > delivered;
      for( const item_hash_t& hash : request.items_to_fetch )
      {
        std::lock_guard< std::mutex > guard( busy_mutex );
        if( request.item_type == trx_message_type && busy_for.count( hash ) )
          self.send( item_busy_message( item_id( trx_message_type, hash ) ) );
        else
          delivered.push_back( hash );
      }
      const message rest( fetch_items_message( request.item_type, delivered ) );
      self.later( backlog, [&self, rest]() { self.answer( rest ); } );
    } );
    connect_active( *busy );
    auto holder = make_peer( "other advertiser" );
    connect_active( *holder );
    const auto transfer = [&]( const std::string& memo )
    {
      return build_transfer( HIVE_INIT_MINER_NAME, HIVE_NULL_ACCOUNT, ASSET( "0.001 TESTS" ), memo, get_default_expiration() );
    };

    for( int round = 0; round < 3; ++round )
    {
      BOOST_TEST_CONTEXT( "round " << round )
      {
        const full_transaction_ptr backlogged = transfer( "backlogged " + std::to_string( round ) );
        const full_transaction_ptr refused = transfer( "refused " + std::to_string( round ) );
        const full_transaction_ptr next = transfer( "next " + std::to_string( round ) );
        const item_hash_t backlogged_hash = backlogged->get_legacy_transaction_message_hash();
        const item_hash_t refused_hash = refused->get_legacy_transaction_message_hash();
        const item_hash_t next_hash = next->get_legacy_transaction_message_hash();
        {
          std::lock_guard< std::mutex > guard( busy_mutex );
          busy_for.insert( refused_hash );
        }
        busy->provide( { backlogged, refused, next } );
        holder->provide( { refused } );
        busy->advertise( { backlogged_hash } );
        BOOST_REQUIRE( wait_until( [&]() { return times_requested( *busy, backlogged_hash ) != 0; }, reply_timeout ) );
        const fc::time_point backlog_ends = first_request_time( *busy, backlogged_hash ) + backlog;
        busy->advertise( { refused_hash } );
        BOOST_REQUIRE( wait_until( [&]() { return times_requested( *busy, refused_hash ) != 0; }, reply_timeout ) );
        holder->advertise( { refused_hash } );
        BOOST_REQUIRE( wait_until( [&]() { return holder->was_requested( refused_hash ); }, reply_timeout ) );
        // while its backlog lasts it is not asked for a transaction only it advertised
        busy->advertise( { next_hash } );
        fc::usleep( backlog_ends - fc::milliseconds( 500 ) - fc::time_point::now() );
        BOOST_CHECK( times_requested( *busy, next_hash ) == 0 );
        // once it delivers, it is asked at once
        BOOST_REQUIRE_MESSAGE( wait_until( [&]() { return times_requested( *busy, next_hash ) != 0; }, fc::seconds( 3 ) ),
                               "the busy peer was not asked again after it delivered" );
        fc::usleep( backlog + fc::milliseconds( 500 ) );
        const fc::variant_object info = peer_info( *busy );
        BOOST_CHECK_EQUAL( info[ "withholding_strikes" ].as_uint64(), 0u );
        BOOST_CHECK( !info[ "transaction_replies_backlogged" ].as_bool() );
        BOOST_CHECK_EQUAL( times_requested( *busy, refused_hash ), 1u );
      }
    }
    require_kept( *busy );
  } );
}

/// A peer that asks the node for transactions and stops reading the replies, which understands item_busy_message
/// (GRAPHENE_NET_HELLO_UNDERSTANDS_ITEM_BUSY), is told the node is busy for those over its reply budget
/// (GRAPHENE_NET_MAX_DEFERRED_TRANSACTION_REPLIES_PER_PEER), not that it doesn't have them: that would count against
/// the node as withholding. Every request is answered, with a transaction or as busy, once it reads again
BOOST_AUTO_TEST_CASE( busy_answered_to_a_reader_that_stops_test )
{
  start_node( "", true );
  run( [&]()
  {
    fc::sleep_until( get_genesis_time() + HIVE_BLOCK_INTERVAL );
    wait_for_block_num( 1 );

    auto source = make_peer( "source" );
    connect_in_sync( *source );
    auto reader = make_peer( "peer that stops reading" );
    reader->hello().understands_item_busy = true;
    connect_in_sync( *reader );

    // large transactions, asked for again and again: replies over the budget (and what the send queue and the
    // sockets hold, several MB) are refused, but too few busy answers for the send queue's limit to close the connection
    const size_t transaction_count = 50;
    const size_t requests = 240;
    std::vector< full_transaction_ptr > transactions;
    std::vector< item_hash_t > hashes;
    for( size_t i = 0; i < transaction_count; ++i )
    {
      const std::string memo = "large " + std::to_string( i ) + " ";
      transactions.push_back( build_transfer( HIVE_INIT_MINER_NAME, HIVE_NULL_ACCOUNT, ASSET( "0.001 TESTS" ),
                                              memo + std::string( HIVE_MAX_MEMO_SIZE - 1 - memo.size(), 'x' ), get_default_expiration() ) );
      hashes.push_back( transactions.back()->get_legacy_transaction_message_hash() );
    }
    source->provide( transactions );
    source->advertise( hashes );
    BOOST_REQUIRE( wait_until( [&]()
    {
      return std::all_of( hashes.begin(), hashes.end(), [&]( const item_hash_t& hash ) { return reader->was_advertised( hash ); } );
    }, fc::seconds( 30 ) ) );

    std::atomic< bool > reads = false;
    BOOST_SCOPE_EXIT( &reads ) { reads = true; } BOOST_SCOPE_EXIT_END
    reader->on( trx_message_type, [&reads]( test_peer&, const message& )
    {
      while( !reads )
        fc::usleep( fc::milliseconds( 20 ) );
    } );
    for( size_t i = 0; i < requests; ++i )
      reader->send( fetch_items_message( trx_message_type, hashes ) );
    fc::usleep( quiet_period );
    BOOST_REQUIRE( !reader->closed() );
    reads = true;

    const size_t answers = transaction_count * requests;
    BOOST_REQUIRE_MESSAGE( wait_until( [&]()
    {
      return reader->count( trx_message_type ) + reader->count( item_busy_message_type ) >= answers;
    }, fc::seconds( 60 ) ), reader->count( trx_message_type ) << " transactions and " << reader->count( item_busy_message_type )
                            << " busy answers of " << answers );
    BOOST_CHECK_GT( reader->count( item_busy_message_type ), 0u );
    BOOST_CHECK_GE( reader->count( trx_message_type ), size_t( GRAPHENE_NET_MAX_DEFERRED_TRANSACTION_REPLIES_PER_PEER ) );
    BOOST_CHECK_EQUAL( reader->count( item_not_available_message_type ), 0u );
    require_kept( *reader );
  } );
}

/// (T) A peer that sends a block it was asked for a few bytes at a time, so slowly that it would take about 24 s, has
/// its bytes come more often than the request timeout (active_ignored_request_timeout, 6 s), so the request outlives
/// that timeout, but no longer than the ceiling (GRAPHENE_NET_MAX_BLOCK_REQUEST_TIMEOUTS_WHILE_DOWNLOADING, 12 s):
/// then it times out and the peer is disconnected. The block itself is asked of the next advertiser at 1 s
BOOST_AUTO_TEST_CASE( dripping_block_peer_times_out_at_the_ceiling_test )
{
  const std::vector< block_ptr > chain = generate_chain( 7 );
  const fc::microseconds drip_time = fc::seconds( 24 );
  const fc::microseconds timeout = fc::seconds( GRAPHENE_NET_ACTIVE_IGNORED_REQUEST_TIMEOUT_SECONDS );
  start_node();
  run( [&]()
  {
    sync_node( chain, 5 );
    const auto id = [&]( uint32_t num ) { return item_hash_t( chain[ num - 1 ]->get_block_id() ); };
    throttling_relay link( peer_thread, node_endpoint() );
    auto dripper = connect_proven_peer_through( link, "peer dripping blocks", chain, 6 );
    auto honest = connect_live_peer( "honest advertiser", chain );

    const message delivery{ block_message( chain[ 6 ] ) };
    // its bytes come more often than the timeout, so arrival alone would keep the request alive
    BOOST_REQUIRE_LT( drip_time.count() / int64_t( ( sizeof( message_header ) + delivery.size + 15 ) / 16 ), timeout.count() / 2 );
    stream_through( link, chain[ 6 ], drip_time );
    dripper->advertise( block_message_type, { id( 7 ) } );
    BOOST_REQUIRE( wait_until( [&]() { return times_requested( *dripper, id( 7 ) ) != 0; }, reply_timeout ) );
    honest->advertise( block_message_type, { id( 7 ) } );
    BOOST_REQUIRE_MESSAGE( head_reaches( 7 ), "head is " << get_block_num() );

    const fc::time_point asked = first_request_time( *dripper, id( 7 ) );
    const fc::microseconds honest_asked_after = first_request_time( *honest, id( 7 ) ) - asked;
    BOOST_CHECK_MESSAGE( honest_asked_after < fc::seconds( 2 ),
                         "the honest peer was asked " << honest_asked_after.count() / 1000 << " ms after the dripping one" );
    const fc::microseconds ceiling( timeout.count() * GRAPHENE_NET_MAX_BLOCK_REQUEST_TIMEOUTS_WHILE_DOWNLOADING );
    BOOST_REQUIRE_MESSAGE( dripper->wait_closed( ceiling + fc::seconds( 5 ) ), "the node kept the dripping peer" );
    const fc::microseconds closed_after = fc::time_point::now() - asked;
    BOOST_CHECK_GE( closed_after.count(), ( ceiling - fc::milliseconds( 100 ) ).count() );
    BOOST_CHECK_MESSAGE( closed_after < ceiling + fc::milliseconds( 2500 ),
                         "the dripping peer was disconnected " << closed_after.count() / 1000 << " ms after the request" );
    require_kept( *honest );
  } );
}

/// Three peers that stream a block to finish within the 12 s ceiling, about 10 s after their requests, asked for it first, hold it only for their
/// deadlines in a row, 1 + 2 + 4 s, as silent withholders do: the honest advertiser is asked 7 s after the first
/// request, as the first one's request, 6 s old by then, no longer takes one of the three places a block's requests
/// may hold. Their copies come past their deadlines: each is deprioritized, with no strike
BOOST_AUTO_TEST_CASE( colluding_block_streamers_rerequested_within_cap_test )
{
  const std::vector< block_ptr > chain = generate_chain( 9 );
  const fc::microseconds stream_time = fc::seconds( 10 );
  start_node();
  run( [&]()
  {
    sync_node( chain, 5 );
    const auto id = [&]( uint32_t num ) { return item_hash_t( chain[ num - 1 ]->get_block_id() ); };
    // each delivers a block, so all are asked before the honest peer, which never did
    std::vector< std::unique_ptr< throttling_relay > > links;
    std::vector< std::unique_ptr< test_peer > > streamers;
    for( uint32_t i = 0; i < 3; ++i )
    {
      links.push_back( std::make_unique< throttling_relay >( peer_thread, node_endpoint() ) );
      streamers.push_back( connect_proven_peer_through( *links.back(), "streamer " + std::to_string( i ), chain, 6 + i ) );
    }
    auto honest = connect_live_peer( "honest advertiser", chain );
    for( const auto& link : links )
      stream_through( *link, chain[ 8 ], stream_time );

    streamers.front()->advertise( block_message_type, { id( 9 ) } );
    BOOST_REQUIRE( wait_until( [&]() { return times_requested( *streamers.front(), id( 9 ) ) != 0; }, reply_timeout ) );
    for( size_t i = 1; i < streamers.size(); ++i )
      streamers[ i ]->advertise( block_message_type, { id( 9 ) } );
    honest->advertise( block_message_type, { id( 9 ) } );
    BOOST_REQUIRE_MESSAGE( head_reaches( 9 ), "head is " << get_block_num() );

    const fc::time_point first_asked = first_request_time( *streamers.front(), id( 9 ) );
    const fc::microseconds cap_total = fc::milliseconds( GRAPHENE_NET_BLOCK_REREQUEST_TIMEOUT_MS * ( 1 + 2 + 4 ) );
    const fc::microseconds honest_asked_after = first_request_time( *honest, id( 9 ) ) - first_asked;
    BOOST_CHECK_GE( honest_asked_after.count(), ( cap_total - fc::milliseconds( 100 ) ).count() );
    BOOST_CHECK_MESSAGE( honest_asked_after < cap_total + fc::milliseconds( 1500 ),
                         "the honest peer was asked " << honest_asked_after.count() / 1000 << " ms after the first streamer" );
    for( const auto& streamer : streamers )
      BOOST_CHECK_EQUAL( times_requested( *streamer, id( 9 ) ), 1u );

    // all three copies have come
    fc::usleep( first_request_time( *streamers.back(), id( 9 ) ) + stream_time + fc::seconds( 2 ) - fc::time_point::now() );
    for( size_t i = 0; i < links.size(); ++i )
    {
      BOOST_TEST_CONTEXT( "streamer " << i )
      {
        const fc::variant_object info = peer_info( links[ i ]->node_side_endpoint() );
        BOOST_CHECK( info[ "block_fetching_deprioritized" ].as_bool() );
        BOOST_CHECK_EQUAL( info[ "withholding_strikes" ].as_uint64(), 0u );
      }
    }
    BOOST_CHECK_EQUAL( peer_info( *honest )[ "withholding_strikes" ].as_uint64(), 0u );
  } );
}

/// A busy answer (item_busy_message) from a peer that doesn't defer its replies (no
/// GRAPHENE_NET_HELLO_DEFERS_TRANSACTION_REPLIES in its hello) can't mean its replies to the node are backed up: it is
/// taken as item_not_available, a strike and a demotion, and the transaction is asked of the other advertiser
BOOST_AUTO_TEST_CASE( busy_from_peer_not_deferring_replies_counts_as_not_available_test )
{
  start_node( "", true );
  run( [&]()
  {
    fc::sleep_until( get_genesis_time() + HIVE_BLOCK_INTERVAL );
    wait_for_block_num( 1 );

    auto busy = make_peer( "peer answering busy that doesn't defer its replies" );
    busy->hello().understands_item_busy = true;
    busy->on( fetch_items_message_type, []( test_peer& self, const message& m )
    {
      const fetch_items_message request = m.as< fetch_items_message >();
      for( const item_hash_t& hash : request.items_to_fetch )
        self.send( item_busy_message( item_id( request.item_type, hash ) ) );
    } );
    connect_active( *busy );
    auto holder = make_peer( "other advertiser" );
    connect_active( *holder );

    const full_transaction_ptr transaction = build_transfer( HIVE_INIT_MINER_NAME, HIVE_NULL_ACCOUNT, ASSET( "0.001 TESTS" ), "busy",
                                                            get_default_expiration() );
    const item_hash_t hash = transaction->get_legacy_transaction_message_hash();
    holder->provide( { transaction } );
    busy->advertise( { hash } );
    BOOST_REQUIRE( wait_until( [&]() { return times_requested( *busy, hash ) != 0; }, reply_timeout ) );
    holder->advertise( { hash } );
    BOOST_REQUIRE( wait_until( [&]() { return holder->was_requested( hash ); }, reply_timeout ) );
    const fc::variant_object info = peer_info( *busy );
    BOOST_CHECK_EQUAL( info[ "withholding_strikes" ].as_uint64(), 1u );
    BOOST_CHECK( info[ "transaction_fetching_demoted" ].as_bool() );
    BOOST_CHECK( !info[ "transaction_replies_backlogged" ].as_bool() );
    BOOST_CHECK_EQUAL( times_requested( *busy, hash ), 1u );
    require_kept( *busy );
  } );
}

/// A peer that defers its replies and holds the requests for 4 s before answering busy for all but one transaction
/// of them, which it delivers 100 ms later to be asked again, is busy all the while: once that is more than half of
/// GRAPHENE_NET_TRANSACTION_BUSY_WINDOW_SEC (30 s), after four such rounds, it is demoted for transactions, though not
/// struck
BOOST_AUTO_TEST_CASE( repeated_busy_answers_demoted_test )
{
  start_node( "", true );
  run( [&]()
  {
    fc::sleep_until( get_genesis_time() + HIVE_BLOCK_INTERVAL );
    wait_for_block_num( 1 );

    const fc::microseconds held_for = fc::seconds( 4 );
    // more than it delivers before it is demoted, so that each request it is asked holds one it can deliver
    std::vector< full_transaction_ptr > transactions;
    std::vector< item_hash_t > held;
    for( int i = 0; i < 16; ++i )
    {
      transactions.push_back( build_transfer( HIVE_INIT_MINER_NAME, HIVE_NULL_ACCOUNT, ASSET( "0.001 TESTS" ),
                                              "held " + std::to_string( i ), get_default_expiration() ) );
      held.push_back( transactions.back()->get_legacy_transaction_message_hash() );
    }

    auto busy = make_peer( "peer holding requests and answering busy" );
    busy->hello().understands_item_busy = true;
    busy->hello().defers_transaction_replies = true;
    busy->provide( transactions );
    // what it holds, answered together held_for after the first of them came; on the peer's thread only
    struct held_requests
    {
      std::vector< item_hash_t > hashes;
      bool answer_scheduled = false;
    };
    auto holding = std::make_shared< held_requests >();
    busy->on( fetch_items_message_type, [holding, held_for]( test_peer& self, const message& m )
    {
      const fetch_items_message request = m.as< fetch_items_message >();
      holding->hashes.insert( holding->hashes.end(), request.items_to_fetch.begin(), request.items_to_fetch.end() );
      if( std::exchange( holding->answer_scheduled, true ) )
        return;
      self.later( held_for, [&self, holding]()
      {
        const std::vector< item_hash_t > hashes = std::exchange( holding->hashes, {} );
        holding->answer_scheduled = false;
        for( size_t i = 1; i < hashes.size(); ++i )
          self.send( item_busy_message( item_id( trx_message_type, hashes[ i ] ) ) );
        // after the busy answers, which the node is to see first
        if( !hashes.empty() )
          self.later( fc::milliseconds( 100 ), [&self, rest = message( fetch_items_message( trx_message_type, { hashes.front() } ) )]()
          {
            self.answer( rest );
          } );
      } );
    } );
    connect_active( *busy );

    busy->advertise( held );
    BOOST_REQUIRE( wait_until( [&]() { return busy->count( fetch_items_message_type ) != 0; }, reply_timeout ) );
    const fc::time_point first_asked = busy->received( fetch_items_message_type ).front().time;
    BOOST_REQUIRE_MESSAGE( wait_until( [&]() { return peer_info( *busy )[ "transaction_fetching_demoted" ].as_bool(); }, fc::seconds( 40 ) ),
                           "the busy peer was not demoted after " << busy->count( fetch_items_message_type ) << " requests" );
    const fc::microseconds demoted_after = fc::time_point::now() - first_asked;
    BOOST_CHECK_MESSAGE( demoted_after > fc::seconds( GRAPHENE_NET_TRANSACTION_BUSY_WINDOW_SEC / 2 - 1 ),
                         "the busy peer was demoted " << demoted_after.count() / 1000 << " ms after its first request" );
    BOOST_CHECK_GE( busy->count( fetch_items_message_type ), 4u );
    BOOST_CHECK_EQUAL( peer_info( *busy )[ "withholding_strikes" ].as_uint64(), 0u );
    require_kept( *busy );
  } );
}

/// A transaction the chain rejects, delivered under the id the peer advertised, is no delivery: it doesn't clear the
/// strike the peer took for answering item_not_available. A valid one does, once the chain accepted it
BOOST_AUTO_TEST_CASE( rejected_transaction_does_not_clear_strikes_test )
{
  start_node( "", true );
  run( [&]()
  {
    fc::sleep_until( get_genesis_time() + HIVE_BLOCK_INTERVAL );
    wait_for_block_num( 1 );

    auto peer = make_peer( "peer with a strike" );
    peer->stop_withholding();
    connect_active( *peer );
    const item_hash_t missed = fc::ripemd160::hash( std::string( "missed" ) );
    peer->advertise( { missed } );
    BOOST_REQUIRE( wait_until( [&]() { return peer_info( *peer )[ "withholding_strikes" ].as_uint64() == 1; }, reply_timeout ) );
    // demoted for transactions meanwhile
    fc::usleep( fc::seconds( GRAPHENE_NET_UNAVAILABLE_ITEM_DEMOTION_SEC + 1 ) );

    // from an account that doesn't exist
    const full_transaction_ptr fabricated = build_transfer( "nobody", HIVE_NULL_ACCOUNT, ASSET( "0.001 TESTS" ), "fabricated",
                                                           get_default_expiration() );
    peer->provide( { fabricated } );
    peer->advertise( { fabricated->get_legacy_transaction_message_hash() } );
    BOOST_REQUIRE( wait_until( [&]() { return peer->was_requested( fabricated->get_legacy_transaction_message_hash() ); }, reply_timeout ) );
    fc::usleep( fc::seconds( 1 ) );
    BOOST_CHECK_EQUAL( peer_info( *peer )[ "withholding_strikes" ].as_uint64(), 1u );

    const full_transaction_ptr valid = build_transfer( HIVE_INIT_MINER_NAME, HIVE_NULL_ACCOUNT, ASSET( "0.001 TESTS" ), "valid",
                                                      get_default_expiration() );
    peer->provide( { valid } );
    peer->advertise( { valid->get_legacy_transaction_message_hash() } );
    BOOST_REQUIRE( wait_until( [&]() { return peer->was_requested( valid->get_legacy_transaction_message_hash() ); }, reply_timeout ) );
    BOOST_CHECK( wait_until( [&]() { return peer_info( *peer )[ "withholding_strikes" ].as_uint64() == 0; }, reply_timeout ) );
    require_kept( *peer );
  } );
}

/// A block the node only stores on a shorter fork, delivered by a peer that has a strike, is no delivery to the node:
/// it doesn't clear the strike
BOOST_AUTO_TEST_CASE( fork_only_block_does_not_clear_strikes_test )
{
  const std::vector< block_ptr > fork = generate_chain( 7, 6 );
  const std::vector< block_ptr > chain = generate_chain( 7 );
  BOOST_REQUIRE( fork[ 4 ]->get_block_id() == chain[ 4 ]->get_block_id() );
  BOOST_REQUIRE( fork[ 5 ]->get_block_id() != chain[ 5 ]->get_block_id() );
  start_node();
  run( [&]()
  {
    sync_node( chain, 7 );
    // it has the fork up to its block 6, not its block 7
    auto peer = connect_live_peer( "peer on a shorter fork", std::vector< block_ptr >( fork.begin(), fork.begin() + 6 ) );
    peer->stop_withholding();
    const item_hash_t missing( fork[ 6 ]->get_block_id() );
    peer->advertise( block_message_type, { missing } );
    BOOST_REQUIRE( wait_until( [&]() { return peer_info( *peer )[ "withholding_strikes" ].as_uint64() == 1; }, reply_timeout ) );

    const item_hash_t fork_block( fork[ 5 ]->get_block_id() );
    peer->advertise( block_message_type, { fork_block } );
    BOOST_REQUIRE( wait_until( [&]() { return peer->was_requested( fork_block ); }, reply_timeout ) );
    fc::usleep( fc::seconds( 1 ) );
    BOOST_REQUIRE( head_block_id() == chain[ 6 ]->get_block_id() );
    BOOST_CHECK_EQUAL( peer_info( *peer )[ "withholding_strikes" ].as_uint64(), 1u );
    require_kept( *peer );
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

/// The per-host connection limit leaves loopback alone by default, where local test networks run all their nodes:
/// with a limit of 1, three peers from 127.0.0.1 are all accepted
BOOST_AUTO_TEST_CASE( loopback_not_limited_per_host_test )
{
  start_node( R"("maximum_connections_per_host":1)" );
  run( [&]()
  {
    auto first = make_peer( "first" );
    connect_active( *first );
    auto second = make_peer( "second" );
    connect_active( *second );
    auto third = make_peer( "third" );
    connect_active( *third );
    require_kept( *first );
    require_kept( *second );
  } );
}

/// With loopback not exempt, it is one host like any other: with a limit of 1, a second peer's hello is rejected
/// with too_many_connections_from_host while the first is connected, and the first is kept. Once the first is
/// gone, the next peer is accepted
BOOST_AUTO_TEST_CASE( hello_over_per_host_limit_rejected_test )
{
  start_node( R"("maximum_connections_per_host":1,"host_limit_exempt_subnets":[])" );
  run( [&]()
  {
    auto first = make_peer( "first" );
    connect_active( *first );
    auto second = make_peer( "second on the host" );
    second->connect( node_endpoint() );
    require_rejected( *second, rejection_reason_code::too_many_connections_from_host );
    BOOST_CHECK( second->rejection()->reason_string.find( "nodes on your host" ) != std::string::npos );
    require_kept( *first );

    first->close();
    BOOST_REQUIRE( wait_until( [&]() { return !node_lists( *first ); }, reply_timeout ) );
    auto third = make_peer( "third, after the first left" );
    connect_active( *third );
  } );
}

/// An exempt address stands in for a proxy that rewrites the source address of every inbound peer: with a limit
/// of 1 and 127.0.0.1 exempt, three peers from it are all accepted
BOOST_AUTO_TEST_CASE( exempt_address_not_limited_per_host_test )
{
  start_node( R"("maximum_connections_per_host":1,"host_limit_exempt_subnets":["127.0.0.1"])" );
  run( [&]()
  {
    auto first = make_peer( "first via the proxy" );
    connect_active( *first );
    auto second = make_peer( "second via the proxy" );
    connect_active( *second );
    auto third = make_peer( "third via the proxy" );
    connect_active( *third );
    require_kept( *first );
    require_kept( *second );
  } );
}

/// A peer that rejects the node's hello with too_many_connections_from_host is redialed with the failure backoff,
/// not every peer_connection_retry_timeout (1 s here): after its k-th rejection the node waits over k seconds, up to
/// GRAPHENE_NET_MAX_HOST_LIMIT_REJECTIONS_COUNTED rejections; later ones grow the wait no further (another node behind
/// the same address may be what fills the peer's share for it). Another peer keeps relaying the refusing peer's
/// address, which runs the connect loop about once a second, so the retry delay rather than the loop's 10 s idle wait
/// sets when the node dials again
BOOST_AUTO_TEST_CASE( per_host_rejection_backs_off_test )
{
  start_node( R"("peer_connection_retry_timeout":1)" );
  run( [&]()
  {
    auto relay = make_peer( "address relay" );
    connect_active( *relay );

    const auto refusing_peer = [&]()
    {
      auto peer = make_peer( "refusing peer" );
      peer->on( hello_message_type, []( test_peer& self, const message& )
      {
        self.send( connection_rejected_message( "test peer", GRAPHENE_NET_PROTOCOL_VERSION, self.endpoint(),
                                                rejection_reason_code::too_many_connections_from_host, "rejected by the test" ) );
      } );
      return peer;
    };

    auto refusing = refusing_peer();
    const fc::ip::endpoint refusing_endpoint = refusing->listen();
    p2p().add_node( refusing_endpoint );
    std::vector< fc::time_point > dialed;
    const size_t counted = GRAPHENE_NET_MAX_HOST_LIMIT_REJECTIONS_COUNTED;
    for( size_t round = 0; round < counted + 3; ++round )
    {
      if( round > 0 )
      {
        refusing = refusing_peer();
        refusing->listen( refusing_endpoint.port() );
      }
      const fc::time_point deadline = fc::time_point::now() + fc::seconds( round + 10 );
      while( !refusing->connected() && fc::time_point::now() < deadline )
      {
        address_message relayed;
        relayed.addresses.emplace_back( refusing_endpoint, fc::time_point::now(), fc::microseconds( 0 ), node_id_t(),
                                        peer_connection_direction::inbound, firewalled_state::not_firewalled );
        relay->send( relayed );
        fc::usleep( fc::milliseconds( 250 ) );
      }
      BOOST_REQUIRE_MESSAGE( refusing->connected(), "the node did not dial the refusing peer again, round " << round );
      dialed.push_back( refusing->connected_at() );
      require_closed_with( *refusing, "You rejected my connection request" );
      refusing.reset();
    }
    for( size_t k = 1; k < dialed.size(); ++k )
    {
      BOOST_CHECK_MESSAGE( dialed[ k ] - dialed[ k - 1 ] > fc::seconds( std::min( k, counted ) ),
                           "redialed " << ( dialed[ k ] - dialed[ k - 1 ] ).count() / 1000 << " ms after rejection " << k );
      // counted in full, the delay would be k + 1 seconds; the connect loop runs about once a second
      if( k > counted )
        BOOST_CHECK_MESSAGE( dialed[ k ] - dialed[ k - 1 ] < fc::milliseconds( ( counted + 1 ) * 1000 + 1500 ),
                             "redialed " << ( dialed[ k ] - dialed[ k - 1 ] ).count() / 1000 << " ms after rejection " << k );
    }
    require_kept( *relay );
  } );
}

/// Left at its default, the per-host limit (2) gives way to a peer dialing the node only up to half its desired
/// connections from one host: with a desired 3, none, also once the relax delay is over (0 here) and while the node
/// holds fewer than its desired connections. With loopback not exempt, the third peer from 127.0.0.1 is rejected with
/// too_many_connections_from_host, so one host dialing a restarted node first can't fill its slots
BOOST_AUTO_TEST_CASE( default_per_host_limit_holds_for_hellos_test )
{
  start_node( R"("desired_number_of_connections":3,"maximum_number_of_connections":5,"host_limit_exempt_subnets":[],)"
              R"("host_limit_relax_delay":0)" );
  run( [&]()
  {
    std::vector< std::unique_ptr< test_peer > > peers;
    for( int i = 0; i < 2; ++i )
    {
      peers.push_back( make_peer( "peer " + std::to_string( i ) + " on the host" ) );
      connect_active( *peers.back() );
    }
    auto third = make_peer( "third on the host" );
    third->connect( node_endpoint() );
    require_rejected( *third, rejection_reason_code::too_many_connections_from_host );
    for( const auto& peer : peers )
      require_kept( *peer );
  } );
}

/// Left at its default, the per-host limit (2) gives way to peers dialing the node while it holds fewer than its desired
/// connections (8), up to their host's share of them (all 8 with one host) and half of them: nodes behind a NAT or on
/// a shared /64 reach it past the limit. So the third peer from 127.0.0.1 is accepted. Once 5 peers from 127.0.0.2
/// (exempt, so not limited) make up the desired 8, the fourth from 127.0.0.1 is rejected, though its host would still
/// be within its share (4 of 8 on 2 hosts) and half
BOOST_AUTO_TEST_CASE( default_per_host_limit_relaxed_for_hellos_test )
{
  start_node( R"("desired_number_of_connections":8,"maximum_number_of_connections":12,"host_limit_exempt_subnets":["127.0.0.2"],)"
              R"("host_limit_relax_delay":0)" );
  run( [&]()
  {
    std::vector< std::unique_ptr< test_peer > > peers;
    for( int i = 0; i < 3; ++i )
    {
      peers.push_back( make_peer( "peer " + std::to_string( i ) + " on the host" ) );
      connect_active( *peers.back() );
    }
    std::vector< std::unique_ptr< test_peer > > others;
    for( int i = 0; i < 5; ++i )
    {
      others.push_back( make_peer( "peer " + std::to_string( i ) + " on the exempt host" ) );
      others.back()->connect( node_endpoint(), "127.0.0.2" );
      BOOST_REQUIRE_MESSAGE( others.back()->wait_for( fetch_blockchain_item_ids_message_type, reply_timeout ), "the node did not take up the peer" );
    }
    auto fourth = make_peer( "fourth on the host" );
    fourth->connect( node_endpoint() );
    require_rejected( *fourth, rejection_reason_code::too_many_connections_from_host );
    for( const auto& peer : peers )
      require_kept( *peer );
    BOOST_CHECK_EQUAL( number_of_connections(), 8u );
  } );
}

/// A per-host limit set with p2p-max-connections-per-host is a hard limit: in the setup of
/// default_per_host_limit_relaxed_for_hellos_test, the third peer from 127.0.0.1 is rejected although the node holds
/// fewer than its desired connections and the host is under its share. (One set in p2p-parameters is one too, see
/// hello_over_per_host_limit_rejected_test.)
BOOST_AUTO_TEST_CASE( explicit_per_host_limit_not_relaxed_test )
{
  start_node( R"("desired_number_of_connections":8,"maximum_number_of_connections":12,"host_limit_exempt_subnets":[],)"
              R"("host_limit_relax_delay":0)", false,
              { config_line_t( { "p2p-max-connections-per-host", { "2" } } ) } );
  run( [&]()
  {
    auto first = make_peer( "first on the host" );
    connect_active( *first );
    auto second = make_peer( "second on the host" );
    connect_active( *second );
    auto third = make_peer( "third on the host" );
    third->connect( node_endpoint() );
    require_rejected( *third, rejection_reason_code::too_many_connections_from_host );
    require_kept( *first );
    require_kept( *second );
  } );
}

namespace
{
  /// Peers listening on @p count ports of @p host, each for one connection from the node; their endpoints go to @p endpoints
  std::vector< std::unique_ptr< test_peer > > listening_peers( p2p_protocol_fixture& fixture, const std::string& host, int count,
                                                               std::vector< fc::ip::endpoint >& endpoints )
  {
    std::vector< std::unique_ptr< test_peer > > peers;
    for( int i = 0; i < count; ++i )
    {
      peers.push_back( fixture.make_peer( "listening on " + host + " #" + std::to_string( i ) ) );
      endpoints.push_back( peers.back()->listen( 0, host ) );
    }
    return peers;
  }

  /// Makes the node learn @p endpoints from @p relay (not as add-node targets, which the per-host limit leaves alone)
  void relay_addresses( test_peer& relay, const std::vector< fc::ip::endpoint >& endpoints )
  {
    address_message relayed;
    for( const fc::ip::endpoint& endpoint : endpoints )
      relayed.addresses.emplace_back( endpoint, fc::time_point::now(), fc::microseconds( 0 ), node_id_t(),
                                      peer_connection_direction::inbound, firewalled_state::not_firewalled );
    relay.send( relayed );
  }

  size_t count_connected( const std::vector< std::unique_ptr< test_peer > >& peers )
  {
    return std::count_if( peers.begin(), peers.end(), []( const std::unique_ptr< test_peer >& peer ) { return peer->accepted() && !peer->closed(); } );
  }
}

/// The node saves its whole configuration to node_config.json, the per-host limit included, and loads it on the next
/// start. A limit found only there was not set by the operator, so it stays relaxed: as in
/// default_per_host_limit_relaxed_for_few_hosts_test, the node dials 3 of 5 peers on 127.0.0.2 to reach its desired 4
BOOST_AUTO_TEST_CASE( saved_per_host_limit_stays_relaxed_test )
{
  const fc::path data_dir = hive::utilities::temp_directory_path() / "p2p-protocol-saved-node-config";
  fc::remove_all( data_dir );
  fc::create_directories( data_dir / "p2p" );
  fc::json::save_to_file( fc::mutable_variant_object( "maximum_connections_per_host", 2 ), data_dir / "p2p" / "node_config.json" );
  set_data_dir( data_dir.string() );
  start_node( R"("desired_number_of_connections":4,"maximum_number_of_connections":6,"host_limit_exempt_subnets":[],)"
              R"("host_limit_relax_delay":0)" );
  run( [&]()
  {
    BOOST_REQUIRE_EQUAL( p2p().get_advanced_node_parameters().maximum_connections_per_host, 2u );
    auto relay = make_peer( "address relay" );
    connect_active( *relay );
    std::vector< fc::ip::endpoint > endpoints;
    const auto peers = listening_peers( *this, "127.0.0.2", 5, endpoints );
    relay_addresses( *relay, endpoints );
    BOOST_REQUIRE_MESSAGE( wait_until( [&]() { return count_connected( peers ) == 3; }, reply_timeout ),
                           "connected to " << count_connected( peers ) << " of the peers" );
  } );
}

/// Candidates on 2 hosts only: a relay on 127.0.0.1 and 5 peers on 127.0.0.2. With the default per-host limit the
/// node dials 2 of the peers at once, and once host_limit_relax_delay (5 s here) has passed since it started, dials
/// past the limit until it holds its desired 4 connections, so 3 of the 5 peers
BOOST_AUTO_TEST_CASE( default_per_host_limit_relaxed_for_few_hosts_test )
{
  const fc::microseconds relax_delay = fc::seconds( 5 );
  const fc::time_point before_start = fc::time_point::now();
  start_node( R"("desired_number_of_connections":4,"maximum_number_of_connections":6,"host_limit_exempt_subnets":[],)"
              R"("host_limit_relax_delay":)" + std::to_string( relax_delay.to_seconds() ) );
  run( [&]()
  {
    auto relay = make_peer( "address relay" );
    connect_active( *relay );
    std::vector< fc::ip::endpoint > endpoints;
    const auto peers = listening_peers( *this, "127.0.0.2", 5, endpoints );
    relay_addresses( *relay, endpoints );
    BOOST_REQUIRE_MESSAGE( wait_until( [&]() { return count_connected( peers ) == 2; }, reply_timeout ),
                           "connected to " << count_connected( peers ) << " of the peers" );
    BOOST_REQUIRE_MESSAGE( wait_until( [&]() { return count_connected( peers ) == 3; }, relax_delay + reply_timeout ),
                           "connected to " << count_connected( peers ) << " of the peers" );
    fc::time_point third_connected_at;
    for( const auto& peer : peers )
      if( peer->accepted() )
        third_connected_at = std::max( third_connected_at, peer->connected_at() );
    BOOST_CHECK_MESSAGE( third_connected_at >= before_start + relax_delay,
                         "dialed past the limit " << ( third_connected_at - before_start ).count() / 1000 << " ms after start" );
    fc::usleep( quiet_period );
    BOOST_CHECK_EQUAL( count_connected( peers ), 3u );
    BOOST_CHECK_EQUAL( number_of_connections(), 4u );
  } );
}

/// The same with p2p-max-connections-per-host = 2: the node stays at 2 connections to 127.0.0.2
BOOST_AUTO_TEST_CASE( explicit_per_host_limit_for_few_hosts_test )
{
  start_node( R"("desired_number_of_connections":4,"maximum_number_of_connections":6,"host_limit_exempt_subnets":[])", false,
              { config_line_t( { "p2p-max-connections-per-host", { "2" } } ) } );
  run( [&]()
  {
    auto relay = make_peer( "address relay" );
    connect_active( *relay );
    std::vector< fc::ip::endpoint > endpoints;
    const auto peers = listening_peers( *this, "127.0.0.2", 5, endpoints );
    relay_addresses( *relay, endpoints );
    BOOST_REQUIRE_MESSAGE( wait_until( [&]() { return count_connected( peers ) == 2; }, reply_timeout ),
                           "connected to " << count_connected( peers ) << " of the peers" );
    fc::usleep( quiet_period );
    BOOST_CHECK_EQUAL( count_connected( peers ), 2u );
    BOOST_CHECK_EQUAL( number_of_connections(), 3u );
  } );
}

namespace
{
  /**
   * Has @p fixture's node (desired 4, default per-host limit relaxed at once) fill its slots past the limit: a relay on
   * 127.0.0.1 tells it of 5 peers on 127.0.0.2, and it dials 3 of them. Returns those peers; the 3 it holds are @p held
   */
  std::vector< std::unique_ptr< test_peer > > fill_slots_past_host_limit( p2p_protocol_fixture& fixture, test_peer& relay,
                                                                          std::vector< test_peer* >& held )
  {
    std::vector< fc::ip::endpoint > endpoints;
    auto peers = listening_peers( fixture, "127.0.0.2", 5, endpoints );
    relay_addresses( relay, endpoints );
    BOOST_REQUIRE_MESSAGE( wait_until( [&]() { return count_connected( peers ) == 3; }, reply_timeout ),
                           "connected to " << count_connected( peers ) << " of the peers" );
    for( const auto& peer : peers )
      if( peer->accepted() && !peer->closed() )
        held.push_back( peer.get() );
    return peers;
  }

  /**
   * Requires exactly one of @p held (dialed at about the same time, so which one the node dialed last isn't known
   * here) to be closed to make room for a peer on another host, and the others to be kept
   */
  void require_one_gives_way( const std::vector< test_peer* >& held )
  {
    BOOST_REQUIRE_MESSAGE( wait_until( [&]() {
      return std::any_of( held.begin(), held.end(), []( const test_peer* peer ) { return peer->closed(); } );
    }, reply_timeout ), "no connection over the limit gave way" );
    for( const test_peer* peer : held )
      if( peer->closed() )
      {
        p2p_protocol_fixture::require_closed_with( *peer, "making room for a peer on another host" );
        BOOST_CHECK( !peer->closing_message()->closing_due_to_error );
      }
    fc::usleep( quiet_period );
    BOOST_CHECK_EQUAL( std::count_if( held.begin(), held.end(), []( const test_peer* peer ) { return peer->closed(); } ), 1 );
  }
}

/// A connection held past the default per-host limit gives way to a peer on a new host: with a relay on 127.0.0.1 and
/// 3 peers the node dialed on 127.0.0.2 filling the desired 4, the node, told of a peer on 127.0.0.3, dials it past the
/// desired number, and once it is connected closes one of the 3 (the one dialed last, not as an error). The others are
/// kept, and the node holds 4 connections
BOOST_AUTO_TEST_CASE( relaxed_connection_gives_way_to_new_host_test )
{
  start_node( R"("desired_number_of_connections":4,"maximum_number_of_connections":6,"host_limit_exempt_subnets":[],)"
              R"("host_limit_relax_delay":0)" );
  run( [&]()
  {
    auto relay = make_peer( "address relay" );
    connect_active( *relay );
    std::vector< test_peer* > held;
    const auto peers = fill_slots_past_host_limit( *this, *relay, held );
    BOOST_REQUIRE_EQUAL( held.size(), 3u );
    std::vector< fc::ip::endpoint > endpoints;
    const auto new_host = listening_peers( *this, "127.0.0.3", 1, endpoints );
    relay_addresses( *relay, endpoints );
    BOOST_REQUIRE_MESSAGE( new_host.front()->wait_for( fetch_blockchain_item_ids_message_type, reply_timeout ),
                           "the node did not take up the peer on the new host" );
    require_one_gives_way( held );
    require_kept( *relay );
    BOOST_CHECK( !new_host.front()->closed() );
    BOOST_CHECK_EQUAL( number_of_connections(), 4u );
  } );
}

/// The same while a connection that never sends its hello is held open past the desired 4: the node still dials the
/// peer on the new host at once (in the connect loop pass the relayed address triggers), and one of the 3 gives way to it
BOOST_AUTO_TEST_CASE( relaxed_connection_gives_way_with_handshaking_connection_test )
{
  // long enough that the node does not close the silent connection during the test
  start_node( R"("desired_number_of_connections":4,"maximum_number_of_connections":6,"host_limit_exempt_subnets":[],)"
              R"("peer_inactivity_timeout":60,"host_limit_relax_delay":0)" );
  run( [&]()
  {
    auto relay = make_peer( "address relay" );
    connect_active( *relay );
    std::vector< test_peer* > held;
    const auto peers = fill_slots_past_host_limit( *this, *relay, held );
    BOOST_REQUIRE_EQUAL( held.size(), 3u );
    auto no_hello = make_peer( "silent after key exchange" );
    no_hello->hello().count = 0;
    no_hello->ignore( hello_message_type );
    no_hello->ignore( legacy_hello_message_type );
    no_hello->connect( node_endpoint() );
    BOOST_REQUIRE( wait_until( [&]() { return no_hello->connected(); }, reply_timeout ) );

    std::vector< fc::ip::endpoint > endpoints;
    const auto new_host = listening_peers( *this, "127.0.0.3", 1, endpoints );
    const fc::time_point relayed = fc::time_point::now();
    relay_addresses( *relay, endpoints );
    BOOST_REQUIRE_MESSAGE( wait_until( [&]() { return new_host.front()->connected(); }, reply_timeout ),
                           "the node did not dial the peer on the new host" );
    BOOST_CHECK_MESSAGE( new_host.front()->connected_at() - relayed < fc::seconds( 5 ),
                         "dialed " << ( new_host.front()->connected_at() - relayed ).count() / 1000 << " ms after the address arrived" );
    BOOST_CHECK( !no_hello->closed() );
    require_one_gives_way( held );
    BOOST_CHECK( !new_host.front()->closed() );
  } );
}

/// Category 2 (fallback), D9, D12: a block the node asked one peer for is fetched from another peer that
/// advertised it too when the first one doesn't deliver it: when it ignores the request (the other peer is asked
/// once GRAPHENE_NET_BLOCK_REREQUEST_TIMEOUT_MS, 1 s, passes, before active_ignored_request_timeout, 2 s here, ends
/// the silent peer's connection), when it answers item_not_available (it is kept), and when it sends a copy whose
/// body doesn't match the header (it is disconnected, #999, and the block is fetched again from the other peer, #1001)
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
    const fc::microseconds asked_after = first_request_time( *backup, id( 6 ) ) - first_request_time( *silent, id( 6 ) );
    BOOST_CHECK_GE( asked_after.count(), fc::milliseconds( GRAPHENE_NET_BLOCK_REREQUEST_TIMEOUT_MS - 100 ).count() );
    BOOST_CHECK_LT( asked_after.count(), timeout.count() );

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

/// Has @p relay name @p candidate, listening at @p endpoint, to the node in an address message
static void name_candidate( test_peer& relay, const test_peer& candidate, const fc::ip::endpoint& endpoint )
{
  address_message addresses;
  addresses.addresses.emplace_back( endpoint, fc::time_point_sec( fc::time_point::now() ), fc::microseconds(), candidate.node_id(),
                                    peer_connection_direction::outbound, firewalled_state::not_firewalled );
  relay.send( addresses );
}

/// F4 (T): while the node is behind (an idle node: its head is far older than a minute), its three peers deliver it no
/// block: they answer keepalives, and requests for block ids with nothing it lacks. Holding more than its two desired
/// connections, the node doesn't dial a candidate one of them names until peer_expansion_timeout (4 s here) passed with no
/// block delivered; then it dials it past the desired connections, and syncs the chain from it. Only then does it close
/// peers it had, all of which delivered nothing, down to the desired two: two of the three. The candidate is kept, and a
/// closed peer dialing the node again is not taken back for a timeout
BOOST_AUTO_TEST_CASE( peer_expansion_finds_a_serving_peer_test )
{
  const uint32_t synced = 5;
  const uint32_t ahead = 10;
  const uint32_t expansion_timeout = 4;
  const std::vector< block_ptr > chain = generate_chain( synced + ahead );
  start_node( R"("peer_expansion_timeout":)" + std::to_string( expansion_timeout ) + R"(,"desired_number_of_connections":2)" );
  run( [&]()
  {
    sync_node( chain, synced );
    // the source delivered its last block before this
    const fc::time_point synced_at = fc::time_point::now();
    BOOST_REQUIRE( wait_until( [&]() { return number_of_connections() == 0; }, reply_timeout ) );

    std::vector< std::unique_ptr< test_peer > > had;
    for( const char* name : { "first peer delivering nothing", "second peer delivering nothing", "third peer delivering nothing" } )
    {
      had.push_back( make_peer( name ) );
      connect_active( *had.back() );
    }
    auto candidate = make_peer( "serving candidate" );
    candidate->serve_blocks( chain );
    name_candidate( *had.front(), *candidate, candidate->listen() );
    BOOST_REQUIRE_MESSAGE( candidate->wait_for( fetch_blockchain_item_ids_message_type, fc::seconds( expansion_timeout + 15 ) ),
                           "the node did not dial the candidate" );
    // the node checks once a second, and the source's last block came a little before synced_at
    BOOST_CHECK_GE( ( candidate->connected_at() - synced_at ).count(), fc::seconds( expansion_timeout - 2 ).count() );
    BOOST_REQUIRE_MESSAGE( head_reaches( synced + ahead ), "head is " << get_block_num() );

    const auto closed = [&]()
    {
      std::vector< test_peer* > result;
      for( const auto& peer : had )
        if( peer->closed() )
          result.push_back( peer.get() );
      return result;
    };
    BOOST_REQUIRE_MESSAGE( wait_until( [&]() { return closed().size() == 2; }, reply_timeout ),
                           "the node closed " << closed().size() << " of the peers it had" );
    for( test_peer* peer : closed() )
    {
      require_closed_with( *peer, "nothing I could use" );
      BOOST_CHECK_GE( peer->closed_at().time_since_epoch().count(), candidate->connected_at().time_since_epoch().count() );
    }
    require_kept( *candidate );
    BOOST_CHECK_EQUAL( number_of_connections(), 2u );

    auto back = make_peer( "closed peer dialing again" );
    back->hello().node_id = closed().front()->node_id();
    back->connect( node_endpoint() );
    require_rejected( *back, rejection_reason_code::unspecified );
    BOOST_CHECK( back->rejection()->reason_string.find( "won't take you back" ) != std::string::npos );
  } );
}

/// F5 (T): on a halted chain (an idle node whose peers all have its head and nothing more) no peer can deliver the node a
/// block, also not the one whose hello claims a head past the node's. After peer_expansion_timeout (3 s here) the node
/// dials a candidate a peer names, past its four desired connections; the candidate has nothing more either, and a
/// timeout later the node closes it. The next expansion comes a whole timeout after that: a second candidate named once
/// the first is closed is dialed no sooner, and is closed in turn. The node never closes a peer it had, and never holds
/// fewer than them
BOOST_AUTO_TEST_CASE( peer_expansion_keeps_peers_on_halted_chain_test )
{
  const uint32_t synced = 5;
  const uint32_t expansion_timeout = 3;
  const std::vector< block_ptr > chain = generate_chain( synced );
  start_node( R"("peer_expansion_timeout":)" + std::to_string( expansion_timeout ) + R"(,"desired_number_of_connections":4)" );
  run( [&]()
  {
    sync_node( chain, synced );
    BOOST_REQUIRE( wait_until( [&]() { return number_of_connections() == 0; }, reply_timeout ) );

    std::vector< std::unique_ptr< test_peer > > peers;
    for( const char* name : { "first serving peer", "second serving peer", "third serving peer" } )
    {
      peers.push_back( make_peer( name ) );
      peers.back()->serve_blocks( chain );
      connect_active( *peers.back() );
    }
    peers.push_back( make_peer( "peer claiming a head past the node's" ) );
    peers.back()->hello().last_known_block_number = synced + 10;
    connect_active( *peers.back() );

    size_t fewest_connections = number_of_connections();
    const auto expansion = [&]( const std::string& name )
    {
      auto candidate = make_peer( name );
      candidate->serve_blocks( chain );
      name_candidate( *peers.front(), *candidate, candidate->listen() );
      const fc::time_point deadline = fc::time_point::now() + fc::seconds( 3 * expansion_timeout + 15 );
      while( !candidate->closed() && fc::time_point::now() < deadline )
      {
        fewest_connections = std::min( fewest_connections, number_of_connections() );
        fc::usleep( fc::milliseconds( 200 ) );
      }
      BOOST_REQUIRE_MESSAGE( candidate->connected(), "the node did not dial " << name );
      require_closed_with( *candidate, "you gave me none" );
      return candidate;
    };
    // the first expansion may have started before the first candidate was named
    const auto first = expansion( "first candidate" );
    const auto second = expansion( "second candidate" );
    // the node checks once a second
    BOOST_CHECK_GE( ( second->connected_at() - first->closed_at() ).count(), fc::seconds( expansion_timeout - 1 ).count() );
    BOOST_CHECK_GE( ( second->closed_at() - second->connected_at() ).count(), fc::seconds( expansion_timeout - 1 ).count() );

    for( const auto& peer : peers )
      BOOST_CHECK_MESSAGE( !peer->closed(), "the node disconnected " << peer->hello().user_agent << " on a halted chain" );
    BOOST_CHECK_EQUAL( fewest_connections, peers.size() );
    BOOST_CHECK_EQUAL( number_of_connections(), peers.size() );
  } );
}

/// F5a (T): peer expansion is on by default (90 s), and peer_expansion_timeout 0 disables it: behind, with three peers
/// that deliver it nothing over more than two desired connections, the node doesn't dial a candidate one of them names
BOOST_AUTO_TEST_CASE( peer_expansion_disabled_by_zero_test )
{
  BOOST_REQUIRE_EQUAL( graphene::net::node_configuration().peer_expansion_timeout, 90u );
  const uint32_t synced = 5;
  const std::vector< block_ptr > chain = generate_chain( synced + 5 );
  start_node( R"("peer_expansion_timeout":0,"desired_number_of_connections":2)" );
  run( [&]()
  {
    sync_node( chain, synced );
    BOOST_REQUIRE( wait_until( [&]() { return number_of_connections() == 0; }, reply_timeout ) );

    std::vector< std::unique_ptr< test_peer > > peers;
    for( const char* name : { "first peer delivering nothing", "second peer delivering nothing", "third peer delivering nothing" } )
    {
      peers.push_back( make_peer( name ) );
      connect_active( *peers.back() );
    }
    auto candidate = make_peer( "serving candidate" );
    candidate->serve_blocks( chain );
    name_candidate( *peers.front(), *candidate, candidate->listen() );
    fc::usleep( fc::seconds( 10 ) );
    BOOST_CHECK_MESSAGE( !candidate->connected(), "the node dialed the candidate" );
    for( const auto& peer : peers )
      BOOST_CHECK_MESSAGE( !peer->closed(), "the node disconnected " << peer->hello().user_agent );
  } );
}

/// F6 (T): a node in sync (it produces blocks, so its head is current) never expands, although its three peers deliver
/// it no block and one claims a head past the node's: over three peer_expansion_timeouts (3 s here) it doesn't dial a
/// candidate one of them names past its two desired connections, and closes none of them
BOOST_AUTO_TEST_CASE( peer_expansion_not_while_in_sync_test )
{
  const uint32_t expansion_timeout = 3;
  start_node( R"("peer_expansion_timeout":)" + std::to_string( expansion_timeout ) + R"(,"desired_number_of_connections":2)", true );
  run( [&]()
  {
    fc::sleep_until( get_genesis_time() + HIVE_BLOCK_INTERVAL );
    wait_for_block_num( 1 );
    std::vector< std::unique_ptr< test_peer > > peers;
    for( const char* name : { "first silent peer", "second silent peer" } )
    {
      peers.push_back( make_peer( name ) );
      connect_active( *peers.back() );
    }
    peers.push_back( make_peer( "silent peer claiming a head past the node's" ) );
    peers.back()->hello().last_known_block_number = 1000;
    connect_active( *peers.back() );
    auto candidate = make_peer( "candidate" );
    name_candidate( *peers.front(), *candidate, candidate->listen() );
    fc::usleep( fc::seconds( 3 * expansion_timeout + 2 ) );
    BOOST_CHECK_MESSAGE( !candidate->connected(), "the node dialed the candidate" );
    for( const auto& peer : peers )
      BOOST_CHECK_MESSAGE( !peer->closed(), "the node disconnected " << peer->hello().user_agent );
  } );
}

/// F7 (T): a node in sync (it produces blocks) replaces a peer that for silent_peer_disconnect_timeout (5 s here) relays
/// it nothing and asks it for nothing, only answering keepalives, once it has a candidate to dial in its place: with no
/// candidate it keeps the peer for three windows; once a relaying peer names one, the silent peer is disconnected and the
/// candidate dialed. Never replaced, with the candidate there too, are a peer that advertises the node's blocks back to
/// it, a peer behind the node that syncs from it (asks for block ids and blocks) and a downstream peer that only fetches
/// the blocks the node advertises. Five desired connections leave a floor of two
BOOST_AUTO_TEST_CASE( silent_peer_replaced_in_sync_test )
{
  const uint32_t silent_timeout = 5;
  start_node( R"("silent_peer_disconnect_timeout":)" + std::to_string( silent_timeout ) + R"(,"desired_number_of_connections":5)", true );
  run( [&]()
  {
    fc::sleep_until( get_genesis_time() + HIVE_BLOCK_INTERVAL );
    wait_for_block_num( 1 );

    auto relaying = make_peer( "peer advertising the node's blocks back" );
    connect_in_sync( *relaying );
    auto syncing = make_peer( "peer behind the node syncing from it" );
    syncing->hello().last_known_block_number = 1;
    connect_active( *syncing );
    auto downstream = make_peer( "downstream peer fetching what the node advertises" );
    downstream->on( item_ids_inventory_message_type, []( test_peer& self, const message& m )
    {
      const item_ids_inventory_message inventory = m.as< item_ids_inventory_message >();
      if( inventory.item_type == block_message_type )
        self.send( fetch_items_message( block_message_type, inventory.item_hashes_available ) );
    } );
    connect_in_sync( *downstream );
    auto bystander = make_peer( "peer advertising the node's blocks back too" );
    connect_in_sync( *bystander );
    auto silent = make_peer( "peer answering only keepalives" );
    connect_active( *silent );
    const fc::time_point silent_taken_up = fc::time_point::now();

    // once a second: the relaying peers advertise the node's head, the syncing peer asks for block ids and a block
    const auto exchange = [&]()
    {
      const block_id_type head = head_block_id();
      relaying->advertise( block_message_type, { head } );
      bystander->advertise( block_message_type, { head } );
      syncing->send( fetch_blockchain_item_ids_message( block_message_type, { block_id( 1 ) } ) );
      syncing->request_blocks( { head } );
      fc::usleep( fc::seconds( 1 ) );
    };
    const fc::time_point kept_until = silent_taken_up + fc::seconds( 3 * silent_timeout );
    while( fc::time_point::now() < kept_until )
      exchange();
    BOOST_REQUIRE_MESSAGE( !silent->closed(), "the node disconnected the silent peer with no candidate to replace it" );

    auto candidate = make_peer( "candidate" );
    const fc::ip::endpoint candidate_endpoint = candidate->listen();
    address_message addresses;
    addresses.addresses.emplace_back( candidate_endpoint, fc::time_point_sec( fc::time_point::now() ), fc::microseconds(), candidate->node_id(),
                                      peer_connection_direction::outbound, firewalled_state::not_firewalled );
    relaying->send( addresses );
    const fc::time_point deadline = fc::time_point::now() + reply_timeout;
    while( !silent->closed() && fc::time_point::now() < deadline )
      exchange();
    require_closed_with( *silent, "relayed me nothing" );
    BOOST_REQUIRE_MESSAGE( candidate->wait_for( fetch_blockchain_item_ids_message_type, fc::seconds( 30 ) ), "the node did not dial the candidate" );

    // a window more with the candidate taken up: the floor is two, so only the rule keeps the others
    const fc::time_point watched_until = fc::time_point::now() + fc::seconds( silent_timeout + 2 );
    while( fc::time_point::now() < watched_until )
      exchange();
    for( test_peer* kept : { relaying.get(), syncing.get(), downstream.get(), bystander.get() } )
    {
      BOOST_CHECK_MESSAGE( !kept->closed(), "the node closed the connection to " << kept->hello().user_agent );
      BOOST_CHECK( node_lists( *kept ) );
    }
    BOOST_CHECK( syncing->count( blockchain_item_ids_inventory_message_type ) != 0 );
    BOOST_CHECK( !downstream->blocks_received().empty() );
  } );
}

/// F8: a node connected to node id X (here an inbound peer from 127.0.0.1) dials X's other address (127.0.0.2, as an
/// IPv6 address of a dual-stack seed would be) once, when a peer names it; the duplicate connection is rejected as
/// already_connected, by the node when X orders after its id and by X when X orders before it. While X stays connected
/// the node doesn't dial that address again, although a peer keeps naming it and the retry delay of such a rejection
/// is peer_connection_retry_timeout (1 s here); once X's connection is gone, the node dials it again
BOOST_AUTO_TEST_CASE( connected_node_address_not_redialed_test )
{
  start_node( R"("peer_connection_retry_timeout":1)" );
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

    auto relay = make_peer( "relay" );
    connect_active( *relay );
    uint16_t port = 0;
    for( const node_id_t& id : { higher, lower } )
      BOOST_TEST_CONTEXT( ( id < node_id ? "lower" : "higher" ) << " id than the node's" )
      {
        const bool peer_rejects = id < node_id;
        const auto other_address = [&]( const std::string& name, bool rejects )
        {
          auto peer = make_peer( name );
          peer->hello().node_id = id;
          if( rejects )
            peer->on( hello_message_type, []( test_peer& self, const message& )
            {
              self.send( connection_rejected_message( "test peer", GRAPHENE_NET_PROTOCOL_VERSION, self.endpoint(),
                                                      rejection_reason_code::already_connected, "already connected" ) );
            } );
          port = peer->listen( port, "127.0.0.2" ).port();
          return peer;
        };
        auto connected = make_peer( "node X" );
        connected->hello().node_id = id;
        connect_active( *connected );

        auto alias = other_address( "node X at its other address", peer_rejects );
        const fc::ip::endpoint alias_endpoint( fc::ip::address( "127.0.0.2" ), port );
        const auto name_alias = [&]()
        {
          address_message relayed;
          relayed.addresses.emplace_back( alias_endpoint, fc::time_point_sec( fc::time_point::now() ), fc::microseconds( 0 ), id,
                                          peer_connection_direction::inbound, firewalled_state::not_firewalled );
          relay->send( relayed );
          fc::usleep( fc::milliseconds( 250 ) );
        };
        fc::time_point deadline = fc::time_point::now() + fc::seconds( 30 );
        while( !alias->connected() && fc::time_point::now() < deadline )
          name_alias();
        BOOST_REQUIRE_MESSAGE( alias->connected(), "the node did not dial the other address" );
        BOOST_REQUIRE( alias->wait_closed( reply_timeout ) );
        if( peer_rejects )
          require_closed_with( *alias, "You rejected my connection request" );
        else
          BOOST_CHECK( alias->rejection() && alias->rejection()->reason_code == rejection_reason_code::already_connected );
        alias.reset();

        // the old code redialed within a retry delay and a connect loop pass, about 2 s
        auto again = other_address( "node X at its other address again", peer_rejects );
        const fc::time_point watched_until = fc::time_point::now() + fc::seconds( 8 );
        while( fc::time_point::now() < watched_until )
          name_alias();
        BOOST_CHECK_MESSAGE( !again->connected(), "the node dialed the other address of a connected node again" );
        BOOST_CHECK( node_lists( *connected ) );
        again.reset();

        auto after = other_address( "node X at its other address after its connection closed", false );
        connected->close();
        BOOST_REQUIRE( wait_until( [&]() { return !node_lists( *connected ); }, reply_timeout ) );
        deadline = fc::time_point::now() + fc::seconds( GRAPHENE_NET_CONNECT_LOOP_MAXIMUM_WAIT_SEC + 10 );
        while( !after->connected() && fc::time_point::now() < deadline )
          name_alias();
        BOOST_REQUIRE_MESSAGE( after->connected(), "the node did not dial the other address once the node's connection closed" );
        BOOST_CHECK( after->wait_for( fetch_blockchain_item_ids_message_type, reply_timeout ) );
        after->close();
        BOOST_REQUIRE( wait_until( [&]() { return !node_lists( *after ); }, reply_timeout ) );
      }
    require_kept( *relay );
  } );
}

BOOST_AUTO_TEST_SUITE_END()

#endif
