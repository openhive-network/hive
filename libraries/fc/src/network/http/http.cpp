#include <fc/network/http/http.hpp>
#include <fc/thread/thread.hpp>
#include <fc/network/tcp_socket.hpp>
#include <fc/io/sstream.hpp>
#include <fc/network/ip.hpp>
#include <fc/io/stdio.hpp>
#include <fc/log/logger.hpp>
#include <fc/network/url.hpp>
#include <boost/system/error_code.hpp>
#include <string>

namespace fc { namespace http {

  namespace detail {

    template< typename ConnectionType >
    class http_connection_impl : public http_connection
    {
    public:
      typedef ConnectionType connection_type;

      http_connection_impl( connection_type con )
        : _http_connection( con )
      {}

      virtual ~http_connection_impl() = default;

      virtual void send_message( const std::string& message )override
      {
        idump((message));
        auto ec = _http_connection->send( message );
        FC_ASSERT( !ec, "http send failed: ${msg}", ("msg",ec.message() ) );
      }
      virtual void close( int64_t code, const std::string& reason )override
      {
          _http_connection->close(code,reason);
      }

      connection_type _http_connection;
    };

    class http_server_impl
    {
    public:
      http_server_impl()
      {}
    };

    class http_tls_server_impl
    {
    public:
      http_tls_server_impl( const std::string& server_pem, const std::string& ssl_password )
      {}
    };

    class http_client_impl
    {
    public:
      http_client_impl()
        : _client_thread( fc::thread::current() )
      {}

      fc::promise<void>::ptr  _connected;
      fc::url                 _url;

    private:
      fc::thread&             _client_thread;
    };

    class http_tls_client_impl
    {
    public:
      http_tls_client_impl( const std::string& ca_filename )
        : _client_thread( fc::thread::current() )
      {}

      fc::promise<void>::ptr  _connected;
      fc::url                 _url;

    private:
      fc::thread&             _client_thread;
    };
  } // namespace detail

  http_server::http_server()
      : server(), my( new detail::http_server_impl() ) {}
  http_server::~http_server() {}

  void http_server::on_connection( const on_connection_handler& handler )
  {}
  void http_server::listen( uint16_t port )
  {}
  void http_server::listen( const fc::ip::endpoint& ep )
  {}
  void http_server::start_accept()
  {}


  http_tls_server::http_tls_server( const std::string& server_pem, const std::string& ssl_password )
    : server( server_pem, ssl_password ), my( new detail::http_tls_server_impl(server_pem, ssl_password) ) {}
  http_tls_server::~http_tls_server() {}

  void http_tls_server::on_connection( const on_connection_handler& handler )
  {}
  void http_tls_server::listen( uint16_t port )
  {}
  void http_tls_server::listen( const fc::ip::endpoint& ep )
  {}
  void http_tls_server::start_accept()
  {}


  http_client::http_client()
    : client(), my( new detail::http_client_impl() ) {}
  http_client::~http_client() {}

  connection_ptr http_client::connect( const std::string& _url_str )
  { try {
    fc::url _url{ _url_str };
    FC_ASSERT( _url.proto() == "http", "Invalid protocol: \"{proto}\". Expected: \"http\"", ("proto", _url.proto()) );
    my->_url = _url;

    my->_connected = fc::promise<void>::ptr( new fc::promise<void>("http::connect") );

    boost::system::error_code ec;


    FC_ASSERT( !ec, "${con_desc}: Error: ${ec_msg}", ("con_desc",my->_connected->get_desc())("ec_msg",ec.message()) );

    my->_connected->wait();
    return nullptr;
  } FC_CAPTURE_AND_RETHROW( (_url_str) )}


  http_tls_client::http_tls_client( const std::string& ca_filename )
    : client( ca_filename ), my( new detail::http_tls_client_impl( ca_filename ) ) {}
  http_tls_client::~http_tls_client() {}

  connection_ptr http_tls_client::connect( const std::string& _url_str )
  { try {
    fc::url _url{ _url_str };
    FC_ASSERT( _url.proto() == "https", "Invalid protocol: \"{proto}\". Expected: \"https\"", ("proto", _url.proto()) );
    my->_url = _url;

    my->_connected = fc::promise<void>::ptr( new fc::promise<void>("https::connect") );

    boost::system::error_code ec;


    FC_ASSERT( !ec, "${con_desc}: Error: ${ec_msg}", ("con_desc",my->_connected->get_desc())("ec_msg",ec.message()) );

    my->_connected->wait();
    return nullptr;
  } FC_CAPTURE_AND_RETHROW( (_url_str) ) }

} } // fc::http
