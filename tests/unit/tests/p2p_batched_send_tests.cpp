#include <boost/test/unit_test.hpp>

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
  bool closed = false;

  void on_message( peer_connection*, const message& received_message ) override { received.push_back( received_message ); }
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
  recording_delegate sender_delegate;
  recording_delegate receiver_delegate;
  peer_connection_ptr sender = peer_connection::make_shared( &sender_delegate );
  peer_connection_ptr receiver = peer_connection::make_shared( &receiver_delegate );

  fc::tcp_server server;
  server.listen( fc::ip::endpoint( fc::ip::address( "127.0.0.1" ), 0 ) );
  fc::future<void> accepted = fc::async( [&]() {
    server.accept( receiver->get_socket() );
    receiver->accept_connection();
  }, "batched_send_test accept" );
  sender->connect_to( fc::ip::endpoint( fc::ip::address( "127.0.0.1" ), server.get_port() ) );
  accepted.wait();

  write_counting_hooks hooks;
  sender->get_socket().set_io_hooks( &hooks );

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
      sender->send_block_message( block );
    else
      sender->send_message( m );
  }

  const fc::time_point deadline = fc::time_point::now() + fc::seconds( 30 );
  while( receiver_delegate.received.size() < expected.size() && !receiver_delegate.closed && fc::time_point::now() < deadline )
    fc::usleep( fc::milliseconds( 10 ) );

  BOOST_REQUIRE_EQUAL( receiver_delegate.received.size(), expected.size() );
  for( size_t i = 0; i < expected.size(); ++i )
    BOOST_CHECK_MESSAGE( same_message( receiver_delegate.received[i], expected[i] ), "message " << i << " differs" );

  // one message per write would be at least one write per message, plus extra writes for the block
  BOOST_CHECK_LT( hooks.writes, expected.size() / 4 );
  BOOST_CHECK_EQUAL( sender->get_total_bytes_sent(), receiver->get_total_bytes_received() );

  sender->get_socket().set_io_hooks( nullptr );
  sender->close_connection();
  receiver->close_connection();
  sender->destroy_connection( __FUNCTION__ );
  receiver->destroy_connection( __FUNCTION__ );
}

BOOST_AUTO_TEST_SUITE_END()
