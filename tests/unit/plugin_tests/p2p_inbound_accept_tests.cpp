#ifdef IS_TEST_NET
#include <boost/test/unit_test.hpp>

#include "../db_fixture/witness_fixture.hpp"
#include "../db_fixture/p2p_test_peer.hpp"

#include <hive/plugins/p2p/p2p_plugin.hpp>

#include <fc/network/tcp_socket.hpp>

#include <functional>
#include <memory>
#include <string>

namespace {

using p2p_tests::test_peer;

/// The node listens on loopback only, for the peers the test runs in-process, and has room for one connection.
/// A peer that sends nothing during its handshake is dropped after peer_inactivity_timeout seconds.
const std::string one_slot_p2p_parameters =
  R"({"listen_endpoint":"127.0.0.1:0","accept_incoming_connections":true,)"
  R"("desired_number_of_connections":1,"maximum_number_of_connections":1,"peer_inactivity_timeout":5})";

bool wait_until( const std::function< bool() >& condition, fc::microseconds timeout )
{
  const fc::time_point deadline = fc::time_point::now() + timeout;
  while( !condition() )
  {
    if( fc::time_point::now() >= deadline )
      return false;
    fc::usleep( fc::milliseconds( 10 ) );
  }
  return true;
}

/// A TCP connection to the node that never completes the key exchange, like a port probe: it takes the node's
/// half of the exchange and sends nothing. Its socket lives on @p thread.
class silent_connection
{
public:
  silent_connection( fc::thread& thread, const fc::ip::endpoint& node_endpoint ) : _thread( thread )
  {
    _thread.async( [&]()
    {
      _socket.connect_to( node_endpoint );
      // the node sends its public key once it accepted the connection and started the key exchange
      char node_key[ sizeof( fc::ecc::public_key_data ) ];
      _socket.read( node_key, sizeof( node_key ) );
    } ).wait();
    _ended = _thread.async( [this]()
    {
      char byte;
      try
      {
        _socket.readsome( &byte, 1 );
      }
      catch( const fc::exception& )
      {
        return fc::time_point::now();
      }
      return fc::time_point(); // the node sent more than its key, which it must not before the exchange completes
    } );
  }

  ~silent_connection() { close(); }

  void close()
  {
    _thread.async( [this]() { _socket.close(); } ).wait();
    _ended.wait();
  }

  /// true once the connection ended: the node closed it, or close() did
  bool ended() const { return _ended.ready(); }
  /// when the connection ended, or the epoch if the node sent anything past its key
  fc::time_point ended_at() { return _ended.wait(); }

private:
  fc::thread& _thread;
  fc::tcp_socket _socket;
  fc::future< fc::time_point > _ended;
};

struct p2p_inbound_accept_fixture : public witness_fixture
{
  fc::ip::endpoint node_endpoint()
  {
    return theApp.get_plugin< hive::plugins::p2p::p2p_plugin >().get_info()[ "listening_on" ].as< fc::ip::endpoint >();
  }

  /// Connects @p peer and waits for the node's answer to its hello: true if it was accepted, false if rejected
  bool connect_and_get_answer( test_peer& peer )
  {
    peer.connect( node_endpoint() );
    BOOST_REQUIRE( wait_until( [&]() { return peer.accepted() || peer.closed(); }, fc::seconds( 10 ) ) );
    return peer.accepted();
  }

  /// Starts a node with one connection slot and runs @p test on an API thread, its peers' connections on @p peer_thread
  void run( const std::function< void( fc::thread& peer_thread ) >& test )
  {
    BOOST_SCOPE_EXIT( this_ )
    {
      this_->theApp.generate_interrupt_request();
      this_->theApp.wait4interrupt_request();
      this_->theApp.quit( true );
    } BOOST_SCOPE_EXIT_END

    initialize( 3, {}, { "initminer" }, { config_line_t( { "p2p-parameters", { one_slot_p2p_parameters } } ) } );
    bool test_passed = false;
    fc::logger::get( "user" ).set_log_level( fc::log_level::info ); // suppress fast confirm broadcast messages

    fc::thread api_thread;
    auto _future = api_thread.async( [&]()
    {
      BOOST_SCOPE_EXIT( this_ ) { this_->theApp.generate_interrupt_request(); } BOOST_SCOPE_EXIT_END
      try
      {
        fc::sleep_until( get_genesis_time() + 2 * HIVE_BLOCK_INTERVAL );
        BOOST_REQUIRE_GT( get_block_num(), 0u );

        fc::thread peer_thread( "test_peers" );
        test( peer_thread );
        test_passed = !theApp.is_interrupt_request();
      }
      CATCH( "API" )
    } );

    _future.wait();
    theApp.wait4interrupt_request();
    theApp.quit( true );
    db = nullptr; // prevent fixture destructor from accessing database after it was closed
    BOOST_REQUIRE( test_passed );
  }
};

} // namespace

// Part of witness_tests so that, like the other tests on a real-time witness node, each runs as a unit of its own.
BOOST_FIXTURE_TEST_SUITE( witness_tests, p2p_inbound_accept_fixture )

/// A connection that closes during the key exchange, like a port probe, frees its slot right away: the hello of
/// a peer that connects at once is accepted by the node, which has room for one connection. The node counts the
/// handshaking hello sender itself, so it accepts the hello only once the probe no longer counts. Had the probe
/// kept its slot until the node drops inactive handshakes (peer_inactivity_timeout), the hello would be rejected.
BOOST_AUTO_TEST_CASE( failed_inbound_accept_frees_its_slot_at_once )
{
  try
  {
    run( [&]( fc::thread& peer_thread )
    {
      {
        silent_connection probe( peer_thread, node_endpoint() );
        probe.close();
      }
      const fc::time_point probe_closed = fc::time_point::now();

      test_peer peer( peer_thread, db->get_chain_id(), "peer after probe" );
      const bool accepted = connect_and_get_answer( peer );
      const fc::microseconds answered_after = fc::time_point::now() - probe_closed;
      ilog( "hello ${answer} ${ms} ms after the probe closed",
            ( "answer", accepted ? "accepted" : "rejected" )( "ms", answered_after.count() / 1000 ) );
      BOOST_REQUIRE( accepted );
      BOOST_CHECK_LT( answered_after.count(), fc::seconds( 1 ).count() );

      // the peer, the node's only connection, completes the handshake and stays connected
      BOOST_REQUIRE( wait_until( [&]()
      {
        return theApp.get_plugin< hive::plugins::p2p::p2p_plugin >().get_info()[ "connection_count" ].as< uint32_t >() == 1;
      }, fc::seconds( 10 ) ) );
      BOOST_CHECK( !peer.closed() );
    } );
  }
  FC_LOG_AND_RETHROW()
}

/// A connection that stays open but sends nothing during the key exchange holds its slot: a peer's hello is
/// rejected meanwhile. The node drops it after peer_inactivity_timeout, not before, and then accepts a new peer.
BOOST_AUTO_TEST_CASE( silent_inbound_handshake_is_dropped_after_inactivity_timeout )
{
  try
  {
    run( [&]( fc::thread& peer_thread )
    {
      const uint32_t inactivity_timeout =
        theApp.get_plugin< hive::plugins::p2p::p2p_plugin >().get_advanced_node_parameters().peer_inactivity_timeout;

      const fc::time_point silent_opened = fc::time_point::now();
      silent_connection silent( peer_thread, node_endpoint() );

      {
        test_peer rejected_peer( peer_thread, db->get_chain_id(), "peer during silent handshake" );
        BOOST_REQUIRE( !connect_and_get_answer( rejected_peer ) );
      }

      BOOST_REQUIRE( wait_until( [&]() { return silent.ended(); }, fc::seconds( inactivity_timeout + 10 ) ) );
      const fc::time_point silent_dropped = silent.ended_at();
      BOOST_REQUIRE( silent_dropped != fc::time_point() );
      ilog( "silent handshake dropped ${ms} ms after it opened", ( "ms", ( silent_dropped - silent_opened ).count() / 1000 ) );
      BOOST_CHECK_GE( ( silent_dropped - silent_opened ).count(), fc::seconds( inactivity_timeout ).count() );

      test_peer peer( peer_thread, db->get_chain_id(), "peer after silent handshake" );
      BOOST_REQUIRE( connect_and_get_answer( peer ) );
    } );
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_SUITE_END()

#endif
