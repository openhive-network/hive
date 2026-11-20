#include <boost/test/unit_test.hpp>

#include <graphene/net/config.hpp>
#include <graphene/net/peer_connection.hpp>
#include <graphene/net/stcp_socket.hpp>

#include <fc/asio.hpp>
#include <fc/network/ip.hpp>
#include <fc/network/tcp_socket.hpp>
#include <fc/network/tcp_socket_io_hooks.hpp>
#include <fc/thread/thread.hpp>

#include <algorithm>
#include <cstring>
#include <functional>
#include <vector>

using graphene::net::message;
using graphene::net::message_header;
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
  std::shared_ptr<hive::chain::full_block_type> get_block_to_send( peer_connection*, const graphene::net::item_hash_t& ) override { return {}; }
};

/// Counts the socket reads that have completed.
class read_counting_hooks : public fc::tcp_socket_io_hooks
{
public:
  size_t completed_reads = 0;

  size_t readsome( boost::asio::ip::tcp::socket& socket, char* buffer, size_t length ) override
  {
    const size_t bytes_read = fc::asio::read_some( socket, buffer, length ).wait();
    ++completed_reads;
    return bytes_read;
  }
  size_t readsome( boost::asio::ip::tcp::socket& socket, const std::shared_ptr<char>& buffer, size_t length, size_t offset ) override
  {
    const size_t bytes_read = fc::asio::read_some( socket, buffer, length, offset ).wait();
    ++completed_reads;
    return bytes_read;
  }
  size_t writesome( boost::asio::ip::tcp::socket& socket, const char* buffer, size_t length ) override
  {
    return fc::asio::write_some( socket, buffer, length ).wait();
  }
  size_t writesome( boost::asio::ip::tcp::socket& socket, const std::shared_ptr<const char>& buffer, size_t length, size_t offset ) override
  {
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

bool same_message( const message& a, const message& b )
{
  return a.msg_type == b.msg_type && a.size == b.size && a.data == b.data;
}

/// Appends @p m to @p stream as a node puts it on the wire: header and payload, padded to whole AES blocks.
void append_message( std::vector<char>& stream, const message_header& header, const char* payload, size_t payload_size )
{
  const size_t start = stream.size();
  stream.resize( start + 16 * ( ( sizeof( message_header ) + payload_size + 15 ) / 16 ), 0 );
  memcpy( stream.data() + start, &header, sizeof( message_header ) );
  if( payload_size )
    memcpy( stream.data() + start + sizeof( message_header ), payload, payload_size );
}

void append_message( std::vector<char>& stream, const message& m )
{
  append_message( stream, m, m.data.data(), m.data.size() );
}

/// A peer_connection receiving over loopback from a bare encrypted socket, which writes the stream in chunks of the test's choosing.
struct raw_sender_pair
{
  recording_delegate receiver_delegate;
  peer_connection_ptr receiver = peer_connection::make_shared( &receiver_delegate );
  read_counting_hooks hooks;
  graphene::net::stcp_socket sender;

  raw_sender_pair()
  {
    receiver->get_socket().set_io_hooks( &hooks );
    fc::tcp_server server;
    server.listen( fc::ip::endpoint( fc::ip::address( "127.0.0.1" ), 0 ) );
    fc::future<void> accepted = fc::async( [&]() {
      server.accept( receiver->get_socket() );
      receiver->accept_connection();
    }, "batched_receive_test accept" );
    sender.connect_to( fc::ip::endpoint( fc::ip::address( "127.0.0.1" ), server.get_port() ) );
    sender.get_socket().set_no_delay( true );
    accepted.wait();
  }

  ~raw_sender_pair()
  {
    sender.close();
    receiver->close_connection();
    receiver->destroy_connection( __FUNCTION__ );
    receiver->get_socket().set_io_hooks( nullptr );
  }

  void write( const char* bytes, size_t length )
  {
    sender.write( bytes, length );
    sender.flush();
  }

  /// writes @p length bytes and returns once the receiver has read them, so the next write arrives in a read of its own
  void write_and_wait_for_read( const char* bytes, size_t length )
  {
    const size_t reads_before = hooks.completed_reads;
    write( bytes, length );
    const fc::time_point deadline = fc::time_point::now() + fc::seconds( 10 );
    while( hooks.completed_reads == reads_before && fc::time_point::now() < deadline )
      fc::usleep( fc::microseconds( 200 ) );
    BOOST_REQUIRE_GT( hooks.completed_reads, reads_before );
  }

  void wait_until( const std::function<bool()>& condition, fc::microseconds timeout = fc::seconds( 30 ) )
  {
    const fc::time_point deadline = fc::time_point::now() + timeout;
    while( !condition() && fc::time_point::now() < deadline )
      fc::usleep( fc::milliseconds( 1 ) );
  }
};

} // namespace

BOOST_AUTO_TEST_SUITE( p2p_batched_receive_tests )

/// Many messages arriving together are all handed to the node, in order, from a few socket reads
/// rather than two reads per message.
BOOST_AUTO_TEST_CASE( messages_in_one_segment_are_handled_from_few_reads )
{
  raw_sender_pair pair;

  std::vector<message> expected;
  std::vector<char> stream;
  for( size_t i = 0; i < 300; ++i )
  {
    expected.push_back( make_message( 5000 + i, ( i * 37 ) % 200, char( 'a' + i % 26 ) ) );
    append_message( stream, expected.back() );
  }
  BOOST_REQUIRE_LE( stream.size(), 64u * 1024u );

  const size_t reads_before = pair.hooks.completed_reads;
  pair.write( stream.data(), stream.size() );
  pair.wait_until( [&]() { return pair.receiver_delegate.received.size() >= expected.size() || pair.receiver_delegate.closed; } );

  BOOST_REQUIRE_EQUAL( pair.receiver_delegate.received.size(), expected.size() );
  for( size_t i = 0; i < expected.size(); ++i )
    BOOST_CHECK_MESSAGE( same_message( pair.receiver_delegate.received[i], expected[i] ), "message " << i << " differs" );
  BOOST_CHECK_LT( pair.hooks.completed_reads - reads_before, expected.size() / 10 );
  BOOST_CHECK_EQUAL( pair.receiver->get_total_bytes_received(), stream.size() );
  BOOST_CHECK( !pair.receiver_delegate.closed );
}

/// A message whose bytes arrive across many reads, its header split from its payload and from the
/// message before it, is handed to the node once, whole, after its last byte; this holds both for a
/// message that fits the receive buffer and for one larger than it.
BOOST_AUTO_TEST_CASE( message_split_across_many_reads_is_reassembled )
{
  raw_sender_pair pair;

  const message small = make_message( 5001, 1000, 's' );
  std::vector<char> small_stream;
  append_message( small_stream, small );
  for( size_t offset = 0; offset + 16 < small_stream.size(); offset += 16 )
    pair.write_and_wait_for_read( small_stream.data() + offset, 16 );
  BOOST_CHECK_EQUAL( pair.receiver_delegate.received.size(), 0u );
  pair.write_and_wait_for_read( small_stream.data() + small_stream.size() - 16, 16 );
  pair.wait_until( [&]() { return !pair.receiver_delegate.received.empty() || pair.receiver_delegate.closed; } );
  BOOST_REQUIRE_EQUAL( pair.receiver_delegate.received.size(), 1u );
  BOOST_CHECK( same_message( pair.receiver_delegate.received[0], small ) );

  const std::vector<message> expected = {
    make_message( 5002, 20, 'a' ),
    make_message( 5003, 100 * 1024 + 5, 'b' ),
    make_message( 5004, 33, 'c' ),
    make_message( 5005, 30 * 1024 + 3, 'd' ),
  };
  std::vector<char> stream;
  for( const message& m : expected )
    append_message( stream, m );
  const size_t chunk_sizes[] = { 16, 32, 4096, 48, 8192, 16, 12288 };
  size_t chunks = 0;
  for( size_t offset = 0; offset < stream.size(); ++chunks )
  {
    const size_t length = std::min( chunk_sizes[ chunks % ( sizeof( chunk_sizes ) / sizeof( chunk_sizes[0] ) ) ], stream.size() - offset );
    pair.write_and_wait_for_read( stream.data() + offset, length );
    offset += length;
  }
  BOOST_REQUIRE_GT( chunks, 20u );
  pair.wait_until( [&]() { return pair.receiver_delegate.received.size() >= 1 + expected.size() || pair.receiver_delegate.closed; } );

  BOOST_REQUIRE_EQUAL( pair.receiver_delegate.received.size(), 1 + expected.size() );
  for( size_t i = 0; i < expected.size(); ++i )
    BOOST_CHECK_MESSAGE( same_message( pair.receiver_delegate.received[1 + i], expected[i] ), "message " << i << " differs" );
  BOOST_CHECK_EQUAL( pair.receiver->get_total_bytes_received(), small_stream.size() + stream.size() );
  BOOST_CHECK( !pair.receiver_delegate.closed );
}

/// A header announcing more than MAX_MESSAGE_SIZE closes the connection as soon as it is read, without
/// waiting for the payload, while the messages before it in the same segment are still handled.
BOOST_AUTO_TEST_CASE( oversize_message_is_refused_from_its_header )
{
  raw_sender_pair pair;

  const message before = make_message( 5001, 50, 'x' );
  std::vector<char> stream;
  append_message( stream, before );
  message_header oversize;
  oversize.msg_type = 5002;
  oversize.size = MAX_MESSAGE_SIZE + 1;
  append_message( stream, oversize, nullptr, 0 );

  pair.write( stream.data(), stream.size() );
  pair.wait_until( [&]() { return pair.receiver_delegate.closed; }, fc::seconds( 10 ) );

  BOOST_CHECK( pair.receiver_delegate.closed );
  BOOST_REQUIRE_EQUAL( pair.receiver_delegate.received.size(), 1u );
  BOOST_CHECK( same_message( pair.receiver_delegate.received[0], before ) );
}

BOOST_AUTO_TEST_SUITE_END()
