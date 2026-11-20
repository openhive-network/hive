#include <boost/test/unit_test.hpp>

#include <graphene/net/config.hpp>
#include <graphene/net/core_messages.hpp>
#include <graphene/net/peer_connection.hpp>

#include <hive/chain/full_block.hpp>
#include <hive/chain/full_transaction.hpp>
#include <hive/protocol/config.hpp>
#include <hive/protocol/hive_operations.hpp>

#include <fc/asio.hpp>
#include <fc/crypto/aes.hpp>
#include <fc/crypto/city.hpp>
#include <fc/crypto/sha256.hpp>
#include <fc/network/ip.hpp>
#include <fc/network/tcp_socket.hpp>
#include <fc/network/tcp_socket_io_hooks.hpp>
#include <fc/thread/thread.hpp>

#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <vector>

using graphene::net::message;
using graphene::net::peer_connection;
using graphene::net::peer_connection_ptr;

namespace {

/// Collects whatever a peer_connection receives.
class recording_delegate : public graphene::net::peer_connection_delegate
{
public:
  std::vector<message> received;
  std::vector<fc::time_point> received_at;
  bool closed = false;
  size_t batched_writes = 0;
  size_t messages_written = 0;
  size_t bytes_written = 0;
  /// the blocks get_block_to_send() serves, by id
  std::map<graphene::net::item_hash_t, std::shared_ptr<hive::chain::full_block_type>> blocks_to_send;

  void on_message( peer_connection*, const message& received_message ) override
  {
    received.push_back( received_message );
    received_at.push_back( fc::time_point::now() );
  }
  void on_connection_closed( peer_connection* ) override { closed = true; }
  std::shared_ptr<hive::chain::full_block_type> get_full_block_by_block_id( const hive::protocol::block_id_type& ) override { return {}; }
  std::shared_ptr<hive::chain::full_block_type> get_block_to_send( peer_connection*, const graphene::net::item_hash_t& requested_item ) override
  {
    const auto found = blocks_to_send.find( requested_item );
    return found == blocks_to_send.end() ? nullptr : found->second;
  }
  void on_messages_written( size_t message_count, size_t byte_count ) override
  {
    ++batched_writes;
    messages_written += message_count;
    bytes_written += byte_count;
  }
};

/// Counts the writes a tcp_socket hands to the operating system.
class write_counting_hooks : public fc::tcp_socket_io_hooks
{
public:
  size_t writes = 0;

  size_t readsome( boost::asio::ip::tcp::socket& socket, char* buffer, size_t length ) override
  {
    return fc::asio::read_some( socket, buffer, length ).wait();
  }
  size_t readsome( boost::asio::ip::tcp::socket& socket, const std::shared_ptr<char>& buffer, size_t length, size_t offset ) override
  {
    return fc::asio::read_some( socket, buffer, length, offset ).wait();
  }
  size_t writesome( boost::asio::ip::tcp::socket& socket, const char* buffer, size_t length ) override
  {
    ++writes;
    return fc::asio::write_some( socket, buffer, length ).wait();
  }
  size_t writesome( boost::asio::ip::tcp::socket& socket, const std::shared_ptr<const char>& buffer, size_t length, size_t offset ) override
  {
    ++writes;
    return fc::asio::write_some( socket, buffer, length, offset ).wait();
  }
};

message make_message( uint32_t msg_type, size_t size, char fill )
{
  message m;
  m.msg_type = msg_type;
  m.data.resize( size );
  for( size_t i = 0; i < size; ++i )
    m.data[i] = char( fill + i % 7 );
  m.size = static_cast<uint32_t>( size );
  return m;
}

/// A block whose serialized form is well above the batch bound.
std::shared_ptr<hive::chain::full_block_type> make_large_block()
{
  hive::protocol::custom_json_operation op;
  op.required_posting_auths.insert( "alice" );
  op.id = "batched_send_test";
  op.json = "\"" + std::string( 150 * 1024, 'x' ) + "\"";
  hive::protocol::signed_transaction tx;
  tx.operations.push_back( op );
  hive::protocol::signed_block block;
  block.witness = "initminer";
  block.transactions.push_back( tx );
  return hive::chain::full_block_type::create_from_signed_block( block );
}

bool same_message( const message& a, const message& b )
{
  return a.msg_type == b.msg_type && a.size == b.size && a.data == b.data;
}

/// Two peer_connections joined over loopback, with the sender's socket writes counted.
struct connected_pair
{
  recording_delegate sender_delegate;
  recording_delegate receiver_delegate;
  peer_connection_ptr sender = peer_connection::make_shared( &sender_delegate );
  peer_connection_ptr receiver = peer_connection::make_shared( &receiver_delegate );
  write_counting_hooks hooks;

  connected_pair()
  {
    fc::tcp_server server;
    server.listen( fc::ip::endpoint( fc::ip::address( "127.0.0.1" ), 0 ) );
    fc::future<void> accepted = fc::async( [&]() {
      server.accept( receiver->get_socket() );
      receiver->accept_connection();
    }, "batched_send_test accept" );
    sender->connect_to( fc::ip::endpoint( fc::ip::address( "127.0.0.1" ), server.get_port() ) );
    accepted.wait();
    sender->get_socket().set_io_hooks( &hooks );
  }

  ~connected_pair()
  {
    sender->get_socket().set_io_hooks( nullptr );
    sender->close_connection();
    receiver->close_connection();
    sender->destroy_connection( __FUNCTION__ );
    receiver->destroy_connection( __FUNCTION__ );
  }

  /// queues @p block for the receiver, as a node answers a block request: by id, looked up shortly before it is sent
  void send_block( const std::shared_ptr<hive::chain::full_block_type>& block )
  {
    sender_delegate.blocks_to_send[ block->get_block_id() ] = block;
    sender->send_block_message( block->get_block_id() );
  }

  /// waits until the receiver has @p count messages and the sender has accounted for the writes that carried them:
  /// the receiver can handle a message before the sender's write of it returns
  void wait_for_messages( size_t count, fc::microseconds timeout = fc::seconds( 30 ) )
  {
    const fc::time_point deadline = fc::time_point::now() + timeout;
    while( ( receiver_delegate.received.size() < count || sender_delegate.messages_written < receiver_delegate.received.size() ) &&
           !receiver_delegate.closed && fc::time_point::now() < deadline )
      fc::usleep( fc::milliseconds( 1 ) );
  }
};

const fc::microseconds long_flush_delay = fc::seconds( 3 );

message make_transaction_message( char fill )
{
  return make_message( graphene::net::trx_message_type, 200, fill );
}

/// A transaction whose serialization is not a multiple of 16 bytes, so its message is padded on the wire.
std::shared_ptr<hive::chain::full_transaction_type> make_full_transaction( size_t json_size )
{
  hive::protocol::custom_json_operation op;
  op.required_posting_auths.insert( "alice" );
  op.id = "batched_send_test";
  op.json = "\"" + std::string( json_size, 't' ) + "\"";
  hive::protocol::signed_transaction tx;
  tx.ref_block_num = 1;
  tx.operations.push_back( op );
  return hive::chain::full_transaction_type::create_from_signed_transaction( tx, hive::protocol::pack_type::hf26, false );
}

std::shared_ptr<hive::chain::full_block_type> make_small_block()
{
  hive::protocol::signed_block block;
  block.witness = "initminer";
  return hive::chain::full_block_type::create_from_signed_block( block );
}

/// A queued message that cannot be built, as a block whose compression fails; counts the attempts.
struct unbuildable_queued_message : peer_connection::queued_message
{
  size_t& build_attempts;
  size_t size_in_queue;

  unbuildable_queued_message( size_t& build_attempts, size_t size_in_queue ) :
    build_attempts( build_attempts ), size_in_queue( size_in_queue )
  {
    may_wait_for_batch = true;
  }
  message get_message( graphene::net::peer_connection_delegate*, peer_connection* ) override
  {
    ++build_attempts;
    FC_THROW( "test message cannot be built" );
  }
  size_t get_size_in_queue() override { return size_in_queue; }
};

/// A queued message whose building blocks until the send task is canceled; records how that ended.
struct blocking_queued_message : peer_connection::queued_message
{
  size_t& build_attempts;
  bool& saw_cancel;

  blocking_queued_message( size_t& build_attempts, bool& saw_cancel ) :
    build_attempts( build_attempts ), saw_cancel( saw_cancel )
  {}
  message get_message( graphene::net::peer_connection_delegate*, peer_connection* ) override
  {
    ++build_attempts;
    try
    {
      fc::usleep( fc::seconds( 20 ) );
    }
    catch( const fc::canceled_exception& )
    {
      saw_cancel = true;
      throw;
    }
    return make_message( 5000, 16, 'x' );
  }
  size_t get_size_in_queue() override { return 16; }
};

/// A message that alone takes the whole queue limit, so queueing it closes the connection unless the queue is empty.
struct queue_limit_sized_message : peer_connection::queued_message
{
  message message_to_send;

  explicit queue_limit_sized_message( message message_to_send ) : message_to_send( std::move( message_to_send ) ) {}
  message get_message( graphene::net::peer_connection_delegate*, peer_connection* ) override { return message_to_send; }
  size_t get_size_in_queue() override { return GRAPHENE_NET_MAXIMUM_QUEUED_MESSAGES_IN_BYTES; }
};

} // namespace

BOOST_AUTO_TEST_SUITE( p2p_batched_send_tests )

/// The send cipher is one CBC stream, so the chunk size used to encrypt it does not change the bytes
/// on the wire; this is what lets nodes with different stcp_socket write chunk sizes talk to each other.
BOOST_AUTO_TEST_CASE( send_cipher_output_does_not_depend_on_chunk_size )
{
  const fc::sha256 key = fc::sha256::hash( std::string( "batched send key" ) );
  const fc::uint128 iv = fc::city_hash_crc_128( (const char*)&key, sizeof( key ) );
  std::vector<char> plaintext( 200 * 1024 );
  for( size_t i = 0; i < plaintext.size(); ++i )
    plaintext[i] = char( i * 31 + 7 );

  auto encrypt_in_chunks = [&]( size_t chunk_size ) {
    fc::aes_encoder encoder;
    encoder.init( key, iv );
    std::vector<char> ciphertext( plaintext.size() );
    for( size_t offset = 0; offset < plaintext.size(); offset += chunk_size )
    {
      const size_t len = std::min( chunk_size, plaintext.size() - offset );
      encoder.encode( plaintext.data() + offset, static_cast<uint32_t>( len ), ciphertext.data() + offset );
    }
    return ciphertext;
  };

  const std::vector<char> in_4k_chunks = encrypt_in_chunks( 4096 );
  BOOST_CHECK( encrypt_in_chunks( 64 * 1024 ) == in_4k_chunks );
  BOOST_CHECK( encrypt_in_chunks( plaintext.size() ) == in_4k_chunks );
}

/// Messages queued on a peer_connection together arrive intact and in order, a lazily built block above
/// the batch bound included, and go out in far fewer socket writes than there are messages.
BOOST_AUTO_TEST_CASE( queued_messages_arrive_intact_and_in_order_in_few_writes )
{
  connected_pair pair;

  const std::shared_ptr<hive::chain::full_block_type> block = make_large_block();
  std::vector<message> expected;
  for( size_t i = 0; i < 100; ++i )
    expected.push_back( make_message( 5000 + i, 1 + ( i * 37 ) % 300, char( 'a' + i % 26 ) ) );
  expected.push_back( make_message( 5100, 20 * 1024, 'm' ) );
  expected.push_back( message( graphene::net::block_message( block ) ) );
  BOOST_REQUIRE_GT( expected.back().size, 64u * 1024u );
  for( size_t i = 0; i < 100; ++i )
    expected.push_back( make_message( 5200 + i, 16 + i, char( 'A' + i % 26 ) ) );

  // everything is queued before the send task gets to run, as when a node relays a burst to a peer
  for( const message& m : expected )
  {
    if( m.msg_type == graphene::net::block_message_type )
      pair.send_block( block );
    else
      pair.sender->send_message( m );
  }

  pair.wait_for_messages( expected.size() );

  BOOST_REQUIRE_EQUAL( pair.receiver_delegate.received.size(), expected.size() );
  for( size_t i = 0; i < expected.size(); ++i )
    BOOST_CHECK_MESSAGE( same_message( pair.receiver_delegate.received[i], expected[i] ), "message " << i << " differs" );

  // one message per write would be at least one write per message, plus extra writes for the block
  BOOST_CHECK_LT( pair.hooks.writes, expected.size() / 4 );
  // the delegate hears of every batch, with the messages and bytes (headers included) it carried
  size_t expected_bytes = 0;
  for( const message& m : expected )
    expected_bytes += sizeof( graphene::net::message_header ) + m.size;
  BOOST_CHECK_EQUAL( pair.sender_delegate.messages_written, expected.size() );
  BOOST_CHECK_EQUAL( pair.sender_delegate.bytes_written, expected_bytes );
  BOOST_CHECK_GE( pair.sender_delegate.batched_writes, 2u );
  BOOST_CHECK_LE( pair.sender_delegate.batched_writes, pair.hooks.writes );
  BOOST_CHECK_EQUAL( pair.sender->get_total_bytes_sent(), pair.receiver->get_total_bytes_received() );
}

/// A transaction sent from its shared serialization arrives as the same bytes as a trx_message built for
/// the peer, to every peer it is sent to and with the same byte accounting, padding included.
BOOST_AUTO_TEST_CASE( shared_transaction_is_sent_as_the_same_bytes_as_a_trx_message )
{
  std::vector<std::shared_ptr<hive::chain::full_transaction_type>> transactions;
  for( size_t json_size : { 1, 17, 300, 5000 } )
    transactions.push_back( make_full_transaction( json_size ) );
  BOOST_REQUIRE( std::any_of( transactions.begin(), transactions.end(), []( const auto& transaction ) {
    return ( sizeof( graphene::net::message_header ) + transaction->get_transaction_size() ) % 16 != 0; } ) );

  std::vector<message> expected;
  for( const auto& transaction : transactions )
  {
    expected.push_back( message( graphene::net::trx_message( transaction ) ) );
    expected.push_back( make_message( 5000, 3, 'q' ) );
  }

  connected_pair copying_pair;
  for( const message& m : expected )
    copying_pair.sender->send_message( m );
  copying_pair.wait_for_messages( expected.size() );
  BOOST_REQUIRE_EQUAL( copying_pair.receiver_delegate.received.size(), expected.size() );

  for( size_t peer = 0; peer < 3; ++peer )
  {
    BOOST_TEST_CONTEXT( "peer " << peer )
    {
      connected_pair pair;
      for( size_t i = 0; i < transactions.size(); ++i )
      {
        pair.sender->send_transaction_message( transactions[i] );
        pair.sender->send_message( expected[2 * i + 1] );
      }
      pair.wait_for_messages( expected.size() );

      BOOST_REQUIRE_EQUAL( pair.receiver_delegate.received.size(), expected.size() );
      for( size_t i = 0; i < expected.size(); ++i )
        BOOST_CHECK_MESSAGE( same_message( pair.receiver_delegate.received[i], expected[i] ), "message " << i << " differs" );
      BOOST_CHECK_EQUAL( pair.sender_delegate.messages_written, copying_pair.sender_delegate.messages_written );
      BOOST_CHECK_EQUAL( pair.sender_delegate.bytes_written, copying_pair.sender_delegate.bytes_written );
      BOOST_CHECK_EQUAL( pair.sender->get_total_bytes_sent(), copying_pair.sender->get_total_bytes_sent() );
      BOOST_CHECK_EQUAL( pair.sender->get_total_bytes_sent(), pair.receiver->get_total_bytes_received() );
    }
  }
}

/// A shared transaction is transaction traffic: it waits for the flush delay and shares one write with the rest.
BOOST_AUTO_TEST_CASE( shared_transaction_waits_for_the_flush_delay )
{
  connected_pair pair;
  const fc::microseconds flush_delay = fc::milliseconds( 300 );
  pair.sender->set_send_flush_delay( flush_delay );

  const std::shared_ptr<hive::chain::full_transaction_type> transaction = make_full_transaction( 100 );
  const fc::time_point queued_at = fc::time_point::now();
  pair.sender->send_transaction_message( transaction );
  fc::usleep( fc::milliseconds( 20 ) );
  pair.sender->send_message( make_transaction_message( 'b' ) );
  pair.wait_for_messages( 2 );

  BOOST_REQUIRE_EQUAL( pair.receiver_delegate.received.size(), 2u );
  BOOST_CHECK( same_message( pair.receiver_delegate.received[0], message( graphene::net::trx_message( transaction ) ) ) );
  BOOST_CHECK_GE( ( pair.receiver_delegate.received_at.front() - queued_at ).count(), flush_delay.count() );
  BOOST_CHECK_EQUAL( pair.hooks.writes, 1u );
}

/// A message queued with a send time field goes out with the time it was written patched in.
BOOST_AUTO_TEST_CASE( send_time_field_is_patched_when_written )
{
  connected_pair pair;

  const fc::time_point queued_at = fc::time_point::now();
  pair.sender->send_message( message( graphene::net::current_time_request_message( fc::time_point() ) ),
                             offsetof( graphene::net::current_time_request_message, request_sent_time ) );
  pair.wait_for_messages( 1 );

  BOOST_REQUIRE_EQUAL( pair.receiver_delegate.received.size(), 1u );
  const graphene::net::current_time_request_message request =
    pair.receiver_delegate.received[0].as<graphene::net::current_time_request_message>();
  BOOST_CHECK( request.request_sent_time >= queued_at );
  BOOST_CHECK( request.request_sent_time <= pair.receiver_delegate.received_at[0] );
}

/// With a flush delay, transaction traffic waits for the delay and then goes out together in one write.
BOOST_AUTO_TEST_CASE( transaction_traffic_waits_for_the_flush_delay )
{
  connected_pair pair;
  const fc::microseconds flush_delay = fc::milliseconds( 300 );
  pair.sender->set_send_flush_delay( flush_delay );

  std::vector<message> expected;
  expected.push_back( make_transaction_message( 'a' ) );
  expected.push_back( message( graphene::net::item_ids_inventory_message( graphene::net::trx_message_type,
    { graphene::net::item_hash_t( std::string( "0102030405060708090a0b0c0d0e0f1011121314" ) ) } ) ) );
  expected.push_back( message( graphene::net::fetch_items_message( graphene::net::trx_message_type,
    { graphene::net::item_hash_t( std::string( "1112131415161718191a1b1c1d1e1f2021222324" ) ) } ) ) );

  const fc::time_point queued_at = fc::time_point::now();
  pair.sender->send_message( expected[0] );
  // the send task runs between these, as it would while a node handles the next incoming transaction
  fc::usleep( fc::milliseconds( 20 ) );
  pair.sender->send_message( expected[1] );
  pair.sender->send_message( expected[2] );
  pair.wait_for_messages( expected.size() );

  BOOST_REQUIRE_EQUAL( pair.receiver_delegate.received.size(), expected.size() );
  for( size_t i = 0; i < expected.size(); ++i )
    BOOST_CHECK_MESSAGE( same_message( pair.receiver_delegate.received[i], expected[i] ), "message " << i << " differs" );
  BOOST_CHECK_GE( ( pair.receiver_delegate.received_at.front() - queued_at ).count(), flush_delay.count() );
  BOOST_CHECK_EQUAL( pair.hooks.writes, 1u );
}

/// A block queued after transactions goes out ahead of them, and the send task does not wait at all.
BOOST_AUTO_TEST_CASE( queued_block_is_sent_without_the_flush_delay )
{
  connected_pair pair;
  pair.sender->set_send_flush_delay( long_flush_delay );

  const std::shared_ptr<hive::chain::full_block_type> block = make_small_block();
  const fc::time_point queued_at = fc::time_point::now();
  pair.sender->send_message( make_transaction_message( 'a' ) );
  pair.sender->send_message( make_transaction_message( 'b' ) );
  pair.send_block( block );
  pair.wait_for_messages( 3 );

  BOOST_REQUIRE_EQUAL( pair.receiver_delegate.received.size(), 3u );
  BOOST_CHECK_EQUAL( pair.receiver_delegate.received.front().msg_type, uint32_t( graphene::net::block_message_type ) );
  BOOST_CHECK_LT( ( pair.receiver_delegate.received_at.back() - queued_at ).count(), long_flush_delay.count() / 2 );
}

/// Blocks and block inventories go out ahead of the transaction traffic queued before them, in the order they were
/// queued; transaction traffic keeps its order, and so does everything else, which a block passes only where it
/// waits behind transactions.
BOOST_AUTO_TEST_CASE( block_traffic_is_written_ahead_of_queued_transaction_traffic )
{
  for( fc::microseconds flush_delay : { fc::microseconds( 0 ), long_flush_delay } )
  {
    BOOST_TEST_CONTEXT( "flush delay " << flush_delay.count() )
    {
      connected_pair pair;
      pair.sender->set_send_flush_delay( flush_delay );

      const auto hash = []( const char* seed ) { return fc::ripemd160::hash( std::string( seed ) ); };
      const auto block_inventory = [&]( const char* seed ) {
        return message( graphene::net::item_ids_inventory_message( graphene::net::block_message_type, { hash( seed ) } ) );
      };
      const auto not_available = [&]( const char* seed ) {
        return message( graphene::net::item_not_available_message( graphene::net::item_id( graphene::net::trx_message_type, hash( seed ) ) ) );
      };
      const message sync_request( graphene::net::fetch_blockchain_item_ids_message( graphene::net::block_message_type, {} ) );
      const message transaction_a = make_transaction_message( 'a' );
      const message transaction_inventory_b( graphene::net::item_ids_inventory_message( graphene::net::trx_message_type, { hash( "b" ) } ) );
      const message transaction_c = make_transaction_message( 'c' );
      const message transaction_request_d( graphene::net::fetch_items_message( graphene::net::trx_message_type, { hash( "d" ) } ) );
      const std::shared_ptr<hive::chain::full_block_type> block = make_small_block();
      const message block_reply = message( graphene::net::block_message( block ) );

      // everything is queued before the send task gets to run, as while a node handles one incoming message
      pair.sender->send_message( sync_request );
      pair.sender->send_message( transaction_a );
      pair.sender->send_message( not_available( "1" ) );
      pair.sender->send_message( transaction_inventory_b );
      pair.sender->send_message( block_inventory( "first" ) );
      pair.sender->send_message( not_available( "2" ) );
      pair.sender->send_message( transaction_c );
      pair.send_block( block );
      pair.sender->send_message( transaction_request_d );
      pair.sender->send_message( block_inventory( "second" ) );

      const std::vector<message> expected = {
        sync_request, block_inventory( "first" ), block_reply, block_inventory( "second" ),
        transaction_a, not_available( "1" ), transaction_inventory_b, not_available( "2" ), transaction_c, transaction_request_d };
      pair.wait_for_messages( expected.size() );

      BOOST_REQUIRE_EQUAL( pair.receiver_delegate.received.size(), expected.size() );
      for( size_t i = 0; i < expected.size(); ++i )
        BOOST_CHECK_MESSAGE( same_message( pair.receiver_delegate.received[i], expected[i] ), "message " << i << " differs" );

      // with the queue drained, block traffic queued after transactions is written ahead of them again
      pair.sender->send_message( transaction_a );
      pair.sender->send_message( block_inventory( "third" ) );
      pair.wait_for_messages( expected.size() + 2 );
      BOOST_REQUIRE_EQUAL( pair.receiver_delegate.received.size(), expected.size() + 2 );
      BOOST_CHECK( same_message( pair.receiver_delegate.received[expected.size()], block_inventory( "third" ) ) );
      BOOST_CHECK( same_message( pair.receiver_delegate.received[expected.size() + 1], transaction_a ) );
    }
  }
}

/// A block, or any other message that is not transaction traffic, queued while the send task waits ends the wait.
BOOST_AUTO_TEST_CASE( message_arriving_during_the_wait_ends_it )
{
  for( bool send_block : { true, false } )
  {
    BOOST_TEST_CONTEXT( ( send_block ? "block" : "sync request" ) )
    {
      connected_pair pair;
      pair.sender->set_send_flush_delay( long_flush_delay );

      const fc::time_point queued_at = fc::time_point::now();
      pair.sender->send_message( make_transaction_message( 'a' ) );
      fc::usleep( fc::milliseconds( 100 ) );
      BOOST_REQUIRE( pair.receiver_delegate.received.empty() );

      if( send_block )
        pair.send_block( make_small_block() );
      else
        pair.sender->send_message( message( graphene::net::fetch_blockchain_item_ids_message( graphene::net::block_message_type, {} ) ) );
      pair.wait_for_messages( 2 );

      BOOST_REQUIRE_EQUAL( pair.receiver_delegate.received.size(), 2u );
      // a block goes ahead of the waiting transaction, a sync request behind it
      BOOST_CHECK_EQUAL( pair.receiver_delegate.received.back().msg_type,
                         uint32_t( send_block ? graphene::net::trx_message_type : graphene::net::fetch_blockchain_item_ids_message_type ) );
      BOOST_CHECK_LT( ( pair.receiver_delegate.received_at.back() - queued_at ).count(), long_flush_delay.count() / 2 );
      BOOST_CHECK_EQUAL( pair.hooks.writes, 1u );
    }
  }
}

/// Transaction traffic that fills a batch is written without waiting for the delay.
BOOST_AUTO_TEST_CASE( full_batch_ends_the_wait )
{
  connected_pair pair;
  pair.sender->set_send_flush_delay( long_flush_delay );

  const fc::time_point queued_at = fc::time_point::now();
  pair.sender->send_message( make_transaction_message( 'a' ) );
  fc::usleep( fc::milliseconds( 100 ) );
  pair.sender->send_message( make_message( graphene::net::trx_message_type, 64 * 1024, 'b' ) );
  pair.wait_for_messages( 2 );

  BOOST_REQUIRE_EQUAL( pair.receiver_delegate.received.size(), 2u );
  BOOST_CHECK_LT( ( pair.receiver_delegate.received_at.back() - queued_at ).count(), long_flush_delay.count() / 2 );
}

/// Closing a connection whose send task is waiting does not wait for the delay.
BOOST_AUTO_TEST_CASE( destroying_a_waiting_connection_does_not_wait_for_the_delay )
{
  const fc::time_point started_at = fc::time_point::now();
  {
    connected_pair pair;
    pair.sender->set_send_flush_delay( fc::seconds( 20 ) );
    pair.sender->send_message( make_transaction_message( 'a' ) );
    fc::usleep( fc::milliseconds( 50 ) );
  }
  BOOST_CHECK_LT( ( fc::time_point::now() - started_at ).count(), fc::seconds( 10 ).count() );
}

/// A queued message that cannot be built is dropped on its own: the messages around it are delivered,
/// with or without a flush delay, and its queue space is given back.
BOOST_AUTO_TEST_CASE( message_that_cannot_be_built_is_dropped_alone )
{
  for( fc::microseconds flush_delay : { fc::microseconds( 0 ), fc::milliseconds( 300 ) } )
  {
    BOOST_TEST_CONTEXT( "flush delay " << flush_delay.count() << "us" )
    {
      connected_pair pair;
      pair.sender->set_send_flush_delay( flush_delay );

      size_t build_attempts = 0;
      const message before = make_transaction_message( 'a' );
      const message after = make_transaction_message( 'c' );
      pair.sender->send_message( before );
      // small enough that the batch still waits for the flush delay
      pair.sender->send_queueable_message( std::make_unique<unbuildable_queued_message>( build_attempts, 1000 ) );
      pair.sender->send_message( after );
      pair.wait_for_messages( 2 );

      BOOST_CHECK_EQUAL( build_attempts, 1u );
      BOOST_REQUIRE_EQUAL( pair.receiver_delegate.received.size(), 2u );
      BOOST_CHECK( same_message( pair.receiver_delegate.received[0], before ) );
      BOOST_CHECK( same_message( pair.receiver_delegate.received[1], after ) );

      // the send task gives the queue space back once its write returns
      fc::usleep( fc::milliseconds( 50 ) );
      const message probe = make_message( 5000, 16, 'p' );
      pair.sender->send_queueable_message( std::make_unique<queue_limit_sized_message>( probe ) );
      pair.wait_for_messages( 3 );

      BOOST_CHECK( !pair.sender_delegate.closed );
      BOOST_CHECK( !pair.receiver_delegate.closed );
      BOOST_REQUIRE_EQUAL( pair.receiver_delegate.received.size(), 3u );
      BOOST_CHECK( same_message( pair.receiver_delegate.received[2], probe ) );
    }
  }
}

/// Messages that cannot be built, again and again, do not fill the send queue and close the connection.
BOOST_AUTO_TEST_CASE( repeated_build_failures_do_not_overflow_the_send_queue )
{
  connected_pair pair;

  const size_t rounds = 1000;
  // three of these left in the queue's size would exceed its limit
  const size_t unbuildable_message_size = GRAPHENE_NET_MAXIMUM_QUEUED_MESSAGES_IN_BYTES * 2 / 5;
  size_t build_attempts = 0;
  for( size_t i = 0; i < rounds; ++i )
  {
    pair.sender->send_queueable_message( std::make_unique<unbuildable_queued_message>( build_attempts, unbuildable_message_size ) );
    pair.sender->send_message( make_message( 5000, 16, char( 'a' + i % 26 ) ) );
    pair.wait_for_messages( i + 1 );
    BOOST_REQUIRE_EQUAL( pair.receiver_delegate.received.size(), i + 1 );
  }

  BOOST_CHECK_EQUAL( build_attempts, rounds );
  BOOST_CHECK( !pair.sender_delegate.closed );
  BOOST_CHECK( !pair.receiver_delegate.closed );
}

/// Replies to a request for far more transaction bytes than the send queue holds, all queued at once, wait for
/// the queue to drain instead of closing the connection: every one arrives, in order, and a block queued after
/// them overtakes the ones still waiting.  Only replies past the number allowed to wait are refused.
BOOST_AUTO_TEST_CASE( transaction_replies_over_the_queue_limit_wait_for_it_to_drain )
{
  connected_pair pair;

  const std::shared_ptr<hive::chain::full_transaction_type> transaction = make_full_transaction( 8000 );
  const size_t transaction_size = transaction->get_transaction_size();
  BOOST_REQUIRE_GT( transaction_size, 8000u );
  // everything is queued before the send task gets to run, as when a node answers one fetch request
  size_t accepted = 0;
  while( pair.sender->send_transaction_reply( transaction ) )
    ++accepted;
  const size_t queued_at_once = GRAPHENE_NET_MAXIMUM_QUEUED_TRANSACTION_REPLY_BYTES / transaction_size;
  // whichever bound on the waiting ones is reached first: their bytes, for transactions of this size
  const size_t may_wait = std::min<size_t>( GRAPHENE_NET_MAX_DEFERRED_TRANSACTION_REPLIES_PER_PEER,
                                            GRAPHENE_NET_MAX_DEFERRED_TRANSACTION_REPLY_BYTES_PER_PEER / transaction_size );
  BOOST_CHECK_EQUAL( accepted, queued_at_once + may_wait );
  BOOST_REQUIRE_GT( accepted * transaction_size, 4u * GRAPHENE_NET_MAXIMUM_QUEUED_MESSAGES_IN_BYTES );
  pair.send_block( make_small_block() );

  pair.wait_for_messages( accepted + 1 );

  BOOST_CHECK( !pair.sender_delegate.closed );
  BOOST_CHECK( !pair.receiver_delegate.closed );
  BOOST_REQUIRE_EQUAL( pair.receiver_delegate.received.size(), accepted + 1 );
  const message expected_reply = message( graphene::net::trx_message( transaction ) );
  size_t replies = 0;
  size_t replies_before_block = 0;
  for( const message& received : pair.receiver_delegate.received )
  {
    if( received.msg_type == graphene::net::block_message_type )
      replies_before_block = replies;
    else if( same_message( received, expected_reply ) )
      ++replies;
  }
  BOOST_CHECK_EQUAL( replies, accepted );
  // the block goes ahead of the queued replies, and the waiting ones are queued behind it
  BOOST_CHECK_EQUAL( replies_before_block, 0u );

  // with the replies sent, as many may wait again
  for( size_t i = 0; i < may_wait; ++i )
    BOOST_REQUIRE( pair.sender->send_transaction_reply( transaction ) );
}

/// Replies wait for the queue in a bounded number of bytes, whatever the size of the transactions: past it, and past
/// the count bound for small ones, the rest are refused.  While they wait, the connection reports when the queue last
/// made progress towards the peer, which is what a peer that stops reading is dropped on.
BOOST_AUTO_TEST_CASE( waiting_transaction_replies_are_bounded_in_bytes )
{
  const size_t byte_bound = GRAPHENE_NET_MAX_DEFERRED_TRANSACTION_REPLY_BYTES_PER_PEER;
  for( const size_t json_size : { size_t( 60000 ), size_t( 200 ) } )
  {
    connected_pair pair;
    const std::shared_ptr<hive::chain::full_transaction_type> transaction = make_full_transaction( json_size );
    const size_t transaction_size = transaction->get_transaction_size();
    BOOST_REQUIRE_LE( transaction_size, size_t( HIVE_MAX_TRANSACTION_SIZE ) );
    BOOST_CHECK( pair.sender->get_transaction_reply_send_progress_time() == fc::time_point::maximum() );

    const fc::time_point before = fc::time_point::now();
    size_t accepted = 0;
    while( pair.sender->send_transaction_reply( transaction ) )
    {
      ++accepted;
      BOOST_REQUIRE_LE( pair.sender->get_deferred_transaction_reply_bytes(), byte_bound );
    }
    const size_t queued_at_once = GRAPHENE_NET_MAXIMUM_QUEUED_TRANSACTION_REPLY_BYTES / transaction_size;
    const size_t waiting = accepted - queued_at_once;
    BOOST_CHECK_EQUAL( pair.sender->get_deferred_transaction_reply_bytes(), waiting * transaction_size );
    if( transaction_size * GRAPHENE_NET_MAX_DEFERRED_TRANSACTION_REPLIES_PER_PEER > byte_bound )
      BOOST_CHECK_EQUAL( waiting, byte_bound / transaction_size );
    else
      BOOST_CHECK_EQUAL( waiting, size_t( GRAPHENE_NET_MAX_DEFERRED_TRANSACTION_REPLIES_PER_PEER ) );
    const fc::time_point progress = pair.sender->get_transaction_reply_send_progress_time();
    BOOST_CHECK( progress >= before && progress <= fc::time_point::now() );

    pair.wait_for_messages( accepted );
    BOOST_CHECK( !pair.receiver_delegate.closed );
    BOOST_CHECK_EQUAL( pair.receiver_delegate.received.size(), accepted );
    BOOST_CHECK_EQUAL( pair.sender->get_deferred_transaction_reply_bytes(), 0u );
    BOOST_CHECK( pair.sender->get_transaction_reply_send_progress_time() == fc::time_point::maximum() );
  }
}

/// Canceling the send task while it builds a message still ends it; the build failure handling does not swallow the cancel.
BOOST_AUTO_TEST_CASE( canceling_the_send_task_while_building_ends_it )
{
  size_t build_attempts = 0;
  bool saw_cancel = false;
  const fc::time_point started_at = fc::time_point::now();
  {
    connected_pair pair;
    pair.sender->send_queueable_message( std::make_unique<blocking_queued_message>( build_attempts, saw_cancel ) );
    pair.sender->send_queueable_message( std::make_unique<blocking_queued_message>( build_attempts, saw_cancel ) );
    fc::usleep( fc::milliseconds( 50 ) );
    BOOST_REQUIRE_EQUAL( build_attempts, 1u );
  }
  BOOST_CHECK( saw_cancel );
  BOOST_CHECK_EQUAL( build_attempts, 1u );
  BOOST_CHECK_LT( ( fc::time_point::now() - started_at ).count(), fc::seconds( 10 ).count() );
}

BOOST_AUTO_TEST_SUITE_END()
