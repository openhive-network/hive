#include <boost/test/unit_test.hpp>

#include <fc/network/http/http.hpp>
#include <fc/exception/exception.hpp>

BOOST_AUTO_TEST_SUITE( fc_network )

BOOST_AUTO_TEST_CASE( http_test )
{
  fc::http::http_client client;
  fc::http::http_connection_ptr s_conn, c_conn;
  {
    fc::http::http_server server;
    server.on_connection( [&]( const fc::http::connection_ptr& c ) {
      s_conn = c;
      c->on_message_handler( [&]( const std::string& s ) {
        c->send_message("echo: " + s);
      });
    });

    server.listen( 8093 );
    server.start_accept();

    std::string echo;
    c_conn = client.connect( "http://localhost:8093" );
    c_conn->on_message_handler( [&]( const std::string& s ) {
      echo = s;
    });
    c_conn->send_message( "hello world" );
    fc::usleep( fc::seconds(1) );
    BOOST_CHECK_EQUAL("echo: hello world", echo);

    c_conn->send_message( "again" );
    fc::usleep( fc::seconds(1) );
    BOOST_CHECK_EQUAL("echo: again", echo);

    s_conn->close(0, "test");
    fc::usleep( fc::seconds(1) );
    try {
      c_conn->send_message( "again" );
      BOOST_FAIL("expected assertion failure");
    } FC_CAPTURE_AND_LOG(());

    c_conn = client.connect( "http://localhost:8093" );
    c_conn->on_message_handler( [&]( const std::string& s ) {
        echo = s;
    } );
    c_conn->send_message( "hello world" );
    fc::usleep( fc::seconds(1) );
    BOOST_CHECK_EQUAL("echo: hello world", echo);
  }

  try {
    c_conn->send_message( "again" );
    BOOST_FAIL( "expected assertion failure" );
  } FC_CAPTURE_AND_LOG(());

  try {
    c_conn = client.connect( "http://localhost:8093" );
    BOOST_FAIL( "expected assertion failure" );
  } FC_CAPTURE_AND_LOG(());
}

BOOST_AUTO_TEST_SUITE_END()
