#include <boost/test/unit_test.hpp>

#include <fc/network/http/http.hpp>

#include <iostream>

BOOST_AUTO_TEST_SUITE(fc_network)

BOOST_AUTO_TEST_CASE(http_test)
{
  try
  {
    fc::http::http_client client;
    fc::http::http_connection_ptr s_conn, c_conn;

    {
      fc::http::http_server server;

      server.on_connection( [&]( const fc::http::connection_ptr& c ){
        s_conn = std::static_pointer_cast< fc::http::http_connection >( c );
        s_conn->on_http_handler( [&](const std::string& s) -> std::string {
          return "echo: " + s;
        } );
      } );

      server.listen( 8093 );
      server.start_accept();

      std::string echo;
      c_conn = std::static_pointer_cast< fc::http::http_connection >( client.connect( "http://localhost:8093" ) );

      BOOST_CHECK_EQUAL( "echo: hello world", c_conn->send_message( "hello world" ) );

      BOOST_CHECK_EQUAL( "echo: again", c_conn->send_message( "again" ) );

      s_conn->close(0, "test");
      fc::usleep( fc::seconds(1) );
      BOOST_REQUIRE_THROW( c_conn->send_message( "again" ), fc::assert_exception );

      c_conn = std::static_pointer_cast< fc::http::http_connection >( client.connect( "http://localhost:8093" ) );

      BOOST_CHECK_EQUAL( "echo: hello world", c_conn->send_message( "hello world" ) );
    }

    BOOST_REQUIRE_THROW( c_conn->send_message( "again" ), fc::assert_exception );

    BOOST_REQUIRE_THROW( client.connect( "http://localhost:8093" ), fc::exception );

  } FC_CAPTURE_LOG_AND_RETHROW(());
}

BOOST_AUTO_TEST_SUITE_END()
