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

    template< typename ConnectionType >
    class http_connection_impl final : public http_connection
    {
    public:
      typedef typename ConnectionType::element_type element_type; // requires ConnectionType to be a std::shared_ptr
      typedef typename std::enable_if<
           std::is_base_of< http_server_impl, typename std::decay< element_type >::type >::value
        || std::is_base_of< http_client_impl, typename std::decay< element_type >::type >::value,
       ConnectionType >::type connection_type; // Should be available only for http server and http client implementations

      http_connection_impl( connection_type con ) : _http_connection( con ) {}
      virtual ~http_connection_impl() {};

      virtual void send_message( const std::string& message ) override
      {
        idump((message));
        _http_connection->send( message );
      }

      virtual void close( int64_t code, const std::string& reason ) override
      {
        _http_connection->close( code, reason );
      }

    private:
      connection_type _http_connection;
    };
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
