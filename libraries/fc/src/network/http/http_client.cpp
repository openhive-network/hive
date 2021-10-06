#include <fc/network/http/http.hpp>
#include <fc/network/http/processor.hpp>
#include <fc/thread/thread.hpp>
#include <fc/network/tcp_socket.hpp>
#include <fc/io/sstream.hpp>
#include <fc/network/ip.hpp>
#include <fc/io/stdio.hpp>
#include <fc/log/logger.hpp>
#include <fc/network/url.hpp>
#include <fc/asio.hpp>

#include <boost/system/error_code.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/asio/ssl/rfc2818_verification.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/asio/steady_timer.hpp>

#include <string>
#include <memory>
#include <future>

namespace fc { namespace http {

  namespace detail {
    enum class client_state
    {
      not_connected = 0,
      connected     = 1,
      closed        = 2
    };

    class http_client_impl
    {
    public:
      http_client_impl() {}
      virtual ~http_client_impl() {}

      virtual void connect( const fc::url& _url ) = 0;
      virtual void send( const std::string& message ) = 0;
      virtual void close( int64_t code, const std::string& reason ) = 0;

      std::future<void> get_connected_future()
      {
        return _connected.get_future();
      }

      std::future<void> get_closed_future()
      {
        return _closed.get_future();
      }

      const fc::url& get_url()
      {
        return _url;
      }

      client_state get_state()
      {
        return _state;
      }

    protected:
      std::promise<void>      _connected;
      std::promise<void>      _closed;

      fc::url                 _url;

      client_state            _state = client_state::not_connected;
    };


    class http_unsecure_client_impl final : public http_client_impl
    {
    public:
      http_unsecure_client_impl()
      {}

      virtual ~http_unsecure_client_impl()
      {}

      virtual void connect( const fc::url& _url ) override
      {
        http_client_impl::_url = _url;
        // TODO: Implement connecting over unsecured TCP connections
        _state = client_state::connected;
        _connected.set_value();
      }

      virtual void send( const std::string& message )
      {
        // TODO: Implement sending messages over unsecured TCP connections
      }

      virtual void close( int64_t code, const std::string& reason )
      {
        ilog( "Closing http secure connection with code: ${code}, and reason: ${reason}", ("code",code)("reason",reason) );
        // TODO: Implement closing connection over unsecured TCP connections
        _state = client_state::closed;
        _closed.set_value();
      }
    };

    class http_tls_client_impl final : public http_client_impl
    {
    public:
      http_tls_client_impl( const std::string& ca_filename )
        : ca_filename( ca_filename )
      {}

      virtual ~http_tls_client_impl()
      {}

      virtual void connect( const fc::url& _url ) override
      {
        http_client_impl::_url = _url;
        // TODO: Implement connecting over secured TCP connections
        _state = client_state::connected;
        _connected.set_value();
      }

      virtual void send( const std::string& message )
      {
        // TODO: Implement sending messages over secured TCP connections
      }

      virtual void close( int64_t code, const std::string& reason )
      {
        ilog( "Closing http secure connection with code: ${code}, and reason: ${reason}", ("code",code)("reason",reason) );
        // TODO: Implement closing connection over secured TCP connections
        _state = client_state::closed;
        _closed.set_value();
      }

    private:
      std::string     ca_filename;
    };
  } // namespace detail

  http_client::http_client()
    : client() {}
  http_client::~http_client() {}

  connection_ptr http_client::connect( const std::string& _url_str )
  { try {
    fc::url _url{ _url_str };
    FC_ASSERT( _url.proto() == "http", "Invalid protocol: \"{proto}\". Expected: \"http\"", ("proto", _url.proto()) );

    auto my = std::make_unique< detail::http_unsecure_client_impl >(); // Create unique client for every connection

    my->connect( _url );
    my->get_connected_future().wait();

    return std::make_shared< detail::http_connection_impl< std::shared_ptr< detail::http_unsecure_client_impl > > >( std::move( my ) );
  } FC_CAPTURE_AND_RETHROW( (_url_str) )}


  http_tls_client::http_tls_client( const std::string& ca_filename )
    : client( ca_filename ) {}
  http_tls_client::~http_tls_client() {}

  connection_ptr http_tls_client::connect( const std::string& _url_str )
  { try {
    fc::url _url{ _url_str };
    FC_ASSERT( _url.proto() == "https", "Invalid protocol: \"{proto}\". Expected: \"https\"", ("proto", _url.proto()) );

    auto my = std::make_unique< detail::http_tls_client_impl >( client::ca_filename ); // Create unique client for every connection

    my->connect( _url );
    my->get_connected_future().wait();

    return std::make_shared< detail::http_connection_impl< std::shared_ptr< detail::http_tls_client_impl > > >( std::move( my ) );
  } FC_CAPTURE_AND_RETHROW( (_url_str) ) }

} } // fc::http
