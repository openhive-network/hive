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

BOOST_AUTO_TEST_SUITE_END()
