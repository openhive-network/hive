#include <boost/test/unit_test.hpp>

#include <fc/network/http/websocket.hpp>

#include <iostream>

BOOST_AUTO_TEST_SUITE(fc_network)

BOOST_AUTO_TEST_CASE(websocket_test)
{
  try
  {
    fc::http::websocket_client client;
    fc::http::connection_ptr s_conn, c_conn;

    {
      ilog("Before server");
      fc::http::websocket_server server;

      ilog("Before on connection");
      server.on_connection( [&]( const fc::http::connection_ptr& c ){
        s_conn = c;
        c->on_message_handler( [&](const std::string& s){
          c->send_message("echo: " + s);
        } );
      } );

      ilog("Before listen");
      server.listen( 8090 );
      ilog("Before start accept");
      server.start_accept();

      std::string echo;
      ilog("Before connect");
      c_conn = client.connect( "ws://localhost:8090" );
      c_conn->on_message_handler( [&](const std::string& s){
        echo = s;
      } );

      ilog("Before send");
      c_conn->send_message( "hello world" );
      fc::usleep( fc::seconds(1) );
      BOOST_CHECK_EQUAL("echo: hello world", echo);

      c_conn->send_message( "again" );
      fc::usleep( fc::seconds(1) );
      BOOST_CHECK_EQUAL("echo: again", echo);

      s_conn->close(0, "test");
      fc::usleep( fc::seconds(1) );
      BOOST_REQUIRE_THROW( c_conn->send_message( "again" ), fc::assert_exception );

      c_conn = client.connect( "ws://localhost:8090" );
      c_conn->on_message_handler( [&](const std::string& s){
        echo = s;
      } );

      c_conn->send_message( "hello world" );
      fc::usleep( fc::seconds(1) );
      BOOST_CHECK_EQUAL("echo: hello world", echo);
    }

    BOOST_REQUIRE_THROW( c_conn->send_message( "again" ), fc::assert_exception );

    BOOST_REQUIRE_THROW( c_conn = client.connect( "ws://localhost:8090" ), fc::assert_exception );

  } FC_CAPTURE_LOG_AND_RETHROW(());
}

BOOST_AUTO_TEST_SUITE_END()
