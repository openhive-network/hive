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

/// Reads at most @p chunk bytes at a time: synchronously whatever the socket already holds, so a read whose bytes are
/// there never lets another task run, as an fc read that finds them there need not; waiting for bytes as fc does.
class synchronous_reading_hooks : public fc::tcp_socket_io_hooks
{
public:
  explicit synchronous_reading_hooks( size_t chunk ) : _chunk( chunk ) {}

  size_t readsome( boost::asio::ip::tcp::socket& socket, char* buffer, size_t length ) override
  {
    const size_t available = socket.available();
    if( available == 0 )
      return fc::asio::read_some( socket, buffer, std::min( length, _chunk ) ).wait();
    return socket.read_some( boost::asio::buffer( buffer, std::min( { length, available, _chunk } ) ) );
  }
  size_t readsome( boost::asio::ip::tcp::socket& socket, const std::shared_ptr<char>& buffer, size_t length, size_t offset ) override
  {
    return readsome( socket, buffer.get() + offset, length );
  }
  size_t writesome( boost::asio::ip::tcp::socket& socket, const char* buffer, size_t length ) override
  {
    return fc::asio::write_some( socket, buffer, length ).wait();
  }
  size_t writesome( boost::asio::ip::tcp::socket& socket, const std::shared_ptr<const char>& buffer, size_t length, size_t offset ) override
  {
    return fc::asio::write_some( socket, buffer, length, offset ).wait();
  }

private:
  size_t _chunk;
};

/// Collects whatever a peer_connection receives.
class recording_delegate : public graphene::net::peer_connection_delegate
{
public:
  std::vector<message> received;
  bool closed = false;
  std::function<void()> after_message;

  void on_message( peer_connection*, const message& received_message ) override
  {
    received.push_back( received_message );
    if( after_message )
      after_message();
  }
  void on_connection_closed( peer_connection* ) override { closed = true; }
  std::shared_ptr<hive::chain::full_block_type> get_full_block_by_block_id( const hive::protocol::block_id_type& ) override { return {}; }
  std::shared_ptr<hive::chain::full_block_type> get_block_to_send( peer_connection*, const graphene::net::item_hash_t& ) override { return {}; }
};

/// Counts the socket reads that have completed, and records the most bytes one asked for.
class read_counting_hooks : public fc::tcp_socket_io_hooks
{
public:
  size_t completed_reads = 0;
  size_t largest_read_length = 0;

  size_t readsome( boost::asio::ip::tcp::socket& socket, char* buffer, size_t length ) override
  {
    largest_read_length = std::max( largest_read_length, length );
    const size_t bytes_read = fc::asio::read_some( socket, buffer, length ).wait();
    ++completed_reads;
    return bytes_read;
  }
  size_t readsome( boost::asio::ip::tcp::socket& socket, const std::shared_ptr<char>& buffer, size_t length, size_t offset ) override
  {
    largest_read_length = std::max( largest_read_length, length );
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
/// Unless @p full_size_messages, the receiver stays as it is until a node accepts the peer's hello.
struct raw_sender_pair
{
  recording_delegate receiver_delegate;
  peer_connection_ptr receiver = peer_connection::make_shared( &receiver_delegate );
  read_counting_hooks hooks;
  graphene::net::stcp_socket sender;

  explicit raw_sender_pair( bool full_size_messages = true )
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
    if( full_size_messages )
    {
      receiver->allow_full_size_messages();
      sender.set_buffer_size( GRAPHENE_NET_CONNECTION_BUFFER_SIZE );
    }
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

/// Until the receiver allows full size messages, as before a node accepts the peer's hello, its reads ask for at most
/// GRAPHENE_NET_HANDSHAKE_BUFFER_SIZE bytes, a message of GRAPHENE_NET_MAXIMUM_HANDSHAKE_MESSAGE_SIZE is handed out
/// whole, and a header announcing one byte more closes the connection as soon as it is read. Once allowed, such a
/// message is handed out and reads ask for more.
BOOST_AUTO_TEST_CASE( handshake_limits_until_full_size_messages_are_allowed )
{
  const message largest_handshake_message = make_message( 5001, GRAPHENE_NET_MAXIMUM_HANDSHAKE_MESSAGE_SIZE, 'h' );
  const message too_large_for_handshake = make_message( 5002, GRAPHENE_NET_MAXIMUM_HANDSHAKE_MESSAGE_SIZE + 1, 'f' );
  {
    raw_sender_pair pair( false );
    std::vector<char> stream;
    append_message( stream, largest_handshake_message );
    message_header too_large = too_large_for_handshake;
    append_message( stream, too_large, nullptr, 0 );

    pair.write( stream.data(), stream.size() );
    pair.wait_until( [&]() { return pair.receiver_delegate.closed; }, fc::seconds( 10 ) );

    BOOST_CHECK( pair.receiver_delegate.closed );
    BOOST_REQUIRE_EQUAL( pair.receiver_delegate.received.size(), 1u );
    BOOST_CHECK( same_message( pair.receiver_delegate.received[0], largest_handshake_message ) );
    BOOST_CHECK_LE( pair.hooks.largest_read_length, size_t( GRAPHENE_NET_HANDSHAKE_BUFFER_SIZE ) );
  }
  {
    raw_sender_pair pair;
    std::vector<char> stream;
    append_message( stream, too_large_for_handshake );

    pair.write( stream.data(), stream.size() );
    pair.wait_until( [&]() { return !pair.receiver_delegate.received.empty() || pair.receiver_delegate.closed; } );

    BOOST_REQUIRE_EQUAL( pair.receiver_delegate.received.size(), 1u );
    BOOST_CHECK( same_message( pair.receiver_delegate.received[0], too_large_for_handshake ) );
    BOOST_CHECK( !pair.receiver_delegate.closed );
    BOOST_CHECK_GT( pair.hooks.largest_read_length, size_t( GRAPHENE_NET_HANDSHAKE_BUFFER_SIZE ) );
  }
}

/// A connection closed locally by another task while the read loop yields to it, after a run of messages
/// handled without a read, gets none of its still buffered messages handed to the node.
BOOST_AUTO_TEST_CASE( connection_closed_during_the_yield_hands_out_no_further_message )
{
  raw_sender_pair pair;

  // the closer waits until the loop is about to yield, then runs ahead of the rescheduled loop, as a task
  // terminating an inactive peer or disconnecting from it would
  fc::promise<void>::ptr loop_about_to_yield = fc::promise<void>::create( "batched_receive_test yield" );
  bool closer_waiting = false;
  bool yield_reached = false;
  size_t received_at_close = 0;
  fc::future<void> closer = fc::async( [&]() {
    closer_waiting = true;
    fc::future<void>( loop_about_to_yield ).wait();
    received_at_close = pair.receiver_delegate.received.size();
    pair.receiver->close_connection();
  }, "batched_receive_test closer" );
  pair.wait_until( [&]() { return closer_waiting; }, fc::seconds( 10 ) );
  BOOST_REQUIRE( closer_waiting );

  // the loop yields after the first 256 messages it handled, whatever reads they came from
  pair.receiver_delegate.after_message = [&]() {
    if( pair.receiver_delegate.received.size() == 256 )
    {
      yield_reached = true;
      loop_about_to_yield->set_value();
    }
  };

  std::vector<char> stream;
  for( size_t i = 0; i < 600; ++i )
    append_message( stream, make_message( 5000 + i, i % 9, char( 'a' + i % 26 ) ) );
  BOOST_REQUIRE_LE( stream.size(), 64u * 1024u );

  pair.write( stream.data(), stream.size() );
  pair.wait_until( [&]() { return pair.receiver_delegate.closed; } );
  if( !yield_reached )
    loop_about_to_yield->set_value();
  closer.wait();

  BOOST_REQUIRE_MESSAGE( yield_reached, "fewer than 256 messages handled; received " << pair.receiver_delegate.received.size() );
  BOOST_CHECK( pair.receiver_delegate.closed );
  BOOST_CHECK_GE( received_at_close, 256u );
  BOOST_CHECK_LT( received_at_close, 600u );
  BOOST_CHECK_EQUAL( pair.receiver_delegate.received.size(), received_at_close );
}

/// A peer whose messages are all buffered already can't keep the read loop from letting other tasks run for more
/// than 256 messages, however many reads they take: the count of messages handled goes on across reads until the loop
/// yields.
BOOST_AUTO_TEST_CASE( buffered_messages_over_many_reads_still_yield_every_256 )
{
  synchronous_reading_hooks hooks( 64 );
  raw_sender_pair pair;
  // a read the loop may be waiting in already takes the first message, the reads after it go through the hooks
  pair.receiver->get_socket().set_io_hooks( &hooks );
  std::vector<char> first;
  append_message( first, make_message( 4999, 0, 'f' ) );
  pair.write( first.data(), first.size() );
  pair.wait_until( [&]() { return pair.receiver_delegate.received.size() == 1; } );
  BOOST_REQUIRE_EQUAL( pair.receiver_delegate.received.size(), 1u );

  std::vector<char> stream;
  const size_t count = 2000;
  for( size_t i = 0; i < count; ++i )
    append_message( stream, make_message( 5000 + i, i % 9, char( 'a' + i % 26 ) ) );
  BOOST_REQUIRE_LE( stream.size(), 64u * 1024u );

  // a task that every handled message makes ready, so it runs whenever the loop lets others run, and records how
  // many messages the loop had handled by then
  bool stop = false;
  std::vector<size_t> handled_when_others_ran;
  fc::promise<void>::ptr wake;
  bool woken = false;
  const auto wake_other = [&]() {
    if( wake && !woken )
    {
      woken = true;
      wake->set_value();
    }
  };
  pair.receiver_delegate.after_message = wake_other;
  fc::future<void> other = fc::async( [&]() {
    while( !stop )
    {
      handled_when_others_ran.push_back( pair.receiver_delegate.received.size() );
      woken = false;
      wake = fc::promise<void>::create( "batched_receive_test wake" );
      fc::future<void>( wake ).wait();
    }
  }, "batched_receive_test other task" );

  pair.write( stream.data(), stream.size() );
  pair.wait_until( [&]() { return pair.receiver_delegate.received.size() >= count + 1 || pair.receiver_delegate.closed; } );
  stop = true;
  wake_other();
  other.wait();

  BOOST_REQUIRE_EQUAL( pair.receiver_delegate.received.size(), count + 1 );
  size_t longest_run = 0;
  for( size_t i = 1; i < handled_when_others_ran.size(); ++i )
    longest_run = std::max( longest_run, handled_when_others_ran[i] - handled_when_others_ran[i - 1] );
  BOOST_CHECK_LE( longest_run, 256u );
  BOOST_CHECK( !pair.receiver_delegate.closed );
}

BOOST_AUTO_TEST_SUITE_END()
