#pragma once
#include <fc/network/http/connection.hpp>
#include <fc/shared_ptr.hpp>
#include <fc/network/http/processor.hpp>
#include <boost/asio.hpp>
#include <boost/asio/ssl/context.hpp>
#include <vector>
#include <functional>
#include <type_traits>
#include <memory>

namespace fc { namespace http {

  typedef connection                         http_connection;
  typedef std::shared_ptr< http_connection > http_connection_ptr;

  namespace detail {
    typedef boost::asio::ssl::context       ssl_context;
    typedef std::shared_ptr< ssl_context >  ssl_context_ptr;
    typedef boost::asio::io_service*        io_service_ptr;
    typedef std::shared_ptr< processor >    processor_ptr;

    class http_server_impl;
    class http_unsecure_server_impl;
    class http_tls_server_impl;
    class http_client_impl;
    class http_unsecure_client_impl;
    class http_tls_client_impl;
  } // detail

  class http_server : public server
  {
  public:
    http_server();
    virtual ~http_server();

    virtual void on_connection( const on_connection_handler& handler) override;
    virtual void listen( uint16_t port ) override;
    virtual void listen( const fc::ip::endpoint& ep ) override;
    virtual void start_accept() override;

  private:
    typedef std::unique_ptr< detail::http_unsecure_server_impl > server_impl_ptr;
    server_impl_ptr my;
  };


  class http_tls_server : public server
  {
  public:
    http_tls_server( const std::string& server_pem = std::string(),
                const std::string& ssl_password = std::string());
    virtual ~http_tls_server();

    virtual void on_connection( const on_connection_handler& handler) override;
    virtual void listen( uint16_t port ) override;
    virtual void listen( const fc::ip::endpoint& ep ) override;
    virtual void start_accept() override;

  private:
    typedef std::unique_ptr< detail::http_tls_server_impl > server_impl_ptr;
    server_impl_ptr my;
  };

  class http_client : public client
  {
  public:
    http_client();
    virtual ~http_client();

    /// Generates unique client for every connection so you can call multiple times the connect method from one http_client instance
    virtual connection_ptr connect( const std::string& _url_str ) override;
  };
  class http_tls_client : public client
  {
  public:
    http_tls_client( const std::string& ca_filename = "_default" );
    virtual ~http_tls_client();

    /// Generates unique client for every connection so you can call multiple times the connect method from one http_tls_client instance
    virtual connection_ptr connect( const std::string& _url_str ) override;
  };

} } // fc::http
