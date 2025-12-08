#include <boost/test/unit_test.hpp>

#include <fc/network/tcp_socket.hpp>
#include <fc/network/udp_socket.hpp>
#include <fc/network/ip.hpp>
#include <fc/thread/thread.hpp>
#include <fc/exception/exception.hpp>

#include <thread>
#include <atomic>
#include <chrono>

BOOST_AUTO_TEST_SUITE(fc_sockets)

//////////////////////////////////////////////////////////////////////////////
// TCP Server Listen Tests
//////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE( tcp_server_listen_ipv4 )
{
   fc::tcp_server server;

   // Listen on IPv4 any address
   fc::ip::endpoint ipv4_endpoint(fc::ip::address("0.0.0.0"), 0);
   BOOST_CHECK_NO_THROW( server.listen(ipv4_endpoint) );

   // Verify server is listening
   auto local_ep = server.get_local_endpoint();
   BOOST_CHECK( local_ep.get_address().is_ipv4() );
   BOOST_CHECK( local_ep.port() != 0 ); // OS assigned a port

   server.close();
}

BOOST_AUTO_TEST_CASE( tcp_server_listen_ipv4_specific )
{
   fc::tcp_server server;

   // Listen on loopback
   fc::ip::endpoint ipv4_endpoint(fc::ip::address("127.0.0.1"), 0);
   BOOST_CHECK_NO_THROW( server.listen(ipv4_endpoint) );

   auto local_ep = server.get_local_endpoint();
   BOOST_CHECK( local_ep.get_address().is_ipv4() );
   BOOST_CHECK_EQUAL( fc::string(local_ep.get_address()), "127.0.0.1" );

   server.close();
}

BOOST_AUTO_TEST_CASE( tcp_server_listen_ipv6 )
{
   fc::tcp_server server;

   // Listen on IPv6 any address
   fc::ip::endpoint ipv6_endpoint(fc::ip::address("::"), 0);
   BOOST_CHECK_NO_THROW( server.listen(ipv6_endpoint) );

   auto local_ep = server.get_local_endpoint();
   BOOST_CHECK( local_ep.get_address().is_ipv6() );
   BOOST_CHECK( local_ep.port() != 0 );

   server.close();
}

BOOST_AUTO_TEST_CASE( tcp_server_listen_ipv6_loopback )
{
   fc::tcp_server server;

   // Listen on IPv6 loopback
   fc::ip::endpoint ipv6_endpoint(fc::ip::address("::1"), 0);
   BOOST_CHECK_NO_THROW( server.listen(ipv6_endpoint) );

   auto local_ep = server.get_local_endpoint();
   BOOST_CHECK( local_ep.get_address().is_ipv6() );
   BOOST_CHECK_EQUAL( fc::string(local_ep.get_address()), "::1" );

   server.close();
}

BOOST_AUTO_TEST_CASE( tcp_server_listen_port_only )
{
   fc::tcp_server server;

   // Listen on port only (should default to IPv4)
   BOOST_CHECK_NO_THROW( server.listen(0) );

   auto local_ep = server.get_local_endpoint();
   BOOST_CHECK( local_ep.get_address().is_ipv4() );
   BOOST_CHECK( local_ep.port() != 0 );

   server.close();
}

//////////////////////////////////////////////////////////////////////////////
// TCP Socket Bind Tests
//////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE( tcp_socket_bind_ipv4 )
{
   fc::tcp_socket socket;

   fc::ip::endpoint local_ep(fc::ip::address("127.0.0.1"), 0);
   BOOST_CHECK_NO_THROW( socket.bind(local_ep) );

   auto bound_ep = socket.local_endpoint();
   BOOST_CHECK( bound_ep.get_address().is_ipv4() );
   BOOST_CHECK_EQUAL( fc::string(bound_ep.get_address()), "127.0.0.1" );

   socket.close();
}

BOOST_AUTO_TEST_CASE( tcp_socket_bind_ipv6 )
{
   fc::tcp_socket socket;

   fc::ip::endpoint local_ep(fc::ip::address("::1"), 0);
   BOOST_CHECK_NO_THROW( socket.bind(local_ep) );

   auto bound_ep = socket.local_endpoint();
   BOOST_CHECK( bound_ep.get_address().is_ipv6() );
   BOOST_CHECK_EQUAL( fc::string(bound_ep.get_address()), "::1" );

   socket.close();
}

//////////////////////////////////////////////////////////////////////////////
// TCP Socket Connect Tests
//////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE( tcp_socket_connect_ipv4 )
{
   // Set up a server on IPv4
   fc::tcp_server server;
   fc::ip::endpoint server_ep(fc::ip::address("127.0.0.1"), 0);
   server.listen(server_ep);
   auto listen_ep = server.get_local_endpoint();

   // Accept connection in a separate fiber
   auto accept_future = fc::async([&server]() {
      fc::tcp_socket accepted_socket;
      server.accept(accepted_socket);
      accepted_socket.close();
   });

   // Connect from client socket
   fc::tcp_socket client;
   BOOST_CHECK_NO_THROW( client.connect_to(listen_ep) );

   // Verify connection details
   auto remote_ep = client.remote_endpoint();
   BOOST_CHECK( remote_ep.get_address().is_ipv4() );
   BOOST_CHECK_EQUAL( remote_ep.port(), listen_ep.port() );

   auto local_ep = client.local_endpoint();
   BOOST_CHECK( local_ep.get_address().is_ipv4() );

   // Wait for accept fiber to complete before closing
   accept_future.wait();

   client.close();
   server.close();
}

BOOST_AUTO_TEST_CASE( tcp_socket_connect_ipv6 )
{
   // Set up a server on IPv6 loopback
   fc::tcp_server server;
   fc::ip::endpoint server_ep(fc::ip::address("::1"), 0);
   server.listen(server_ep);
   auto listen_ep = server.get_local_endpoint();

   BOOST_CHECK( listen_ep.get_address().is_ipv6() );

   // Accept connection in a separate fiber
   auto accept_future = fc::async([&server]() {
      fc::tcp_socket accepted_socket;
      server.accept(accepted_socket);
      accepted_socket.close();
   });

   // Connect from client socket
   fc::tcp_socket client;
   BOOST_CHECK_NO_THROW( client.connect_to(listen_ep) );

   // Verify connection uses IPv6
   auto remote_ep = client.remote_endpoint();
   BOOST_CHECK( remote_ep.get_address().is_ipv6() );
   BOOST_CHECK_EQUAL( fc::string(remote_ep.get_address()), "::1" );
   BOOST_CHECK_EQUAL( remote_ep.port(), listen_ep.port() );

   auto local_ep = client.local_endpoint();
   BOOST_CHECK( local_ep.get_address().is_ipv6() );

   // Wait for accept fiber to complete before closing
   accept_future.wait();

   client.close();
   server.close();
}

//////////////////////////////////////////////////////////////////////////////
// UDP Socket Tests
//////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE( udp_socket_open_default )
{
   fc::udp_socket socket;

   // Default open() should use IPv4
   BOOST_CHECK_NO_THROW( socket.open() );

   socket.close();
}

BOOST_AUTO_TEST_CASE( udp_socket_open_for_ipv4 )
{
   fc::udp_socket socket;

   fc::ip::endpoint ipv4_ep(fc::ip::address("127.0.0.1"), 0);
   BOOST_CHECK_NO_THROW( socket.open_for_endpoint(ipv4_ep) );

   socket.close();
}

BOOST_AUTO_TEST_CASE( udp_socket_open_for_ipv6 )
{
   fc::udp_socket socket;

   fc::ip::endpoint ipv6_ep(fc::ip::address("::1"), 0);
   BOOST_CHECK_NO_THROW( socket.open_for_endpoint(ipv6_ep) );

   socket.close();
}

BOOST_AUTO_TEST_CASE( udp_socket_bind_ipv4 )
{
   fc::udp_socket socket;

   // bind() should auto-open with correct protocol
   fc::ip::endpoint ipv4_ep(fc::ip::address("127.0.0.1"), 0);
   BOOST_CHECK_NO_THROW( socket.bind(ipv4_ep) );

   auto local_ep = socket.local_endpoint();
   BOOST_CHECK( local_ep.get_address().is_ipv4() );
   BOOST_CHECK_EQUAL( fc::string(local_ep.get_address()), "127.0.0.1" );

   socket.close();
}

BOOST_AUTO_TEST_CASE( udp_socket_bind_ipv6 )
{
   fc::udp_socket socket;

   // bind() should auto-open with correct protocol
   fc::ip::endpoint ipv6_ep(fc::ip::address("::1"), 0);
   BOOST_CHECK_NO_THROW( socket.bind(ipv6_ep) );

   auto local_ep = socket.local_endpoint();
   BOOST_CHECK( local_ep.get_address().is_ipv6() );
   BOOST_CHECK_EQUAL( fc::string(local_ep.get_address()), "::1" );

   socket.close();
}

//////////////////////////////////////////////////////////////////////////////
// TCP Server Accept Tests
//////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE( tcp_server_accept_ipv4 )
{
   fc::tcp_server server;
   fc::ip::endpoint server_ep(fc::ip::address("127.0.0.1"), 0);
   server.listen(server_ep);
   auto listen_ep = server.get_local_endpoint();

   std::atomic<bool> connection_verified{false};

   // Accept connection in a separate fiber
   auto accept_future = fc::async([&server, &connection_verified]() {
      fc::tcp_socket accepted_socket;
      server.accept(accepted_socket);

      // Verify the accepted socket has IPv4 endpoints
      auto remote = accepted_socket.remote_endpoint();
      auto local = accepted_socket.local_endpoint();

      connection_verified = remote.get_address().is_ipv4() &&
                           local.get_address().is_ipv4();

      accepted_socket.close();
   });

   // Connect from client
   fc::tcp_socket client;
   client.connect_to(listen_ep);

   // Wait for accept fiber to complete
   accept_future.wait();

   BOOST_CHECK( connection_verified );

   client.close();
   server.close();
}

BOOST_AUTO_TEST_CASE( tcp_server_accept_ipv6 )
{
   fc::tcp_server server;
   fc::ip::endpoint server_ep(fc::ip::address("::1"), 0);
   server.listen(server_ep);
   auto listen_ep = server.get_local_endpoint();

   std::atomic<bool> connection_verified{false};

   // Accept connection in a separate fiber
   auto accept_future = fc::async([&server, &connection_verified]() {
      fc::tcp_socket accepted_socket;
      server.accept(accepted_socket);

      // Verify the accepted socket has IPv6 endpoints
      auto remote = accepted_socket.remote_endpoint();
      auto local = accepted_socket.local_endpoint();

      connection_verified = remote.get_address().is_ipv6() &&
                           local.get_address().is_ipv6();

      accepted_socket.close();
   });

   // Connect from client
   fc::tcp_socket client;
   client.connect_to(listen_ep);

   // Wait for accept fiber to complete
   accept_future.wait();

   BOOST_CHECK( connection_verified );

   client.close();
   server.close();
}

//////////////////////////////////////////////////////////////////////////////
// Data Transfer Tests
//////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE( tcp_data_transfer_ipv4 )
{
   fc::tcp_server server;
   fc::ip::endpoint server_ep(fc::ip::address("127.0.0.1"), 0);
   server.listen(server_ep);
   auto listen_ep = server.get_local_endpoint();

   const std::string test_data = "Hello IPv4!";
   std::string received_data;

   // Accept and receive in a separate fiber
   auto accept_future = fc::async([&server, &received_data]() {
      fc::tcp_socket accepted_socket;
      server.accept(accepted_socket);

      char buffer[256];
      size_t bytes_read = accepted_socket.readsome(buffer, sizeof(buffer));
      received_data = std::string(buffer, bytes_read);

      accepted_socket.close();
   });

   // Connect and send data
   fc::tcp_socket client;
   client.connect_to(listen_ep);
   client.writesome(test_data.c_str(), test_data.size());

   // Wait for receiver fiber to complete
   accept_future.wait();

   BOOST_CHECK_EQUAL( received_data, test_data );

   client.close();
   server.close();
}

BOOST_AUTO_TEST_CASE( tcp_data_transfer_ipv6 )
{
   fc::tcp_server server;
   fc::ip::endpoint server_ep(fc::ip::address("::1"), 0);
   server.listen(server_ep);
   auto listen_ep = server.get_local_endpoint();

   const std::string test_data = "Hello IPv6!";
   std::string received_data;

   // Accept and receive in a separate fiber
   auto accept_future = fc::async([&server, &received_data]() {
      fc::tcp_socket accepted_socket;
      server.accept(accepted_socket);

      char buffer[256];
      size_t bytes_read = accepted_socket.readsome(buffer, sizeof(buffer));
      received_data = std::string(buffer, bytes_read);

      accepted_socket.close();
   });

   // Connect and send data
   fc::tcp_socket client;
   client.connect_to(listen_ep);
   client.writesome(test_data.c_str(), test_data.size());

   // Wait for receiver fiber to complete
   accept_future.wait();

   BOOST_CHECK_EQUAL( received_data, test_data );

   client.close();
   server.close();
}

//////////////////////////////////////////////////////////////////////////////
// UDP Send/Receive Tests
//////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE( udp_send_receive_ipv4 )
{
   fc::udp_socket receiver;
   fc::ip::endpoint recv_ep(fc::ip::address("127.0.0.1"), 0);
   receiver.bind(recv_ep);
   auto bound_ep = receiver.local_endpoint();

   fc::udp_socket sender;
   sender.open();

   const std::string test_data = "Hello UDP IPv4!";

   // Send data
   size_t sent = sender.send_to(test_data.c_str(), test_data.size(), bound_ep);
   BOOST_CHECK_EQUAL( sent, test_data.size() );

   // Receive data
   char buffer[256];
   fc::ip::endpoint from_ep;
   size_t received = receiver.receive_from(buffer, sizeof(buffer), from_ep);

   BOOST_CHECK_EQUAL( received, test_data.size() );
   BOOST_CHECK_EQUAL( std::string(buffer, received), test_data );
   BOOST_CHECK( from_ep.get_address().is_ipv4() );

   sender.close();
   receiver.close();
}

BOOST_AUTO_TEST_CASE( udp_send_receive_ipv6 )
{
   fc::udp_socket receiver;
   fc::ip::endpoint recv_ep(fc::ip::address("::1"), 0);
   receiver.bind(recv_ep);
   auto bound_ep = receiver.local_endpoint();

   BOOST_CHECK( bound_ep.get_address().is_ipv6() );

   fc::udp_socket sender;
   fc::ip::endpoint sender_ep(fc::ip::address("::1"), 0);
   sender.open_for_endpoint(sender_ep);

   const std::string test_data = "Hello UDP IPv6!";

   // Send data
   size_t sent = sender.send_to(test_data.c_str(), test_data.size(), bound_ep);
   BOOST_CHECK_EQUAL( sent, test_data.size() );

   // Receive data
   char buffer[256];
   fc::ip::endpoint from_ep;
   size_t received = receiver.receive_from(buffer, sizeof(buffer), from_ep);

   BOOST_CHECK_EQUAL( received, test_data.size() );
   BOOST_CHECK_EQUAL( std::string(buffer, received), test_data );
   BOOST_CHECK( from_ep.get_address().is_ipv6() );
   BOOST_CHECK_EQUAL( fc::string(from_ep.get_address()), "::1" );

   sender.close();
   receiver.close();
}

BOOST_AUTO_TEST_SUITE_END()
