#include <boost/test/unit_test.hpp>

#include <graphene/net/config.hpp>
#include <graphene/net/core_messages.hpp>
#include <graphene/net/peer_connection.hpp>

#include <hive/chain/full_block.hpp>
#include <hive/protocol/hive_operations.hpp>

#include <fc/asio.hpp>
#include <fc/crypto/aes.hpp>
#include <fc/crypto/city.hpp>
#include <fc/crypto/sha256.hpp>
#include <fc/network/ip.hpp>
#include <fc/network/tcp_socket.hpp>
#include <fc/network/tcp_socket_io_hooks.hpp>
#include <fc/thread/thread.hpp>

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

  void on_message( peer_connection*, const message& received_message ) override
  {
    received.push_back( received_message );
    received_at.push_back( fc::time_point::now() );
  }
  void on_connection_closed( peer_connection* ) override { closed = true; }
  std::shared_ptr<hive::chain::full_block_type> get_full_block_by_block_id( const hive::protocol::block_id_type& ) override { return {}; }
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

  void wait_for_messages( size_t count, fc::microseconds timeout = fc::seconds( 30 ) )
  {
    const fc::time_point deadline = fc::time_point::now() + timeout;
    while( receiver_delegate.received.size() < count && !receiver_delegate.closed && fc::time_point::now() < deadline )
      fc::usleep( fc::milliseconds( 1 ) );
  }
};

const fc::microseconds long_flush_delay = fc::seconds( 3 );

message make_transaction_message( char fill )
{
  return make_message( graphene::net::trx_message_type, 200, fill );
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
      pair.sender->send_block_message( block );
    else
      pair.sender->send_message( m );
  }

  pair.wait_for_messages( expected.size() );

  BOOST_REQUIRE_EQUAL( pair.receiver_delegate.received.size(), expected.size() );
  for( size_t i = 0; i < expected.size(); ++i )
    BOOST_CHECK_MESSAGE( same_message( pair.receiver_delegate.received[i], expected[i] ), "message " << i << " differs" );

  // one message per write would be at least one write per message, plus extra writes for the block
  BOOST_CHECK_LT( pair.hooks.writes, expected.size() / 4 );
  BOOST_CHECK_EQUAL( pair.sender->get_total_bytes_sent(), pair.receiver->get_total_bytes_received() );
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

/// A block already queued behind transactions means the send task does not wait at all.
BOOST_AUTO_TEST_CASE( queued_block_is_sent_without_the_flush_delay )
{
  connected_pair pair;
  pair.sender->set_send_flush_delay( long_flush_delay );

  const std::shared_ptr<hive::chain::full_block_type> block = make_small_block();
  const fc::time_point queued_at = fc::time_point::now();
  pair.sender->send_message( make_transaction_message( 'a' ) );
  pair.sender->send_message( make_transaction_message( 'b' ) );
  pair.sender->send_block_message( block );
  pair.wait_for_messages( 3 );

  BOOST_REQUIRE_EQUAL( pair.receiver_delegate.received.size(), 3u );
  BOOST_CHECK_EQUAL( pair.receiver_delegate.received.back().msg_type, uint32_t( graphene::net::block_message_type ) );
  BOOST_CHECK_LT( ( pair.receiver_delegate.received_at.back() - queued_at ).count(), long_flush_delay.count() / 2 );
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
        pair.sender->send_block_message( make_small_block() );
      else
        pair.sender->send_message( message( graphene::net::fetch_blockchain_item_ids_message( graphene::net::block_message_type, {} ) ) );
      pair.wait_for_messages( 2 );

      BOOST_REQUIRE_EQUAL( pair.receiver_delegate.received.size(), 2u );
      BOOST_CHECK_EQUAL( pair.receiver_delegate.received.front().msg_type, uint32_t( graphene::net::trx_message_type ) );
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
