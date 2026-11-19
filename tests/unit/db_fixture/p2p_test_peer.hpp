#pragma once

/**
  * In-process p2p peers for tests that talk to a node under test one message at a time.
  *
  * test_peer is a peer built on graphene::net::peer_connection, so it does the real key exchange and
  * encryption over loopback TCP, but its behaviour is scripted by the test: what it says in its hello,
  * how it answers each message type, which raw messages it sends and when. It records everything the
  * node sends it and how the connection ended.
  *
  * raw_connection goes below peer_connection, for cases that have to break the key exchange or the
  * message framing itself.
  *
  * Both live on an fc::thread the test gives them, like a node's connections live on the p2p thread;
  * the test thread calls them and reads what they recorded.
  */

#include <boost/test/unit_test.hpp>

#include <hive/chain/full_block.hpp>
#include <hive/chain/full_transaction.hpp>
#include <hive/protocol/types.hpp>

#include <graphene/net/config.hpp>
#include <graphene/net/core_messages.hpp>
#include <graphene/net/peer_connection.hpp>
#include <graphene/net/stcp_socket.hpp>

#include <fc/network/tcp_socket.hpp>
#include <fc/thread/thread.hpp>

#include <algorithm>
#include <atomic>
#include <cstring>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <vector>

namespace p2p_tests {

/// Polls @p condition until it holds or @p timeout passes; true if it holds
inline bool wait_until( const std::function< bool() >& condition, fc::microseconds timeout )
{
  const fc::time_point deadline = fc::time_point::now() + timeout;
  while( !condition() )
  {
    if( fc::time_point::now() >= deadline )
      return false;
    fc::usleep( fc::milliseconds( 20 ) );
  }
  return true;
}

/// What a test_peer says in its hello, and how it goes through that step of the handshake
struct hello_options
{
  std::string user_agent = "test peer";
  uint32_t core_protocol_version = GRAPHENE_NET_PROTOCOL_VERSION;
  /// user_data.chain_id; left out when empty
  fc::optional< hive::protocol::chain_id_type > chain_id;
  /// user_data.node_id; the public key of the peer's key when empty
  fc::optional< graphene::net::node_id_t > node_id;
  /// signs the shared secret instead of the peer's key, which makes the signature invalid
  fc::optional< fc::ecc::private_key > signing_key;
  /// user_data.last_known_block_number; the head of the served chain when empty and blocks are served
  fc::optional< uint32_t > last_known_block_number;
  /// user_data.last_available_zstd_compression_dictionary_number; left out when empty
  fc::optional< uint8_t > last_available_zstd_compression_dictionary_number;
  /// user_data.defers_transaction_replies (GRAPHENE_NET_HELLO_DEFERS_TRANSACTION_REPLIES); left out when false, as by
  /// a node that queues each reply at once
  bool defers_transaction_replies = false;
  /// 0 says the peer doesn't listen; when it listens (test_peer::listen()) its listening port is used instead
  uint16_t inbound_port = 0;
  /// the local port of the connection when empty
  fc::optional< uint16_t > outbound_port;
  /// current: hello (5018) only, legacy: legacy_hello (5006) only, like a peer that predates IPv6 support, both: both
  enum class form { current, legacy, both };
  form hello_form = form::current;
  /// how many times the hello is sent: 0 skips it
  uint32_t count = 1;
  /// how long after the key exchange the hello is sent
  fc::microseconds delay;
};

/// A message the peer received, with the time it arrived
struct received_message
{
  graphene::net::message msg;
  fc::time_point time;
};

/**
 * A p2p peer driven by the test rather than by a node.
 *
 * By default it behaves like a node that is in sync with the one under test: it completes the handshake,
 * answers address and time requests, tells the node it has no blocks the node doesn't (unless it serves a
 * chain, see serve_blocks()), delivers the transactions and blocks it was given, and withholds (or, after
 * stop_withholding(), reports as unavailable) anything else it is asked for. When the node closes the
 * connection with a closing_connection_message, it closes its side.
 *
 * Every received message is recorded with its arrival time, and so is how the connection ended.
 * on() replaces the default answer to one message type, e.g. to ignore it, answer late, twice or with
 * something else; send() and send_raw() send anything at any time.
 */
class test_peer final : public graphene::net::peer_connection_delegate
{
public:
  /// Handles a message the peer received, on the peer's thread, instead of the default answer
  using handler_t = std::function< void( test_peer&, const graphene::net::message& ) >;

  test_peer( fc::thread& thread, const hive::protocol::chain_id_type& chain_id, const std::string& user_agent )
    : _thread( thread ), _key( fc::ecc::private_key::generate() )
  {
    _hello.chain_id = chain_id;
    _hello.user_agent = user_agent;
  }

  ~test_peer()
  {
    _thread.async( [this]()
    {
      _destroying = true;
      if( _server )
        _server->close();
      if( _accept_task.valid() && !_accept_task.ready() )
        _accept_task.cancel_and_wait( __FUNCTION__ );
      for( fc::future< void >& delayed : _delayed )
        if( delayed.valid() && !delayed.ready() )
          delayed.cancel_and_wait( __FUNCTION__ );
      if( _connection )
        _connection->destroy_connection( __FUNCTION__ );
      _connection.reset();
      _server.reset();
    } ).wait();
  }

  /// The hello the peer will send; change it before connect() or listen()
  hello_options& hello() { return _hello; }
  graphene::net::node_id_t node_id() const { return _hello.node_id ? *_hello.node_id : graphene::net::node_id_t( _key.get_public_key().serialize() ); }

  /// Connects to the node (an inbound connection for the node) and, after the key exchange, starts the handshake
  void connect( const fc::ip::endpoint& node_endpoint )
  {
    _thread.async( [&]()
    {
      _connection = graphene::net::peer_connection::make_shared( this );
      _connection->connect_to( node_endpoint );
      on_connected();
    } ).wait();
  }

  /**
   * Listens on a loopback port for one connection from the node (an outbound connection for the node,
   * which the test makes it dial with p2p_plugin::add_node()), and returns that endpoint. Once the node
   * connects and the key exchange is done, the handshake starts as after connect().
   */
  fc::ip::endpoint listen()
  {
    return _thread.async( [&]()
    {
      _server = std::make_unique< fc::tcp_server >();
      _server->listen( fc::ip::endpoint( fc::ip::address( "127.0.0.1" ), 0 ) );
      _listening_endpoint = _server->get_local_endpoint();
      _hello.inbound_port = _listening_endpoint.port();
      _accept_task = fc::async( [this]()
      {
        try
        {
          graphene::net::peer_connection_ptr connection = graphene::net::peer_connection::make_shared( this );
          _server->accept( connection->get_socket() );
          connection->accept_connection();
          _connection = connection;
          on_connected();
        }
        catch( const fc::canceled_exception& )
        {
          throw;
        }
        catch( const fc::exception& e )
        {
          if( !_destroying )
            wlog( "test peer ${agent}: accepting the node's connection failed: ${e}", ( "agent", _hello.user_agent )( "e", e.to_detail_string() ) );
        }
      }, "test_peer accept" );
      return _listening_endpoint;
    } ).wait();
  }

  /// true once the key exchange with the node is done
  bool connected() const { return _connected; }
  /// true once the node accepted our hello
  bool accepted() const { return count( graphene::net::connection_accepted_message_type ) != 0; }

  /// The endpoint the node sees this peer at
  fc::ip::endpoint endpoint() const
  {
    std::lock_guard< std::mutex > guard( _mutex );
    return _local_endpoint;
  }

  /// Replaces the default answer to messages of type @p msg_type
  void on( uint32_t msg_type, handler_t handler )
  {
    _thread.async( [&]() { _handlers[ msg_type ] = std::move( handler ); } ).wait();
  }
  /// Messages of type @p msg_type are recorded, but not answered
  void ignore( uint32_t msg_type ) { on( msg_type, []( test_peer&, const graphene::net::message& ) {} ); }
  /// Messages of type @p msg_type are answered as by default, but @p delay after they arrive
  void answer_after( uint32_t msg_type, fc::microseconds delay )
  {
    on( msg_type, [delay]( test_peer& peer, const graphene::net::message& m ) { peer.later( delay, [&peer, m]() { peer.answer( m ); } ); } );
  }
  /// Messages of type @p msg_type are answered as by default, twice
  void answer_twice( uint32_t msg_type )
  {
    on( msg_type, []( test_peer& peer, const graphene::net::message& m ) { peer.answer( m ); peer.answer( m ); } );
  }

  /// The default answer to @p received; for handlers, which run on the peer's thread
  void answer( const graphene::net::message& received );

  /// Runs @p task on the peer's thread @p delay from now; for handlers
  void later( fc::microseconds delay, std::function< void() > task )
  {
    _delayed.push_back( fc::schedule( [this, task]() { if( !_destroying ) task(); }, fc::time_point::now() + delay, "test_peer later" ) );
  }

  /// Sends @p m as it is
  void send( const graphene::net::message& m )
  {
    _thread.async( [&]() { send_now( m ); } ).wait();
  }
  /// Sends a message of type @p msg_type whose payload is @p payload, whatever that decodes to
  void send_raw( uint32_t msg_type, const std::vector< char >& payload )
  {
    graphene::net::message m;
    m.msg_type = msg_type;
    m.data = payload;
    m.size = static_cast< uint32_t >( payload.size() );
    send( m );
  }
  /// Sends the hello the options describe once more, at once
  void send_hello() { _thread.async( [&]() { send_hello_now(); } ).wait(); }

  /// Closes the connection from this side without a closing_connection_message
  void close()
  {
    _thread.async( [&]()
    {
      if( !_connection )
        return;
      _closed_by_us = true;
      _connection->close_connection();
    } ).wait();
  }
  /// From now on the peer keeps its socket open when the node asks it to close the connection
  void keep_open_when_asked_to_close() { _thread.async( [&]() { _close_when_asked = false; } ).wait(); }

  /// Messages of type @p msg_type the peer received so far, oldest first
  std::vector< received_message > received( uint32_t msg_type ) const
  {
    std::lock_guard< std::mutex > guard( _mutex );
    std::vector< received_message > result;
    std::copy_if( _received.begin(), _received.end(), std::back_inserter( result ),
                  [&]( const received_message& r ) { return r.msg.msg_type == msg_type; } );
    return result;
  }
  /// Every message the peer received so far, oldest first
  std::vector< received_message > received() const
  {
    std::lock_guard< std::mutex > guard( _mutex );
    return _received;
  }
  size_t count( uint32_t msg_type ) const { return received( msg_type ).size(); }
  /// The payloads of the messages of type T::type received so far, decoded
  template< typename T >
  std::vector< T > received_as() const
  {
    std::vector< T > result;
    for( const received_message& r : received( T::type ) )
      result.push_back( r.msg.as< T >() );
    return result;
  }
  /// Waits until @p n messages of type @p msg_type arrived in all
  bool wait_for( uint32_t msg_type, fc::microseconds timeout, size_t n = 1 ) const
  {
    return wait_until( [&]() { return count( msg_type ) >= n; }, timeout );
  }

  /// true once the connection is closed, whichever side closed it
  bool closed() const { return _closed; }
  /// true when this side closed the connection (close(), or after the node asked it to)
  bool closed_by_us() const { return _closed_by_us; }
  fc::time_point closed_at() const
  {
    std::lock_guard< std::mutex > guard( _mutex );
    return _closed_at;
  }
  fc::time_point connected_at() const
  {
    std::lock_guard< std::mutex > guard( _mutex );
    return _connected_at;
  }
  bool wait_closed( fc::microseconds timeout ) const { return wait_until( [&]() { return closed(); }, timeout ); }
  /// The node's closing_connection_message, if one arrived and could be decoded
  fc::optional< graphene::net::closing_connection_message > closing_message() const
  {
    std::lock_guard< std::mutex > guard( _mutex );
    return _closing_message;
  }
  /// The node's connection_rejected_message (or the legacy one, converted), if one arrived
  fc::optional< graphene::net::connection_rejected_message > rejection() const
  {
    std::lock_guard< std::mutex > guard( _mutex );
    return _rejection;
  }
  /// The node's hello (5018), if one arrived
  fc::optional< graphene::net::hello_message > node_hello() const
  {
    const std::vector< graphene::net::hello_message > hellos = received_as< graphene::net::hello_message >();
    return hellos.empty() ? fc::optional< graphene::net::hello_message >() : fc::optional< graphene::net::hello_message >( hellos.front() );
  }
  /// The shared secret of the key exchange, which the hellos sign
  fc::sha512 shared_secret() const
  {
    return _thread.async( [&]() { return _connection->get_shared_secret(); } ).wait();
  }

  /// true once the node asked us for blocks and we told it we have nothing it doesn't
  bool in_sync() const { return _in_sync; }

  /**
   * From now on the peer has the chain @p blocks (block 1 onwards, consecutive): it offers the node the
   * blocks the node lacks when asked for block ids, and delivers them when asked. Set before connecting,
   * its head is the last_known_block_number of the hello.
   */
  void serve_blocks( const std::vector< std::shared_ptr< hive::chain::full_block_type > >& blocks )
  {
    _thread.async( [&]()
    {
      _chain = blocks;
      _chain_index.clear();
      for( size_t i = 0; i < _chain.size(); ++i )
        _chain_index[ _chain[ i ]->get_block_id() ] = i;
    } ).wait();
  }

  void provide( const std::vector< hive::chain::full_transaction_ptr >& transactions )
  {
    _thread.async( [&]()
    {
      for( const hive::chain::full_transaction_ptr& transaction : transactions )
        _deliverable[ transaction->get_legacy_transaction_message_hash() ] = transaction;
    } ).wait();
  }

  /// Advertises transactions with message hashes @p hashes
  void advertise( const std::vector< graphene::net::item_hash_t >& hashes ) { advertise( graphene::net::trx_message_type, hashes ); }

  /// Advertises items of type @p item_type
  void advertise( uint32_t item_type, const std::vector< graphene::net::item_hash_t >& hashes )
  {
    _thread.async( [&]()
    {
      // a connection closes when more than GRAPHENE_NET_MAXIMUM_QUEUED_MESSAGES_IN_BYTES wait to be sent,
      // so a flood goes out in several messages, each one sent before the next is queued
      const size_t max_hashes_per_message = GRAPHENE_NET_MAXIMUM_QUEUED_MESSAGES_IN_BYTES / 2 / sizeof( graphene::net::item_hash_t );
      for( size_t begin = 0; begin < hashes.size() && !_closed; begin += max_hashes_per_message )
      {
        const size_t end = std::min( hashes.size(), begin + max_hashes_per_message );
        const graphene::net::message inventory( graphene::net::item_ids_inventory_message( item_type,
          std::vector< graphene::net::item_hash_t >( hashes.begin() + begin, hashes.begin() + end ) ) );
        const uint64_t bytes_sent_before = _connection->get_total_bytes_sent();
        _connection->send_message( inventory );
        while( _connection->get_total_bytes_sent() < bytes_sent_before + inventory.size && !_closed )
          fc::usleep( fc::milliseconds( 1 ) );
      }
    } ).wait();
  }

  /// Asks the node for the blocks @p ids in one fetch_items_message
  void request_blocks( const std::vector< graphene::net::item_hash_t >& ids )
  {
    send( graphene::net::fetch_items_message( graphene::net::block_message_type, ids ) );
  }

  /// ids of the blocks received so far, in either form, in the order they arrived
  std::vector< hive::protocol::block_id_type > blocks_received() const
  {
    std::vector< hive::protocol::block_id_type > ids;
    for( const received_message& r : received() )
      if( r.msg.msg_type == graphene::net::block_message_type || r.msg.msg_type == graphene::net::compressed_block_message_type )
        ids.push_back( id_of_received_block( r.msg ) );
    return ids;
  }

  /// the requested block ids the node reported as not available, in the order it did
  std::vector< graphene::net::item_hash_t > blocks_not_available() const
  {
    std::vector< graphene::net::item_hash_t > ids;
    for( const graphene::net::item_not_available_message& reply : received_as< graphene::net::item_not_available_message >() )
      if( reply.requested_item.item_type == graphene::net::block_message_type )
        ids.push_back( reply.requested_item.item_hash );
    return ids;
  }

  /// from now on the connection is closed as soon as the items requested in one message are sent;
  /// requests that arrive meanwhile are ignored
  void disconnect_after_delivery()
  {
    _thread.async( [&]() { _disconnect_after_delivery = true; } ).wait();
  }

  /**
   * Tells the node our head block is @p head, its head too: once it agrees (see node_considers_us_in_sync()),
   * it advertises the items it receives to us. Tell it again if its head moved on in the meantime.
   */
  void tell_head( const hive::protocol::block_id_type& head )
  {
    _thread.async( [&]()
    {
      _told_head = head;
      _connection->send_message( graphene::net::fetch_blockchain_item_ids_message( graphene::net::block_message_type, { head } ) );
    } ).wait();
  }

  /// true once the node answered tell_head() with nothing we lack
  bool node_considers_us_in_sync() const { return _node_considers_us_in_sync; }

  /// whether the node advertised the transaction with message hash @p hash to us
  bool was_advertised( const graphene::net::item_hash_t& hash ) const
  {
    return _thread.async( [&]() { return _advertised_transactions.count( hash ) != 0; } ).wait();
  }

  /// whether the node asked us for the item with hash @p hash
  bool was_requested( const graphene::net::item_hash_t& hash ) const
  {
    return _thread.async( [&]() { return _requested.count( hash ) != 0; } ).wait();
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

  void on_message( graphene::net::peer_connection*, const graphene::net::message& received_message ) override
  {
    record( received_message );
    auto handler = _handlers.find( received_message.msg_type );
    if( handler != _handlers.end() )
      handler->second( *this, received_message );
    else
      answer( received_message );
  }

  void on_connection_closed( graphene::net::peer_connection* ) override
  {
    std::lock_guard< std::mutex > guard( _mutex );
    if( !_closed )
      _closed_at = fc::time_point::now();
    _closed = true;
  }

  std::shared_ptr< hive::chain::full_block_type > get_full_block_by_block_id( const hive::protocol::block_id_type& ) override { return {}; }
  std::shared_ptr< hive::chain::full_block_type > get_block_to_send( graphene::net::peer_connection*, const graphene::net::item_hash_t& ) override { return {}; }

private:
  /// the id of the block in a block message, in either form the node sends (see core_messages.cpp)
  static hive::protocol::block_id_type id_of_received_block( const graphene::net::message& received_message )
  {
    if( received_message.msg_type == graphene::net::core_message_type_enum::block_message_type )
    {
      // the serialized block followed by its id
      hive::protocol::block_id_type id;
      BOOST_REQUIRE_GT( received_message.data.size(), sizeof( id ) );
      memcpy( id.data(), received_message.data.data() + received_message.data.size() - sizeof( id ), sizeof( id ) );
      return id;
    }
    // two bytes of compression flags followed by the compressed block
    BOOST_REQUIRE_GT( received_message.data.size(), 2u );
    hive::chain::block_attributes_t attributes;
    attributes.flags = ( received_message.data[0] & 0x80 ) ? hive::chain::block_flags::zstd : hive::chain::block_flags::uncompressed;
    if( received_message.data[0] & 0x40 )
      attributes.dictionary_number = received_message.data[1];
    const size_t compressed_size = received_message.data.size() - 2;
    std::unique_ptr< char[] > compressed_bytes( new char[ compressed_size ] );
    memcpy( compressed_bytes.get(), received_message.data.data() + 2, compressed_size );
    return hive::chain::full_block_type::create_from_compressed_block_data( std::move( compressed_bytes ), compressed_size, attributes )->get_block_id();
  }

  void on_connected()
  {
    {
      std::lock_guard< std::mutex > guard( _mutex );
      _local_endpoint = _connection->get_local_endpoint();
      _connected_at = fc::time_point::now();
    }
    _connected = true;
    if( _hello.count == 0 )
      return;
    if( _hello.delay > fc::microseconds() )
      later( _hello.delay, [this]() { for( uint32_t i = 0; i < _hello.count; ++i ) send_hello_now(); } );
    else
      for( uint32_t i = 0; i < _hello.count; ++i )
        send_hello_now();
  }

  void record( const graphene::net::message& m )
  {
    using namespace graphene::net;
    std::lock_guard< std::mutex > guard( _mutex );
    _received.push_back( { m, fc::time_point::now() } );
    try
    {
      if( m.msg_type == closing_connection_message_type )
        _closing_message = m.as< closing_connection_message >();
      else if( m.msg_type == connection_rejected_message_type )
        _rejection = m.as< connection_rejected_message >();
      else if( m.msg_type == legacy_connection_rejected_message_type )
      {
        const legacy_connection_rejected_message legacy = m.as< legacy_connection_rejected_message >();
        _rejection = connection_rejected_message( legacy.user_agent, legacy.core_protocol_version, legacy.remote_endpoint.to_endpoint(),
                                                  legacy.reason_code, legacy.reason_string );
      }
    }
    catch( const fc::exception& e )
    {
      wlog( "test peer ${agent} could not decode a message of type ${type}: ${e}", ( "agent", _hello.user_agent )( "type", m.msg_type )( "e", e.to_detail_string() ) );
    }
  }

  void send_now( const graphene::net::message& m )
  {
    if( _connection && !_closed )
      _connection->send_message( m );
  }

  void send_hello_now()
  {
    using namespace graphene::net;
    fc::sha256::encoder shared_secret_encoder;
    const fc::sha512 shared_secret = _connection->get_shared_secret();
    shared_secret_encoder.write( shared_secret.data(), sizeof( shared_secret ) );
    const node_id_t public_key = _key.get_public_key().serialize();
    const fc::ecc::compact_signature signature =
      ( _hello.signing_key ? *_hello.signing_key : _key ).sign_compact( shared_secret_encoder.result() );

    fc::mutable_variant_object user_data;
    user_data[ "node_id" ] = node_id();
    if( _hello.chain_id )
      user_data[ "chain_id" ] = *_hello.chain_id;
    if( _hello.last_known_block_number )
      user_data[ "last_known_block_number" ] = *_hello.last_known_block_number;
    else if( !_chain.empty() )
      user_data[ "last_known_block_number" ] = _chain.back()->get_block_num();
    if( _hello.last_available_zstd_compression_dictionary_number )
      user_data[ "last_available_zstd_compression_dictionary_number" ] = *_hello.last_available_zstd_compression_dictionary_number;
    if( _hello.defers_transaction_replies )
      user_data[ GRAPHENE_NET_HELLO_DEFERS_TRANSACTION_REPLIES ] = true;

    const fc::ip::endpoint local_endpoint = _connection->get_local_endpoint();
    const uint16_t outbound_port = _hello.outbound_port ? *_hello.outbound_port : local_endpoint.port();
    if( _hello.hello_form != hello_options::form::legacy )
      send_now( hello_message( _hello.user_agent, _hello.core_protocol_version, local_endpoint.get_address(), _hello.inbound_port,
                               outbound_port, public_key, signature, user_data ) );
    if( _hello.hello_form != hello_options::form::current )
      send_now( legacy_hello_message( _hello.user_agent, _hello.core_protocol_version, fc::ip::legacy_address( local_endpoint.get_address() ),
                                      _hello.inbound_port, outbound_port, public_key, signature, user_data ) );
  }

  void answer_fetch_blockchain_item_ids( const graphene::net::fetch_blockchain_item_ids_message& request )
  {
    using namespace graphene::net;
    std::vector< item_hash_t > reply;
    uint32_t total_remaining = 0;
    if( _chain.empty() )
    {
      // answering with the last block of the node's synopsis tells it we have no blocks it doesn't
      if( !request.blockchain_synopsis.empty() )
        reply.push_back( request.blockchain_synopsis.back() );
    }
    else
    {
      // the newest synopsis block we have, followed by the blocks after it; from block 1 for an empty synopsis
      fc::optional< size_t > start;
      if( request.blockchain_synopsis.empty() )
        start = 0;
      for( auto it = request.blockchain_synopsis.rbegin(); !start && it != request.blockchain_synopsis.rend(); ++it )
      {
        auto known = _chain_index.find( *it );
        if( known != _chain_index.end() )
          start = known->second;
      }
      if( start )
      {
        const size_t end = std::min( _chain.size(), *start + max_ids_per_reply );
        for( size_t i = *start; i < end; ++i )
          reply.push_back( _chain[ i ]->get_block_id() );
        total_remaining = static_cast< uint32_t >( _chain.size() - end );
      }
    }
    send_now( blockchain_item_ids_inventory_message( total_remaining, request.item_type, reply ) );
    if( reply.size() <= 1 && total_remaining == 0 )
      _in_sync = true;
  }

  void answer_fetch_items( const graphene::net::fetch_items_message& request )
  {
    using namespace graphene::net;
    if( _leaving )
      return;
    const uint64_t bytes_sent_before = _connection->get_total_bytes_sent();
    uint64_t bytes_delivered = 0;
    for( const item_hash_t& hash : request.items_to_fetch )
    {
      const item_id item( request.item_type, hash );
      _requested.insert( hash );
      fc::optional< message > delivery;
      if( request.item_type == trx_message_type )
      {
        auto deliverable = _deliverable.find( hash );
        if( deliverable != _deliverable.end() )
          delivery = message( trx_message( deliverable->second ) );
      }
      else if( request.item_type == block_message_type )
      {
        auto in_chain = _chain_index.find( hash );
        if( in_chain != _chain_index.end() )
          delivery = message( block_message( _chain[ in_chain->second ] ) );
      }
      if( delivery )
      {
        send_now( *delivery );
        bytes_delivered += sizeof( message_header ) + delivery->size;
      }
      else if( _withholding )
        _withheld.push_back( item );
      else
        send_now( item_not_available_message( item ) );
    }
    if( _disconnect_after_delivery && bytes_delivered != 0 )
    {
      // the connection is closed only once the items are written, padding aside
      _leaving = true;
      while( _connection->get_total_bytes_sent() < bytes_sent_before + bytes_delivered && !_closed )
        fc::usleep( fc::milliseconds( 1 ) );
      _closed_by_us = true;
      _connection->close_connection();
    }
  }

  /// as many block ids as a node offers in one reply
  static constexpr size_t max_ids_per_reply = 2000;

  fc::thread& _thread;
  const fc::ecc::private_key _key;
  hello_options _hello;
  graphene::net::peer_connection_ptr _connection;
  std::unique_ptr< fc::tcp_server > _server;
  fc::ip::endpoint _listening_endpoint;
  fc::future< void > _accept_task;
  std::vector< fc::future< void > > _delayed;
  bool _destroying = false;
  std::map< uint32_t, handler_t > _handlers;

  mutable std::mutex _mutex; ///< guards what the test thread reads below
  fc::ip::endpoint _local_endpoint;
  std::vector< received_message > _received;
  fc::optional< graphene::net::closing_connection_message > _closing_message;
  fc::optional< graphene::net::connection_rejected_message > _rejection;
  fc::time_point _connected_at;
  fc::time_point _closed_at;

  std::atomic< bool > _connected = false;
  std::atomic< bool > _in_sync = false;
  std::atomic< bool > _closed = false;
  std::atomic< bool > _closed_by_us = false;
  std::atomic< bool > _node_considers_us_in_sync = false;
  bool _close_when_asked = true;
  hive::protocol::block_id_type _told_head;
  std::map< graphene::net::item_hash_t, hive::chain::full_transaction_ptr > _deliverable;
  std::vector< std::shared_ptr< hive::chain::full_block_type > > _chain;
  std::map< hive::protocol::block_id_type, size_t > _chain_index;
  bool _withholding = true;
  std::vector< graphene::net::item_id > _withheld;
  bool _disconnect_after_delivery = false;
  bool _leaving = false;
  std::set< graphene::net::item_hash_t > _advertised_transactions;
  std::set< graphene::net::item_hash_t > _requested;
};

inline void test_peer::answer( const graphene::net::message& received_message )
{
  using namespace graphene::net;
  const bool legacy = _hello.hello_form == hello_options::form::legacy;
  switch( received_message.msg_type )
  {
  case core_message_type_enum::hello_message_type:
    // a peer that predates the IPv6-aware messages doesn't understand this one
    if( !legacy )
      send_now( connection_accepted_message() );
    break;
  case core_message_type_enum::legacy_hello_message_type:
    if( legacy )
      send_now( connection_accepted_message() );
    break;
  case core_message_type_enum::connection_accepted_message_type:
  case core_message_type_enum::connection_rejected_message_type:
  case core_message_type_enum::legacy_connection_rejected_message_type:
    // after a rejection too: the node disconnects once we have exchanged addresses
    send_now( address_request_message() );
    break;
  case core_message_type_enum::address_request_message_type:
    if( legacy )
      send_now( legacy_address_message() );
    else
      send_now( address_message() );
    break;
  case core_message_type_enum::fetch_blockchain_item_ids_message_type:
    answer_fetch_blockchain_item_ids( received_message.as< fetch_blockchain_item_ids_message >() );
    break;
  case core_message_type_enum::current_time_request_message_type:
    send_now( current_time_reply_message( received_message.as< current_time_request_message >().request_sent_time,
                                          fc::time_point::now(), fc::time_point::now() ) );
    break;
  case core_message_type_enum::fetch_items_message_type:
    answer_fetch_items( received_message.as< fetch_items_message >() );
    break;
  case core_message_type_enum::blockchain_item_ids_inventory_message_type:
  {
    const blockchain_item_ids_inventory_message reply = received_message.as< blockchain_item_ids_inventory_message >();
    _node_considers_us_in_sync = reply.total_remaining_item_count == 0 &&
      ( reply.item_hashes_available.empty() ||
        ( reply.item_hashes_available.size() == 1 && reply.item_hashes_available.front() == _told_head ) );
    break;
  }
  case core_message_type_enum::item_ids_inventory_message_type:
  {
    const item_ids_inventory_message inventory = received_message.as< item_ids_inventory_message >();
    if( inventory.item_type == trx_message_type )
      _advertised_transactions.insert( inventory.item_hashes_available.begin(), inventory.item_hashes_available.end() );
    break;
  }
  case core_message_type_enum::closing_connection_message_type:
    if( _close_when_asked && _connection )
    {
      _closed_by_us = true;
      _connection->close_connection();
    }
    break;
  default:
    // the node's own advertisements, firewall checks, legacy duplicates of the messages above...
    break;
  }
}

/**
 * A connection to the node below peer_connection: a plain TCP socket, optionally with the key exchange
 * (stcp_socket), on which the test writes whatever bytes it wants. A task on the peer's thread reads and
 * discards what the node sends, to notice when the node closes the connection.
 */
class raw_connection final
{
public:
  explicit raw_connection( fc::thread& thread ) : _thread( thread ) {}

  ~raw_connection()
  {
    _thread.async( [this]()
    {
      _socket.close();
      if( _read_task.valid() && !_read_task.ready() )
        _read_task.cancel_and_wait( __FUNCTION__ );
    } ).wait();
  }

  /// Connects without the key exchange
  void connect_plain( const fc::ip::endpoint& node_endpoint )
  {
    _thread.async( [&]()
    {
      _socket.get_socket().connect_to( node_endpoint );
      start_reading( false );
    } ).wait();
  }

  /// Connects and does the key exchange, after which writes are encrypted
  void connect_secure( const fc::ip::endpoint& node_endpoint )
  {
    _thread.async( [&]()
    {
      _socket.connect_to( node_endpoint );
      start_reading( true );
    } ).wait();
  }

  /// Writes @p bytes after the key exchange; they are encrypted in blocks of 16, so their size must be a multiple of 16
  void write( const std::vector< char >& bytes )
  {
    _thread.async( [&]() { _socket.write( bytes.data(), bytes.size() ); _socket.flush(); } ).wait();
  }

  /// A message header claiming @p size bytes of type @p msg_type, followed by @p payload, padded to 16 bytes
  static std::vector< char > frame( uint32_t size, uint32_t msg_type, const std::vector< char >& payload = {} )
  {
    graphene::net::message_header header;
    header.size = size;
    header.msg_type = msg_type;
    std::vector< char > bytes( reinterpret_cast< const char* >( &header ), reinterpret_cast< const char* >( &header ) + sizeof( header ) );
    bytes.insert( bytes.end(), payload.begin(), payload.end() );
    bytes.resize( 16 * ( ( bytes.size() + 15 ) / 16 ) );
    return bytes;
  }

  void close() { _thread.async( [&]() { _socket.close(); } ).wait(); }

  /// true once reading from the node failed: it closed the connection (or close() was called)
  bool closed() const { return _closed; }
  fc::time_point closed_at() const
  {
    std::lock_guard< std::mutex > guard( _mutex );
    return _closed_at;
  }
  bool wait_closed( fc::microseconds timeout ) const { return wait_until( [&]() { return closed(); }, timeout ); }
  uint64_t bytes_received() const { return _bytes_received; }

private:
  void start_reading( bool secure )
  {
    _read_task = fc::async( [this, secure]()
    {
      // stcp_socket decrypts in blocks of 16; the plain socket is read as it comes
      char buffer[ 4096 ];
      try
      {
        for( ;; )
        {
          const size_t n = secure ? _socket.readsome( buffer, sizeof( buffer ) ) : _socket.get_socket().readsome( buffer, sizeof( buffer ) );
          _bytes_received += n;
        }
      }
      catch( const fc::canceled_exception& )
      {
        throw;
      }
      catch( const fc::exception& )
      {
        std::lock_guard< std::mutex > guard( _mutex );
        _closed_at = fc::time_point::now();
        _closed = true;
      }
    }, "raw_connection read" );
  }

  fc::thread& _thread;
  graphene::net::stcp_socket _socket;
  fc::future< void > _read_task;
  mutable std::mutex _mutex;
  fc::time_point _closed_at;
  std::atomic< bool > _closed = false;
  std::atomic< uint64_t > _bytes_received = 0;
};

} // namespace p2p_tests
