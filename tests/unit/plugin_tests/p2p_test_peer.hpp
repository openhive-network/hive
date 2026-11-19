#pragma once

#include <boost/test/unit_test.hpp>

#include <hive/chain/full_block.hpp>
#include <hive/chain/full_transaction.hpp>
#include <hive/protocol/types.hpp>

#include <graphene/net/config.hpp>
#include <graphene/net/core_messages.hpp>
#include <graphene/net/peer_connection.hpp>

#include <fc/thread/thread.hpp>

#include <algorithm>
#include <atomic>
#include <cstring>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <vector>

namespace p2p_tests {

/// A p2p peer driven by the test rather than by a node. It completes the handshake as a node that is
/// in sync with the one under test, advertises the transaction hashes it is told to, delivers the
/// transactions it was given and either withholds or reports as unavailable anything else it is asked for.
/// It records the items the node asks it for and the transactions the node advertises to it. It can also ask
/// the node for blocks, and records the blocks it receives and the ones reported unavailable. Its connection
/// lives on @p thread, like a node's connections live on the p2p thread.
class test_peer final : public graphene::net::peer_connection_delegate
{
public:
  test_peer( fc::thread& thread, const hive::protocol::chain_id_type& chain_id, const std::string& user_agent )
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

  void provide( const std::vector< hive::chain::full_transaction_ptr >& transactions )
  {
    _thread.async( [&]()
    {
      for( const hive::chain::full_transaction_ptr& transaction : transactions )
        _deliverable[ transaction->get_legacy_transaction_message_hash() ] = transaction;
    } ).wait();
  }

  void advertise( const std::vector< graphene::net::item_hash_t >& hashes )
  {
    _thread.async( [&]()
    {
      // a connection closes when more than GRAPHENE_NET_MAXIMUM_QUEUED_MESSAGES_IN_BYTES wait to be sent,
      // so a flood goes out in several messages, each one sent before the next is queued
      const size_t max_hashes_per_message = GRAPHENE_NET_MAXIMUM_QUEUED_MESSAGES_IN_BYTES / 2 / sizeof( graphene::net::item_hash_t );
      for( size_t begin = 0; begin < hashes.size() && !_closed; begin += max_hashes_per_message )
      {
        const size_t end = std::min( hashes.size(), begin + max_hashes_per_message );
        const graphene::net::message inventory( graphene::net::item_ids_inventory_message( graphene::net::trx_message_type,
          std::vector< graphene::net::item_hash_t >( hashes.begin() + begin, hashes.begin() + end ) ) );
        const uint64_t bytes_sent_before = _connection->get_total_bytes_sent();
        _connection->send_message( inventory );
        while( _connection->get_total_bytes_sent() < bytes_sent_before + inventory.size && !_closed )
          fc::usleep( fc::milliseconds( 1 ) );
      }
    } ).wait();
  }

  void request_blocks( const std::vector< graphene::net::item_hash_t >& ids )
  {
    _thread.async( [&]()
    {
      _connection->send_message( graphene::net::fetch_items_message( graphene::net::block_message_type, ids ) );
    } ).wait();
  }

  /// ids of the blocks received so far, in the order they arrived
  std::vector< hive::protocol::block_id_type > blocks_received() const
  {
    std::lock_guard< std::mutex > guard( _blocks_mutex );
    return _blocks_received;
  }

  /// the requested block ids the node reported as not available, in the order it did
  std::vector< graphene::net::item_hash_t > blocks_not_available() const
  {
    std::lock_guard< std::mutex > guard( _blocks_mutex );
    return _blocks_not_available;
  }

  /// from now on the connection is closed as soon as the transactions requested in one message are sent;
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
    case core_message_type_enum::block_message_type:
    case core_message_type_enum::compressed_block_message_type:
    {
      const hive::protocol::block_id_type id = id_of_received_block( received_message );
      std::lock_guard< std::mutex > guard( _blocks_mutex );
      _blocks_received.push_back( id );
      break;
    }
    case core_message_type_enum::item_not_available_message_type:
    {
      const item_not_available_message reply = received_message.as< item_not_available_message >();
      if( reply.requested_item.item_type == block_message_type )
      {
        std::lock_guard< std::mutex > guard( _blocks_mutex );
        _blocks_not_available.push_back( reply.requested_item.item_hash );
      }
      break;
    }
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
    if( _leaving )
      return;
    const uint64_t bytes_sent_before = _connection->get_total_bytes_sent();
    uint64_t bytes_delivered = 0;
    for( const graphene::net::item_hash_t& hash : request.items_to_fetch )
    {
      const graphene::net::item_id item( request.item_type, hash );
      _requested.insert( hash );
      auto deliverable = _deliverable.find( hash );
      if( request.item_type == graphene::net::trx_message_type && deliverable != _deliverable.end() )
      {
        const graphene::net::message delivery( graphene::net::trx_message( deliverable->second ) );
        _connection->send_message( delivery );
        bytes_delivered += sizeof( graphene::net::message_header ) + delivery.size;
      }
      else if( _withholding )
        _withheld.push_back( item );
      else
        _connection->send_message( graphene::net::item_not_available_message( item ) );
    }
    if( _disconnect_after_delivery && bytes_delivered != 0 )
    {
      // the connection is closed only once the transactions are written, padding aside
      _leaving = true;
      while( _connection->get_total_bytes_sent() < bytes_sent_before + bytes_delivered && !_closed )
        fc::usleep( fc::milliseconds( 1 ) );
      _connection->close_connection();
      _closed = true;
    }
  }

  fc::thread& _thread;
  const hive::protocol::chain_id_type _chain_id;
  const std::string _user_agent;
  const fc::ecc::private_key _key;
  graphene::net::peer_connection_ptr _connection;
  fc::ip::endpoint _local_endpoint;
  std::atomic< bool > _in_sync = false;
  std::atomic< bool > _closed = false;
  std::atomic< bool > _node_considers_us_in_sync = false;
  hive::protocol::block_id_type _told_head;
  std::map< graphene::net::item_hash_t, hive::chain::full_transaction_ptr > _deliverable;
  bool _withholding = true;
  std::vector< graphene::net::item_id > _withheld;
  bool _disconnect_after_delivery = false;
  bool _leaving = false;
  std::set< graphene::net::item_hash_t > _advertised_transactions;
  std::set< graphene::net::item_hash_t > _requested;
  mutable std::mutex _blocks_mutex;
  std::vector< hive::protocol::block_id_type > _blocks_received;
  std::vector< graphene::net::item_hash_t > _blocks_not_available;
};

} // namespace p2p_tests
