#include <boost/test/unit_test.hpp>

#include <graphene/net/peer_connection.hpp>

#include <string>

using graphene::net::peer_connection;

BOOST_AUTO_TEST_SUITE( p2p_connection_closed_error_tests )

/// A rejection is followed by the remote closing the socket (possibly with its own closing message);
/// the peer database must end up with the rejection, not whatever came after it.
BOOST_AUTO_TEST_CASE( first_recorded_reason_is_kept )
{
  auto peer = peer_connection::make_shared( nullptr );

  peer->record_connection_closed_error( fc::exception( FC_LOG_MESSAGE( info, "Peer rejected our connection, reason: ${reason}",
                                                                       ( "reason", "already connected" ) ) ) );
  peer->record_connection_closed_error( fc::exception( FC_LOG_MESSAGE( info, "Peer disconnected us: ${msg}",
                                                                       ( "msg", "connection rejected" ) ) ) );

  BOOST_REQUIRE( peer->connection_closed_error );
  const std::string recorded = peer->connection_closed_error->to_string();
  BOOST_CHECK_NE( recorded.find( "Peer rejected our connection, reason: already connected" ), std::string::npos );
  BOOST_CHECK_EQUAL( recorded.find( "Peer disconnected us" ), std::string::npos );
}

/// Disconnecting a peer on purpose records why; that reason, not the generic "lost connection to peer",
/// is what the peer database gets when the socket finally closes.
BOOST_AUTO_TEST_CASE( deliberate_disconnect_reason_survives_connection_close )
{
  auto peer = peer_connection::make_shared( nullptr );

  peer->record_connection_closed_error( fc::exception( FC_LOG_MESSAGE( info, "You rejected my connection request (hello message) so I'm disconnecting" ) ) );

  const std::string recorded = peer->get_connection_closed_reason().to_string();
  BOOST_CHECK_NE( recorded.find( "You rejected my connection request (hello message) so I'm disconnecting" ), std::string::npos );
  BOOST_CHECK_EQUAL( recorded.find( "lost connection to peer" ), std::string::npos );
}

/// A remote rejection recorded before we decide to disconnect stays the recorded reason.
BOOST_AUTO_TEST_CASE( deliberate_disconnect_keeps_earlier_remote_reason )
{
  auto peer = peer_connection::make_shared( nullptr );

  peer->record_connection_closed_error( fc::exception( FC_LOG_MESSAGE( info, "Peer rejected our connection, reason: ${reason}",
                                                                       ( "reason", "already connected" ) ) ) );
  peer->record_connection_closed_error( fc::exception( FC_LOG_MESSAGE( info, "You rejected my connection request (hello message) so I'm disconnecting" ) ) );

  const std::string recorded = peer->get_connection_closed_reason().to_string();
  BOOST_CHECK_NE( recorded.find( "Peer rejected our connection, reason: already connected" ), std::string::npos );
  BOOST_CHECK_EQUAL( recorded.find( "You rejected my connection request" ), std::string::npos );
}

/// When the peer just drops the socket nobody recorded a reason.
BOOST_AUTO_TEST_CASE( dropped_connection_reports_lost_connection )
{
  auto peer = peer_connection::make_shared( nullptr );

  BOOST_CHECK( !peer->connection_closed_error );
  BOOST_CHECK_NE( peer->get_connection_closed_reason().to_string().find( "lost connection to peer" ), std::string::npos );
}

BOOST_AUTO_TEST_SUITE_END()
