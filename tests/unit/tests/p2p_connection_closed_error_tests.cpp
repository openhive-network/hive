#include <boost/test/unit_test.hpp>

#include <graphene/net/node.hpp>
#include <graphene/net/peer_connection.hpp>
#include <graphene/net/peer_database.hpp>

#include <hive/utilities/tempdir.hpp>

#include <fc/filesystem.hpp>

#include <boost/asio/error.hpp>
#include <boost/system/system_error.hpp>

#include <cerrno>
#include <string>
#include <unordered_set>

using graphene::net::peer_connection;
using graphene::net::peer_connection_ptr;
using graphene::net::peer_database;
using graphene::net::potential_peer_record;
using graphene::net::rejection_reason_code;
using fc::ip::endpoint;

namespace {

/// What the peer database record of @p peer looks like after its connection closed.
potential_peer_record record_after_close( const peer_connection& peer )
{
  potential_peer_record record( endpoint::from_string( "172.30.66.20:2001" ) );
  peer.record_connection_closed_in( record );
  return record;
}

/// A connect failure as fc::asio reports it: the text of the boost::system::system_error.
fc::exception connect_error( int error_value )
{
  static constexpr boost::source_location location = BOOST_CURRENT_LOCATION;
  const boost::system::error_code ec( error_value, boost::system::system_category(), &location );
  return fc::exception( FC_LOG_MESSAGE( error, "${message} ", ( "message", boost::system::system_error( ec ).what() ) ) );
}

} // namespace

BOOST_AUTO_TEST_SUITE( p2p_connection_closed_error_tests )

/// A rejection is followed by the remote closing the socket (possibly with its own closing message);
/// the peer database must end up with the rejection, not whatever came after it.
BOOST_AUTO_TEST_CASE( first_recorded_reason_is_kept )
{
  auto peer = peer_connection::make_shared( nullptr );

  peer->record_connection_closed_error( fc::exception( FC_LOG_MESSAGE( info, "Peer rejected our connection, reason: ${reason}",
                                                                       ( "reason", "already connected" ) ) ), false );
  peer->record_connection_closed_error( fc::exception( FC_LOG_MESSAGE( info, "Peer disconnected us: ${msg}",
                                                                       ( "msg", "connection rejected" ) ) ), false );

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

  peer->record_connection_closed_error( fc::exception( FC_LOG_MESSAGE( info, "You rejected my connection request (hello message) so I'm disconnecting" ) ), false );

  const std::string recorded = peer->get_connection_closed_reason().to_string();
  BOOST_CHECK_NE( recorded.find( "You rejected my connection request (hello message) so I'm disconnecting" ), std::string::npos );
  BOOST_CHECK_EQUAL( recorded.find( "lost connection to peer" ), std::string::npos );
}

/// A remote rejection recorded before we decide to disconnect stays the recorded reason.
BOOST_AUTO_TEST_CASE( deliberate_disconnect_keeps_earlier_remote_reason )
{
  auto peer = peer_connection::make_shared( nullptr );

  peer->record_connection_closed_error( fc::exception( FC_LOG_MESSAGE( info, "Peer rejected our connection, reason: ${reason}",
                                                                       ( "reason", "already connected" ) ) ), false );
  peer->record_connection_closed_error( fc::exception( FC_LOG_MESSAGE( info, "You rejected my connection request (hello message) so I'm disconnecting" ) ), false );

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

/// A socket that drops without either side giving a reason is a failure of the peer.
BOOST_AUTO_TEST_CASE( unexpected_drop_marks_peer_failed )
{
  auto peer = peer_connection::make_shared( nullptr );

  const potential_peer_record record = record_after_close( *peer );
  BOOST_CHECK( record.last_failed_time.valid() );
  BOOST_CHECK_NE( record.last_error->to_string().find( "lost connection to peer" ), std::string::npos );
}

/// Our disconnect caused by an error (disconnect_from_peer with caused_by_error) is a failure.
BOOST_AUTO_TEST_CASE( our_error_disconnect_marks_peer_failed )
{
  auto peer = peer_connection::make_shared( nullptr );
  peer->record_connection_closed_error( fc::exception( FC_LOG_MESSAGE( info, "You sent me a block that I didn't ask for" ) ), true );

  BOOST_CHECK( record_after_close( *peer ).last_failed_time.valid() );
}

/// The remote closing with closing_due_to_error set is a failure.
BOOST_AUTO_TEST_CASE( remote_error_closing_message_marks_peer_failed )
{
  auto peer = peer_connection::make_shared( nullptr );
  peer->record_connection_closed_error( fc::exception( FC_LOG_MESSAGE( warn, "Peer is disconnecting us because of an error" ) ), true );

  BOOST_CHECK( record_after_close( *peer ).last_failed_time.valid() );
}

/// A timeout that overwrites an earlier graceful reason must still count as a failure.
BOOST_AUTO_TEST_CASE( timeout_after_graceful_reason_marks_peer_failed )
{
  auto peer = peer_connection::make_shared( nullptr );
  peer->record_connection_closed_error( fc::exception( FC_LOG_MESSAGE( info, "Peer disconnected us: ${msg}", ( "msg", "bye" ) ) ), false );
  peer->connection_closed_error = fc::exception( FC_LOG_MESSAGE( warn, "Terminating handshaking connection due to inactivity" ) );
  peer->connection_closed_by_failure = true;

  BOOST_CHECK( record_after_close( *peer ).last_failed_time.valid() );
}

/// Our own non-error disconnect (too many connections, inactivity, ...) leaves the peer unfailed,
/// but still records why it closed.
BOOST_AUTO_TEST_CASE( our_deliberate_disconnect_does_not_mark_peer_failed )
{
  auto peer = peer_connection::make_shared( nullptr );
  peer->record_connection_closed_error( fc::exception( FC_LOG_MESSAGE( info, "I have too many connections open" ) ), false );

  const potential_peer_record record = record_after_close( *peer );
  BOOST_CHECK( !record.last_failed_time.valid() );
  BOOST_REQUIRE( record.last_error.valid() );
  BOOST_CHECK_NE( record.last_error->to_string().find( "I have too many connections open" ), std::string::npos );
}

/// The remote's non-error closing_connection_message leaves the peer unfailed.
BOOST_AUTO_TEST_CASE( remote_graceful_closing_message_does_not_mark_peer_failed )
{
  auto peer = peer_connection::make_shared( nullptr );
  peer->record_connection_closed_error( fc::exception( FC_LOG_MESSAGE( info, "Peer disconnected us: ${msg}", ( "msg", "I have too many connections open" ) ) ), false );

  BOOST_CHECK( !record_after_close( *peer ).last_failed_time.valid() );
}

/// Only redundant-connection rejections are exempt; any other rejection counts as a failure.
BOOST_AUTO_TEST_CASE( rejection_failure_classification )
{
  BOOST_CHECK( !graphene::net::rejection_is_failure( rejection_reason_code::already_connected ) );
  BOOST_CHECK( !graphene::net::rejection_is_failure( rejection_reason_code::connected_to_self ) );
  BOOST_CHECK( graphene::net::rejection_is_failure( rejection_reason_code::not_accepting_connections ) );
  BOOST_CHECK( graphene::net::rejection_is_failure( rejection_reason_code::different_chain ) );
  BOOST_CHECK( graphene::net::rejection_is_failure( rejection_reason_code::blocked ) );
  BOOST_CHECK( graphene::net::rejection_is_failure( rejection_reason_code::unspecified ) );
}

/// The duplicate connection of a dual-stack peer is dropped with an already_connected rejection; the
/// disconnect that follows must not turn that rejection into a failure.
BOOST_AUTO_TEST_CASE( already_connected_rejection_does_not_mark_peer_failed )
{
  auto peer = peer_connection::make_shared( nullptr );
  peer->record_connection_closed_error( fc::exception( FC_LOG_MESSAGE( info, "Peer rejected our connection, reason: ${reason}",
                                                                       ( "reason", "already connected" ) ) ),
                                        graphene::net::rejection_is_failure( rejection_reason_code::already_connected ) );
  peer->record_connection_closed_error( fc::exception( FC_LOG_MESSAGE( info, "Peer disconnected us: ${msg}",
                                                                       ( "msg", "connection rejected" ) ) ), true );

  BOOST_CHECK( !record_after_close( *peer ).last_failed_time.valid() );
}

/// A peer we closed gracefully must still be dialed before one that failed, however long ago the
/// failure was.
BOOST_AUTO_TEST_CASE( gracefully_closed_peer_ranks_ahead_of_failed_peer )
{
  const endpoint graceful_endpoint = endpoint::from_string( "172.30.66.20:2001" );
  const endpoint failed_endpoint = endpoint::from_string( "[fd5e:6::20]:2001" );
  fc::temp_directory dir( hive::utilities::temp_directory_path() );
  peer_database db;
  db.open( dir.path() / "peers.json" );

  auto failed_peer = peer_connection::make_shared( nullptr );
  potential_peer_record failed_record = db.lookup_or_create_entry_for_endpoint( failed_endpoint );
  failed_record.last_seen_time = fc::time_point::now();
  failed_peer->record_connection_closed_in( failed_record );
  db.update_entry( failed_record );

  auto graceful_peer = peer_connection::make_shared( nullptr );
  graceful_peer->record_connection_closed_error( fc::exception( FC_LOG_MESSAGE( info, "I have too many connections open" ) ), false );
  potential_peer_record graceful_record = db.lookup_or_create_entry_for_endpoint( graceful_endpoint );
  graceful_record.last_seen_time = fc::time_point::now() - fc::hours( 1 );
  graceful_peer->record_connection_closed_in( graceful_record );
  db.update_entry( graceful_record );

  BOOST_REQUIRE_EQUAL( db.size(), 2u );
  BOOST_CHECK( db.begin()->endpoint == graceful_endpoint );
  db.close();
}

/// A connection we are still closing (or terminating) holds the endpoint as much as an active one does.
BOOST_AUTO_TEST_CASE( closing_and_terminating_connections_count_as_unclosed )
{
  const endpoint peer_endpoint = endpoint::from_string( "172.30.66.30:2001" );
  const std::unordered_set<peer_connection_ptr> none;

  auto closing_peer = peer_connection::make_shared( nullptr );
  closing_peer->set_remote_endpoint( peer_endpoint );
  const std::unordered_set<peer_connection_ptr> closing{ closing_peer };
  BOOST_CHECK( graphene::net::find_unclosed_connection_to_endpoint( peer_endpoint, none, none, closing, none ) == closing_peer );

  auto terminating_peer = peer_connection::make_shared( nullptr );
  terminating_peer->set_remote_endpoint( peer_endpoint );
  const std::unordered_set<peer_connection_ptr> terminating{ terminating_peer };
  BOOST_CHECK( graphene::net::find_unclosed_connection_to_endpoint( peer_endpoint, none, none, none, terminating ) == terminating_peer );

  BOOST_CHECK( !graphene::net::find_unclosed_connection_to_endpoint( endpoint::from_string( "[fd5e:6::30]:2001" ),
                                                                     none, none, closing, terminating ) );
  BOOST_CHECK( !graphene::net::find_unclosed_connection_to_endpoint( peer_endpoint, none, none, none, none ) );
}

/// Right after we disconnect a peer for inactivity its record says it is due to be dialed again;
/// what keeps the connect loop off it is the connection that is still closing.
BOOST_AUTO_TEST_CASE( connect_loop_does_not_dial_endpoint_still_closing )
{
  const endpoint peer_endpoint = endpoint::from_string( "172.30.66.30:2001" );
  const fc::time_point now = fc::time_point::now();
  const fc::microseconds delay_until_retry = fc::seconds( 30 );

  potential_peer_record record( peer_endpoint );
  record.last_connection_disposition = graphene::net::last_connection_succeeded;
  record.last_connection_attempt_time = now - fc::seconds( 5 );
  auto closing_peer = peer_connection::make_shared( nullptr );
  closing_peer->set_remote_endpoint( peer_endpoint );
  closing_peer->record_connection_closed_error( fc::exception( FC_LOG_MESSAGE( warn, "Disconnecting due to inactivity" ) ), false );
  closing_peer->record_connection_closed_in( record );
  BOOST_REQUIRE( graphene::net::is_due_for_connection_attempt( record, delay_until_retry, now ) );

  const std::unordered_set<peer_connection_ptr> none;
  const std::unordered_set<peer_connection_ptr> closing{ closing_peer };
  const bool dialed = !graphene::net::find_unclosed_connection_to_endpoint( peer_endpoint, none, none, closing, none ) &&
                      graphene::net::is_due_for_connection_attempt( record, delay_until_retry, now );
  BOOST_CHECK( !dialed );

  const bool dialed_after_close = !graphene::net::find_unclosed_connection_to_endpoint( peer_endpoint, none, none, none, none ) &&
                                  graphene::net::is_due_for_connection_attempt( record, delay_until_retry, now );
  BOOST_CHECK( dialed_after_close );
}

/// A peer whose last connection failed still waits out its retry delay.
BOOST_AUTO_TEST_CASE( failed_peer_waits_for_retry_delay )
{
  const fc::time_point now = fc::time_point::now();
  potential_peer_record record( endpoint::from_string( "172.30.66.30:2001" ) );
  record.last_connection_disposition = graphene::net::last_connection_failed;
  record.last_connection_attempt_time = now - fc::seconds( 10 );

  BOOST_CHECK( !graphene::net::is_due_for_connection_attempt( record, fc::seconds( 30 ), now ) );
  BOOST_CHECK( graphene::net::is_due_for_connection_attempt( record, fc::seconds( 5 ), now ) );
}

/// EADDRNOTAVAIL from connect is told apart from the errors that do mean the peer failed.
BOOST_AUTO_TEST_CASE( local_endpoint_in_use_error_classification )
{
  BOOST_CHECK( graphene::net::is_local_endpoint_in_use_error( connect_error( EADDRNOTAVAIL ) ) );
  BOOST_CHECK( !graphene::net::is_local_endpoint_in_use_error( connect_error( boost::asio::error::connection_refused ) ) );
  BOOST_CHECK( !graphene::net::is_local_endpoint_in_use_error( connect_error( boost::asio::error::timed_out ) ) );
  BOOST_CHECK( !graphene::net::is_local_endpoint_in_use_error( fc::exception( FC_LOG_MESSAGE( warn, "lost connection to peer" ) ) ) );
}

BOOST_AUTO_TEST_SUITE_END()
