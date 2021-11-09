#pragma once
#include <fc/vector.hpp>
#include <fc/string.hpp>
#include <fc/signals.hpp>
#include <fc/network/ip.hpp>
#include <fc/any.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <memory>

namespace fc { namespace http {

  class connection
  {
  public:
    connection() = default;
    virtual ~connection() = default;

    virtual void close( int64_t code, const std::string& reason ) = 0;

    void on_http_handler( const std::function<std::string(const std::string&)>& h ) { _on_http = h; }
    string on_http( const std::string& message ) { return _on_http(message); }

    virtual bool is_server()const = 0;

    fc::signal<void()> closed;

  protected:
    bool                                    _is_server;

  private:
    std::function<string(const std::string&)> _on_http;
  };

  typedef std::shared_ptr< connection > connection_ptr;

  typedef std::function< void(const connection_ptr&) > on_connection_handler;

  class server
  {
  public:
    server( const std::string& server_pem = std::string{},
            const std::string& ssl_password = std::string{} );
    virtual ~server() = default;

    virtual void on_connection( const on_connection_handler& handler) = 0;
    virtual void listen( uint16_t port ) = 0;
    virtual void listen( const boost::asio::ip::tcp::endpoint& ep ) = 0;
    virtual void start_accept() = 0;

  protected:
    std::string server_pem;
    std::string ssl_password;
  };

  class client
  {
  public:
    client( const std::string& ca_filename = std::string{} );
    virtual ~client() = default;

    virtual connection_ptr connect( const std::string& uri ) = 0;

  protected:
    std::string ca_filename;
  };

} } // fc::http
