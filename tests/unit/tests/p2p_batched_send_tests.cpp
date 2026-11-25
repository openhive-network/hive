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
#include <functional>
#include <map>
#if defined( __GLIBC__ )
#include <malloc.h>
#endif
#include <memory>
#include <set>
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
  /// the transactions is_transaction_to_advertise() says are no longer to advertise
  std::set<graphene::net::item_hash_t> transactions_gone;

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
  bool is_transaction_to_advertise( const graphene::net::item_hash_t& message_hash ) override
  {
    return transactions_gone.count( message_hash ) == 0;
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

/// Writes nothing while `blocked`, as toward a peer whose receive window stays closed, and then as fc does.
class blocking_write_hooks : public fc::tcp_socket_io_hooks
{
public:
  bool blocked = true;

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
    wait_while_blocked();
    return fc::asio::write_some( socket, buffer, length ).wait();
  }
  size_t writesome( boost::asio::ip::tcp::socket& socket, const std::shared_ptr<const char>& buffer, size_t length, size_t offset ) override
  {
    wait_while_blocked();
    return fc::asio::write_some( socket, buffer, length, offset ).wait();
  }

private:
  void wait_while_blocked()
  {
    while( blocked )
      fc::usleep( fc::milliseconds( 1 ) );
  }
};

/// Writes as fc does, first calling `before_write` (when set), as when traffic for the peer arrives during every write.
class traffic_during_writes_hooks : public fc::tcp_socket_io_hooks
{
public:
  std::function<void()> before_write;

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
    if( before_write )
      before_write();
    return fc::asio::write_some( socket, buffer, length ).wait();
  }
  size_t writesome( boost::asio::ip::tcp::socket& socket, const std::shared_ptr<const char>& buffer, size_t length, size_t offset ) override
  {
    if( before_write )
      before_write();
    return fc::asio::write_some( socket, buffer, length, offset ).wait();
  }
};

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
    // as when each has accepted the other's hello
    sender->allow_full_size_messages();
    receiver->allow_full_size_messages();
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

/// A queued message whose building takes @p stall, while it takes @p size_in_queue of the queue; it holds back the
/// transaction inventory that the rest of the inventory budget has no room for
struct stalling_queued_message : peer_connection::queued_message
{
  fc::microseconds stall;
  size_t size_in_queue;

  stalling_queued_message( fc::microseconds stall, size_t size_in_queue ) : stall( stall ), size_in_queue( size_in_queue ) {}
  message get_message( graphene::net::peer_connection_delegate*, peer_connection* ) override
  {
    fc::usleep( stall );
    return make_message( 5002, 16, 's' );
  }
  size_t get_size_in_queue() override { return size_in_queue; }
};

/// A message that alone takes the whole queue limit, so queueing it closes the connection unless the queue is empty.
struct queue_limit_sized_message : peer_connection::queued_message
{
  message message_to_send;

  explicit queue_limit_sized_message( message message_to_send ) : message_to_send( std::move( message_to_send ) ) {}
  message get_message( graphene::net::peer_connection_delegate*, peer_connection* ) override { return message_to_send; }
  size_t get_size_in_queue() override { return GRAPHENE_NET_MAXIMUM_QUEUED_MESSAGES_IN_BYTES; }
};

/// The transaction ids listed by the inventories @p delegate received, in the order they arrived; checks that every
/// inventory lists at most GRAPHENE_NET_MAX_TRANSACTION_INVENTORY_IDS_PER_MESSAGE
std::vector<graphene::net::item_hash_t> advertised_transactions( const recording_delegate& delegate )
{
  std::vector<graphene::net::item_hash_t> ids;
  for( const message& received : delegate.received )
    if( received.msg_type == graphene::net::item_ids_inventory_message_type )
    {
      const graphene::net::item_ids_inventory_message inventory = received.as<graphene::net::item_ids_inventory_message>();
      if( inventory.item_type != graphene::net::trx_message_type )
        continue;
      BOOST_CHECK_LE( inventory.item_hashes_available.size(), size_t( GRAPHENE_NET_MAX_TRANSACTION_INVENTORY_IDS_PER_MESSAGE ) );
      ids.insert( ids.end(), inventory.item_hashes_available.begin(), inventory.item_hashes_available.end() );
    }
  return ids;
}

std::vector<graphene::net::item_hash_t> transaction_ids( const std::string& prefix, size_t count )
{
  std::vector<graphene::net::item_hash_t> ids;
  for( size_t i = 0; i < count; ++i )
    ids.push_back( fc::ripemd160::hash( prefix + "/" + std::to_string( i ) ) );
  return ids;
}

std::vector<graphene::net::item_hash_t> sorted( std::vector<graphene::net::item_hash_t> ids )
{
  std::sort( ids.begin(), ids.end() );
  return ids;
}

/// waits until @p pair's receiver has been advertised @p count transactions
void wait_for_advertised( connected_pair& pair, size_t count )
{
  const fc::time_point deadline = fc::time_point::now() + fc::seconds( 30 );
  while( advertised_transactions( pair.receiver_delegate ).size() < count && !pair.receiver_delegate.closed &&
         fc::time_point::now() < deadline )
    fc::usleep( fc::milliseconds( 5 ) );
}

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
/// the count bound for small ones, the rest are refused.  While they wait the send queue is never empty, so the
/// connection reports when the queue last made progress towards the peer, which is what a peer that stops reading is
/// dropped on (the slow-reader rule).
BOOST_AUTO_TEST_CASE( waiting_transaction_replies_are_bounded_in_bytes )
{
  const size_t byte_bound = GRAPHENE_NET_MAX_DEFERRED_TRANSACTION_REPLY_BYTES_PER_PEER;
  for( const size_t json_size : { size_t( 60000 ), size_t( 200 ) } )
  {
    connected_pair pair;
    const std::shared_ptr<hive::chain::full_transaction_type> transaction = make_full_transaction( json_size );
    const size_t transaction_size = transaction->get_transaction_size();
    BOOST_REQUIRE_LE( transaction_size, size_t( HIVE_MAX_TRANSACTION_SIZE ) );
    BOOST_CHECK( pair.sender->get_send_progress_time() == fc::time_point::maximum() );

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
    const fc::time_point progress = pair.sender->get_send_progress_time();
    BOOST_CHECK( progress >= before && progress <= fc::time_point::now() );

    pair.wait_for_messages( accepted );
    BOOST_CHECK( !pair.receiver_delegate.closed );
    BOOST_CHECK_EQUAL( pair.receiver_delegate.received.size(), accepted );
    BOOST_CHECK_EQUAL( pair.sender->get_deferred_transaction_reply_bytes(), 0u );
    const fc::time_point deadline = fc::time_point::now() + fc::seconds( 5 );
    while( pair.sender->get_send_progress_time() != fc::time_point::maximum() && fc::time_point::now() < deadline )
      fc::usleep( fc::milliseconds( 1 ) );
    BOOST_CHECK( pair.sender->get_send_progress_time() == fc::time_point::maximum() );
  }
}

/// Transaction inventories queued faster than the peer reads them, far over their budget in the send queue, wait for
/// the queue to drain instead of closing the connection or being dropped: every id arrives, once, a block
/// inventory queued after them still goes out first, and a message that is not transaction inventory still closes
/// the connection when it would take the queue over its limit.
BOOST_AUTO_TEST_CASE( transaction_inventory_over_its_budget_waits_for_the_queue_to_drain )
{
  connected_pair pair;

  // everything is queued before the send task gets to run, as when a node advertises a burst of new transactions;
  // more than the budget holds, though fewer waiting than may wait
  const size_t per_inventory = 1000;
  const size_t inventories = ( GRAPHENE_NET_MAXIMUM_QUEUED_TRANSACTION_INVENTORY_BYTES +
                               GRAPHENE_NET_MAX_DEFERRED_TRANSACTION_INVENTORY_IDS_PER_PEER * 20 / 2 ) / ( per_inventory * 20 );
  std::vector<graphene::net::item_hash_t> sent;
  for( size_t i = 0; i < inventories; ++i )
  {
    const std::vector<graphene::net::item_hash_t> ids = transaction_ids( std::to_string( i ), per_inventory );
    sent.insert( sent.end(), ids.begin(), ids.end() );
    pair.sender->send_message( message( graphene::net::item_ids_inventory_message( graphene::net::trx_message_type, ids ) ) );
  }
  BOOST_REQUIRE_GT( sent.size() * 20, size_t( GRAPHENE_NET_MAXIMUM_QUEUED_TRANSACTION_INVENTORY_BYTES ) );
  const message block_inventory( graphene::net::item_ids_inventory_message( graphene::net::block_message_type,
                                                                            { fc::ripemd160::hash( std::string( "block" ) ) } ) );
  pair.sender->send_message( block_inventory );

  wait_for_advertised( pair, sent.size() );
  BOOST_CHECK( !pair.sender_delegate.closed );
  BOOST_CHECK( !pair.receiver_delegate.closed );
  BOOST_REQUIRE( !pair.receiver_delegate.received.empty() );
  BOOST_CHECK( same_message( pair.receiver_delegate.received.front(), block_inventory ) );
  BOOST_CHECK( sorted( advertised_transactions( pair.receiver_delegate ) ) == sorted( sent ) );

  // anything else that does not fit closes the connection, as before
  pair.sender->send_message( make_message( 5000, 16, 'x' ) );
  pair.sender->send_queueable_message( std::make_unique<queue_limit_sized_message>( make_message( 5001, 16, 'y' ) ) );
  const fc::time_point deadline = fc::time_point::now() + fc::seconds( 30 );
  while( !pair.receiver_delegate.closed && fc::time_point::now() < deadline )
    fc::usleep( fc::milliseconds( 1 ) );
  BOOST_CHECK( pair.receiver_delegate.closed );
}

/// One transaction inventory larger than its budget in the send queue, queued while no transaction traffic is, is
/// delivered whole, split into messages within the budget
BOOST_AUTO_TEST_CASE( transaction_inventory_larger_than_its_budget_is_split )
{
  connected_pair pair;

  const std::vector<graphene::net::item_hash_t> sent = transaction_ids( "large", 30000 );
  const message large( graphene::net::item_ids_inventory_message( graphene::net::trx_message_type, sent ) );
  BOOST_REQUIRE_GT( large.data.size(), size_t( GRAPHENE_NET_MAXIMUM_QUEUED_TRANSACTION_INVENTORY_BYTES ) );
  BOOST_REQUIRE_LE( large.data.size(), size_t( MAX_MESSAGE_SIZE ) );
  pair.sender->send_message( large );

  wait_for_advertised( pair, sent.size() );
  BOOST_CHECK( !pair.receiver_delegate.closed );
  BOOST_CHECK( sorted( advertised_transactions( pair.receiver_delegate ) ) == sorted( sent ) );
  BOOST_CHECK_GT( pair.receiver_delegate.received.size(), 1u );
}

/// Past the number of transaction ids that may wait for the send queue the oldest waiting ones are dropped: the peer
/// gets the ones queued at once and the newest that waited, the connection stays open, and once the queue has
/// drained the next inventory goes out at once
BOOST_AUTO_TEST_CASE( waiting_transaction_inventory_is_bounded )
{
  connected_pair pair;

  // fill the inventory budget with inventories of 1000 ids (20006 bytes each), then advertise more than may wait
  std::vector<graphene::net::item_hash_t> queued_at_once;
  for( size_t i = 0; ( queued_at_once.size() + 1000 ) * 20 + ( i + 1 ) * 6 <= GRAPHENE_NET_MAXIMUM_QUEUED_TRANSACTION_INVENTORY_BYTES; ++i )
  {
    const std::vector<graphene::net::item_hash_t> ids = transaction_ids( "filler " + std::to_string( i ), 1000 );
    queued_at_once.insert( queued_at_once.end(), ids.begin(), ids.end() );
    pair.sender->send_transaction_inventory( ids );
  }
  const size_t may_wait = GRAPHENE_NET_MAX_DEFERRED_TRANSACTION_INVENTORY_IDS_PER_PEER;
  const size_t over = 5000;
  const std::vector<graphene::net::item_hash_t> burst = transaction_ids( "burst", may_wait + over );
  pair.sender->send_transaction_inventory( burst );

  // the room the fillers left takes fewer than another 1000 of the burst at once; of the rest the oldest are dropped
  wait_for_advertised( pair, queued_at_once.size() + may_wait );
  fc::usleep( fc::milliseconds( 100 ) );
  BOOST_CHECK( !pair.receiver_delegate.closed );
  const std::vector<graphene::net::item_hash_t> advertised = advertised_transactions( pair.receiver_delegate );
  BOOST_REQUIRE_GE( advertised.size(), queued_at_once.size() + may_wait );
  const size_t burst_advertised = advertised.size() - queued_at_once.size();
  BOOST_REQUIRE_LT( burst_advertised, may_wait + 1000 );
  std::vector<graphene::net::item_hash_t> expected = queued_at_once;
  expected.insert( expected.end(), burst.end() - burst_advertised, burst.end() );
  BOOST_CHECK( std::vector<graphene::net::item_hash_t>( advertised.begin(), advertised.begin() + queued_at_once.size() ) == queued_at_once );
  BOOST_CHECK( sorted( advertised ) == sorted( expected ) );

  const std::vector<graphene::net::item_hash_t> next = transaction_ids( "next", 10 );
  pair.sender->send_transaction_inventory( next );
  expected.insert( expected.end(), next.begin(), next.end() );
  wait_for_advertised( pair, expected.size() );
  const std::vector<graphene::net::item_hash_t> advertised_after = advertised_transactions( pair.receiver_delegate );
  BOOST_CHECK( std::vector<graphene::net::item_hash_t>( advertised_after.end() - next.size(), advertised_after.end() ) == next );
}

/// The inventory budget's room takes as many waiting ids as fit, rather than waiting for room for a whole message, and
/// as the queue drains the newest ids go first: a fresh id doesn't wait behind the backlog
BOOST_AUTO_TEST_CASE( waiting_transaction_inventory_goes_newest_first_as_far_as_it_fits )
{
  connected_pair pair;

  // a stalled message leaves room for an inventory of 10 ids
  const size_t room_for_ten = 4 + 1 + 10 * 20;
  pair.sender->send_queueable_message( std::make_unique<stalling_queued_message>(
    fc::milliseconds( 200 ), GRAPHENE_NET_MAXIMUM_QUEUED_TRANSACTION_INVENTORY_BYTES - room_for_ten ) );
  const std::vector<graphene::net::item_hash_t> backlog = transaction_ids( "backlog", 100 );
  pair.sender->send_transaction_inventory( backlog );
  const std::vector<graphene::net::item_hash_t> fresh = transaction_ids( "fresh", 5 );
  pair.sender->send_transaction_inventory( fresh );

  wait_for_advertised( pair, backlog.size() + fresh.size() );
  BOOST_CHECK( !pair.receiver_delegate.closed );
  std::vector<graphene::net::item_hash_t> expected( backlog.end() - 10, backlog.end() );
  expected.insert( expected.end(), fresh.begin(), fresh.end() );
  expected.insert( expected.end(), backlog.begin(), backlog.end() - 10 );
  BOOST_CHECK( advertised_transactions( pair.receiver_delegate ) == expected );
}

/// Waiting transaction ids are not advertised once they have waited over
/// GRAPHENE_NET_MAX_DEFERRED_TRANSACTION_INVENTORY_AGE_MS, or once the delegate no longer has their transaction
BOOST_AUTO_TEST_CASE( stale_waiting_transaction_inventory_is_not_advertised )
{
  connected_pair pair;

  const fc::microseconds age = fc::milliseconds( GRAPHENE_NET_MAX_DEFERRED_TRANSACTION_INVENTORY_AGE_MS );
  pair.sender->send_queueable_message( std::make_unique<stalling_queued_message>(
    age + fc::milliseconds( 500 ), GRAPHENE_NET_MAXIMUM_QUEUED_TRANSACTION_INVENTORY_BYTES ) );
  const std::vector<graphene::net::item_hash_t> expired = transaction_ids( "expired", 10 );
  pair.sender->send_transaction_inventory( expired );
  fc::usleep( age + fc::milliseconds( 200 ) );
  const std::vector<graphene::net::item_hash_t> recent = transaction_ids( "recent", 10 );
  pair.sender_delegate.transactions_gone = { recent[ 2 ], recent[ 7 ] };
  pair.sender->send_transaction_inventory( recent );

  std::vector<graphene::net::item_hash_t> expected = recent;
  expected.erase( expected.begin() + 7 );
  expected.erase( expected.begin() + 2 );
  wait_for_advertised( pair, expected.size() );
  fc::usleep( fc::milliseconds( 100 ) );
  BOOST_CHECK( !pair.receiver_delegate.closed );
  BOOST_CHECK( advertised_transactions( pair.receiver_delegate ) == expected );
}

/// Canceling the send task while it builds a message still ends it; the build failure handling does not swallow the cancel.
/// Toward a peer that reads nothing (its socket takes no bytes), however much transaction inventory waits outside the
/// send queue, which doesn't close the connection, the connection's send progress time stays when sending started,
/// which is what such a peer is disconnected on.  Once the socket takes bytes again, everything is written and, with
/// nothing left to send, the time is fc::time_point::maximum().
BOOST_AUTO_TEST_CASE( send_progress_stops_while_the_peer_reads_nothing )
{
  connected_pair pair;
  blocking_write_hooks blocking;
  pair.sender->get_socket().set_io_hooks( &blocking );
  BOOST_CHECK( pair.sender->get_send_progress_time() == fc::time_point::maximum() );

  const fc::time_point before = fc::time_point::now();
  const auto advertise = [&]( size_t i ) {
    pair.sender->send_message( message( graphene::net::item_ids_inventory_message( graphene::net::trx_message_type,
                                                                                   transaction_ids( "unread " + std::to_string( i ), 1000 ) ) ) );
  };
  advertise( 0 );
  const fc::time_point pending_since = pair.sender->get_send_progress_time();
  BOOST_CHECK( pending_since >= before && pending_since <= fc::time_point::now() );
  for( size_t i = 1; i < 100; ++i )
  {
    fc::usleep( fc::milliseconds( 10 ) );
    advertise( i );
  }
  BOOST_CHECK( pair.sender->get_send_progress_time() == pending_since );
  BOOST_CHECK( !pair.sender_delegate.closed );

  blocking.blocked = false;
  const fc::time_point deadline = fc::time_point::now() + fc::seconds( 30 );
  while( pair.sender->get_send_progress_time() != fc::time_point::maximum() && fc::time_point::now() < deadline )
    fc::usleep( fc::milliseconds( 1 ) );
  BOOST_CHECK( pair.sender->get_send_progress_time() == fc::time_point::maximum() );
  BOOST_CHECK( !pair.receiver_delegate.closed );
  BOOST_CHECK( !advertised_transactions( pair.receiver_delegate ).empty() );
}

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

/// A transaction may be as large as the head block allows (maximum_block_size - 256), over the send queue's limit when
/// the witnesses vote blocks over it.  One such message is accepted into an empty queue, and the messages queued while
/// it waits for the send task or is being written are held to the limit without it: none closes the connection, and
/// all arrive in order.
BOOST_AUTO_TEST_CASE( message_over_the_queue_limit_is_accepted_into_an_empty_queue )
{
  const message oversized = make_message( 5000, GRAPHENE_NET_MAXIMUM_QUEUED_MESSAGES_IN_BYTES * 3 / 2, 'o' );
  BOOST_REQUIRE_LE( oversized.data.size(), size_t( MAX_MESSAGE_SIZE ) );
  for( bool being_written : { false, true } )
  {
    BOOST_TEST_CONTEXT( ( being_written ? "being written" : "waiting in the queue" ) )
    {
      connected_pair pair;
      blocking_write_hooks blocking;
      pair.sender->get_socket().set_io_hooks( &blocking );

      pair.sender->send_message( oversized );
      if( being_written )
        fc::usleep( fc::milliseconds( 50 ) ); // the send task takes it off the queue and blocks writing it
      const message behind_a = make_message( 5001, GRAPHENE_NET_MAXIMUM_QUEUED_MESSAGES_IN_BYTES / 2, 'a' );
      const message behind_b = make_message( 5002, GRAPHENE_NET_MAXIMUM_QUEUED_MESSAGES_IN_BYTES / 2 - 16, 'b' );
      pair.sender->send_message( behind_a );
      pair.sender->send_message( behind_b );
      fc::usleep( fc::milliseconds( 50 ) );
      BOOST_CHECK( !pair.sender_delegate.closed );
      blocking.blocked = false;
      pair.wait_for_messages( 3 );

      BOOST_CHECK( !pair.sender_delegate.closed );
      BOOST_CHECK( !pair.receiver_delegate.closed );
      BOOST_REQUIRE_EQUAL( pair.receiver_delegate.received.size(), 3u );
      BOOST_CHECK( same_message( pair.receiver_delegate.received[0], oversized ) );
      BOOST_CHECK( same_message( pair.receiver_delegate.received[1], behind_a ) );
      BOOST_CHECK( same_message( pair.receiver_delegate.received[2], behind_b ) );

      // once it is written the queue holds the limit again, as for any other message
      fc::usleep( fc::milliseconds( 50 ) );
      pair.sender->send_queueable_message( std::make_unique<queue_limit_sized_message>( make_message( 5003, 16, 'p' ) ) );
      pair.wait_for_messages( 4 );
      BOOST_CHECK( !pair.receiver_delegate.closed );
      BOOST_CHECK_EQUAL( pair.receiver_delegate.received.size(), 4u );
    }
  }
}

/// The limit still closes the connection when the queue holds something: a second message over the limit while the
/// first is queued, or messages behind an oversized one that take the rest of the queue over the limit.
BOOST_AUTO_TEST_CASE( queue_holding_messages_still_closes_past_the_limit )
{
  const message oversized = make_message( 5000, GRAPHENE_NET_MAXIMUM_QUEUED_MESSAGES_IN_BYTES * 3 / 2, 'o' );
  for( bool behind_oversized : { false, true } )
  {
    BOOST_TEST_CONTEXT( ( behind_oversized ? "behind an oversized message" : "a second oversized message" ) )
    {
      connected_pair pair;
      blocking_write_hooks blocking;
      pair.sender->get_socket().set_io_hooks( &blocking );

      if( behind_oversized )
      {
        pair.sender->send_message( oversized );
        pair.sender->send_message( make_message( 5001, 16, 'a' ) );
        pair.sender->send_queueable_message( std::make_unique<queue_limit_sized_message>( make_message( 5002, 16, 'b' ) ) );
      }
      else
      {
        pair.sender->send_message( make_message( 5001, 16, 'a' ) );
        pair.sender->send_message( oversized );
        pair.sender->send_message( oversized );
      }
      blocking.blocked = false;
      const fc::time_point deadline = fc::time_point::now() + fc::seconds( 30 );
      while( !pair.receiver_delegate.closed && fc::time_point::now() < deadline )
        fc::usleep( fc::milliseconds( 1 ) );
      BOOST_CHECK( pair.receiver_delegate.closed );
    }
  }
}

/// A requested transaction larger than the send queue's limit is queued beside the other replies, outside the limit,
/// and the replies behind it fit the reply budget without it; a second one waits only for the first to be written.
/// The replies around them, and a block queued while they are written, all arrive, with the replies in the order asked.
BOOST_AUTO_TEST_CASE( transaction_reply_over_the_queue_limit_is_queued_beside_other_replies )
{
  connected_pair pair;
  blocking_write_hooks blocking;
  pair.sender->get_socket().set_io_hooks( &blocking );

  const std::shared_ptr<hive::chain::full_transaction_type> small = make_full_transaction( 8000 );
  const std::shared_ptr<hive::chain::full_transaction_type> large = make_full_transaction( GRAPHENE_NET_MAXIMUM_QUEUED_MESSAGES_IN_BYTES * 3 / 2 );
  BOOST_REQUIRE_GT( large->get_transaction_size(), size_t( GRAPHENE_NET_MAXIMUM_QUEUED_MESSAGES_IN_BYTES ) );
  BOOST_REQUIRE_LE( large->get_transaction_size(), size_t( MAX_MESSAGE_SIZE ) );
  BOOST_REQUIRE( pair.sender->send_transaction_reply( small ) );
  BOOST_REQUIRE( pair.sender->send_transaction_reply( large ) );
  BOOST_REQUIRE( pair.sender->send_transaction_reply( small ) );
  BOOST_REQUIRE( pair.sender->send_transaction_reply( large ) );
  BOOST_REQUIRE( pair.sender->send_transaction_reply( small ) );
  // the send task is writing the first reply, and the first large one and the small one behind it are queued: only
  // the second large one, and the reply asked for after it, wait outside the queue
  fc::usleep( fc::milliseconds( 50 ) );
  BOOST_CHECK_EQUAL( pair.sender->get_deferred_transaction_reply_bytes(),
                     large->get_transaction_size() + small->get_transaction_size() );
  blocking.blocked = false;
  const fc::time_point large_written_deadline = fc::time_point::now() + fc::seconds( 30 );
  while( pair.sender->get_deferred_transaction_reply_bytes() > 0 && fc::time_point::now() < large_written_deadline )
    fc::usleep( fc::milliseconds( 1 ) );
  pair.send_block( make_small_block() );
  pair.wait_for_messages( 6 );

  BOOST_CHECK( !pair.sender_delegate.closed );
  BOOST_CHECK( !pair.receiver_delegate.closed );
  BOOST_CHECK_EQUAL( pair.sender->get_deferred_transaction_reply_bytes(), 0u );
  BOOST_REQUIRE_EQUAL( pair.receiver_delegate.received.size(), 6u );
  const message small_reply = message( graphene::net::trx_message( small ) );
  const message large_reply = message( graphene::net::trx_message( large ) );
  std::vector<message> replies;
  for( const message& received : pair.receiver_delegate.received )
    if( received.msg_type != graphene::net::block_message_type )
      replies.push_back( received );
  BOOST_REQUIRE_EQUAL( replies.size(), 5u );
  for( size_t i = 0; i < replies.size(); ++i )
    BOOST_CHECK_MESSAGE( same_message( replies[i], i % 2 == 0 ? small_reply : large_reply ), "reply " << i << " differs" );
}

/// Under continuous small traffic the send queue is never empty when a write ends: here a control message and a
/// transaction inventory are queued during every socket write.  Requested transactions larger than the queue's limit
/// and larger than the reply budget still go out at once, well within the time the requester waits for them, and so
/// do the small replies asked for behind them, in order; the connection stays open, and the small traffic flows on.
BOOST_AUTO_TEST_CASE( large_transaction_replies_are_not_starved_by_continuous_small_traffic )
{
  connected_pair pair;
  traffic_during_writes_hooks traffic;
  size_t traffic_queued = 0;
  traffic.before_write = [&]() {
    pair.sender->send_message( make_message( 5000, 16, 'f' ) );
    pair.sender->send_transaction_inventory( transaction_ids( "traffic " + std::to_string( traffic_queued ), 10 ) );
    ++traffic_queued;
  };
  pair.sender->get_socket().set_io_hooks( &traffic );
  pair.sender->send_message( make_message( 5000, 16, 'f' ) );
  wait_for_advertised( pair, 50 );
  BOOST_REQUIRE( !pair.receiver_delegate.closed );

  const std::shared_ptr<hive::chain::full_transaction_type> small = make_full_transaction( 8000 );
  const std::shared_ptr<hive::chain::full_transaction_type> over_limit = make_full_transaction( GRAPHENE_NET_MAXIMUM_QUEUED_MESSAGES_IN_BYTES * 3 / 2 );
  const std::shared_ptr<hive::chain::full_transaction_type> over_budget = make_full_transaction( GRAPHENE_NET_MAXIMUM_QUEUED_TRANSACTION_REPLY_BYTES * 3 / 2 );
  BOOST_REQUIRE_GT( over_limit->get_transaction_size(), size_t( GRAPHENE_NET_MAXIMUM_QUEUED_MESSAGES_IN_BYTES ) );
  BOOST_REQUIRE_GT( over_budget->get_transaction_size(), size_t( GRAPHENE_NET_MAXIMUM_QUEUED_TRANSACTION_REPLY_BYTES ) );
  BOOST_REQUIRE_LE( over_budget->get_transaction_size(), size_t( GRAPHENE_NET_MAXIMUM_QUEUED_MESSAGES_IN_BYTES ) );
  const std::vector<std::shared_ptr<hive::chain::full_transaction_type>> asked = { small, over_limit, small, over_budget, small };
  const size_t traffic_before = traffic_queued;
  const fc::time_point asked_at = fc::time_point::now();
  for( const std::shared_ptr<hive::chain::full_transaction_type>& transaction : asked )
    BOOST_REQUIRE( pair.sender->send_transaction_reply( transaction ) );

  const auto replies_received = [&]() {
    std::vector<message> replies;
    for( const message& received : pair.receiver_delegate.received )
      if( received.msg_type == graphene::net::trx_message_type )
        replies.push_back( received );
    return replies;
  };
  const fc::time_point deadline = asked_at + fc::seconds( 30 );
  while( replies_received().size() < asked.size() && !pair.receiver_delegate.closed && fc::time_point::now() < deadline )
    fc::usleep( fc::milliseconds( 1 ) );
  const fc::microseconds took = fc::time_point::now() - asked_at;
  const size_t traffic_meanwhile = traffic_queued - traffic_before;
  // the traffic queued meanwhile still goes out once it stops
  traffic.before_write = nullptr;
  wait_for_advertised( pair, traffic_queued * 10 );

  BOOST_CHECK( !pair.sender_delegate.closed );
  BOOST_CHECK( !pair.receiver_delegate.closed );
  const std::vector<message> replies = replies_received();
  BOOST_REQUIRE_EQUAL( replies.size(), asked.size() );
  for( size_t i = 0; i < replies.size(); ++i )
    BOOST_CHECK_MESSAGE( same_message( replies[i], message( graphene::net::trx_message( asked[i] ) ) ), "reply " << i << " differs" );
  // every write of them had traffic queued behind it: at least one per 64 KiB written
  BOOST_CHECK_GE( traffic_meanwhile, ( over_limit->get_transaction_size() + over_budget->get_transaction_size() ) / GRAPHENE_NET_CONNECTION_BUFFER_SIZE );
  // a requester gives up on a reply after GRAPHENE_NET_ACTIVE_IGNORED_REQUEST_TIMEOUT_SECONDS
  BOOST_CHECK_LT( took.count(), fc::seconds( GRAPHENE_NET_ACTIVE_IGNORED_REQUEST_TIMEOUT_SECONDS ).count() / 3 );
}

#if defined( __GLIBC__ )
/// A message as large as a transaction may be, being written to a peer that takes no bytes, holds no private copy of
/// itself: the connection allocates a staging buffer of at most GRAPHENE_NET_CONNECTION_BUFFER_SIZE beyond the
/// transaction, which is written from its shared serialization.  A private copy of the batch would add the whole
/// message, 1.5 MiB here.
BOOST_AUTO_TEST_CASE( writing_a_large_message_holds_no_copy_of_it )
{
  connected_pair pair;
  blocking_write_hooks blocking;
  blocking.blocked = false;
  pair.sender->get_socket().set_io_hooks( &blocking );
  // a first write sets up what every write reuses (the socket's write buffer, the send task's stack)
  pair.sender->send_message( make_message( 5000, 16, 'w' ) );
  pair.wait_for_messages( 1 );
  BOOST_REQUIRE_EQUAL( pair.receiver_delegate.received.size(), 1u );
  fc::usleep( fc::milliseconds( 50 ) );
  blocking.blocked = true;
  const std::shared_ptr<hive::chain::full_transaction_type> large = make_full_transaction( GRAPHENE_NET_MAXIMUM_QUEUED_MESSAGES_IN_BYTES * 3 / 2 );
  const size_t large_size = large->get_transaction_size();
  BOOST_REQUIRE_GT( large_size, size_t( GRAPHENE_NET_MAXIMUM_QUEUED_MESSAGES_IN_BYTES ) );

  // what malloc has handed out over all arenas: in use from the heap plus mmapped
  const auto allocated = []() -> int64_t {
#if __GLIBC_PREREQ( 2, 33 )
    const struct mallinfo2 info = mallinfo2();
#else
    const struct mallinfo info = mallinfo();
#endif
    return int64_t( info.uordblks ) + int64_t( info.hblkhd );
  };
  const int64_t before = allocated();
  BOOST_REQUIRE( pair.sender->send_transaction_reply( large ) );
  fc::usleep( fc::milliseconds( 100 ) ); // the send task takes it off the queue and blocks writing it
  const int64_t allocated_while_written = allocated() - before;
  BOOST_TEST_MESSAGE( "allocated while a " << large_size << "-byte transaction is written: " << allocated_while_written << " bytes" );
  BOOST_CHECK_LT( allocated_while_written, int64_t( 2 * GRAPHENE_NET_CONNECTION_BUFFER_SIZE ) );

  blocking.blocked = false;
  pair.wait_for_messages( 2 );
  BOOST_CHECK( !pair.receiver_delegate.closed );
  BOOST_REQUIRE_EQUAL( pair.receiver_delegate.received.size(), 2u );
  BOOST_CHECK( same_message( pair.receiver_delegate.received[1], message( graphene::net::trx_message( large ) ) ) );
}
#endif

BOOST_AUTO_TEST_SUITE_END()
