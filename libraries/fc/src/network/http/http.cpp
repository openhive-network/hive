#include <fc/network/http/http.hpp>
#include <fc/network/http/websocketpp_types.hpp>
#include <websocketpp/server.hpp>
#include <websocketpp/client.hpp>

#include <future>
#include <type_traits>

#include <fc/optional.hpp>
#include <fc/variant.hpp>
#include <fc/thread/thread.hpp>
#include <fc/asio.hpp>
#include <fc/network/http/connection.hpp>

#include <boost/asio/io_service.hpp>

namespace fc { namespace http {

  namespace detail {

    template< typename T >
    class http_connection_impl : public http_connection
    {
    public:
      typedef typename std::decay<T>::type connection_ptr;

      http_connection_impl( bool is_server, connection_ptr con )
        : _http_connection( con )
      {
        http_connection::_is_server = is_server;
      }

      virtual ~http_connection_impl() {}

      virtual std::string send_request( const std::string& method, const std::string& path, const std::string& body )override
      {
        FC_ASSERT( !is_server(), "Server cannot send messages" );

        typename asio_with_stub_log::request_type req;

        req.set_method( method );
        req.set_uri( path );
        req.set_version( "HTTP/1.1" );

        req.replace_header( "Host", _http_connection->get_host() + ":" + std::to_string( _http_connection->get_port() ) );
        req.replace_header( "Accept", "*/*" );
        req.replace_header( "Content-Type", "application/json" );

        req.set_body( body );

        // Send the request.
        boost::system::error_code ec;
        boost::asio::write(
          _http_connection->get_socket(),
          boost::asio::buffer( req.raw() ),
          ec
        );
        FC_ASSERT( !ec, "Transfer error: ${ecm}", ("ecm",ec.message()) );

        boost::asio::streambuf response_buf;
        while( boost::asio::read( _http_connection->get_socket(), response_buf, ec ) );
        FC_ASSERT( !ec || ec == boost::asio::error::eof, "Receive error: ${ecm}", ("ecm",ec.message()) );

        typename asio_with_stub_log::response_type res;
        res.consume( static_cast< const char* >( response_buf.data().data() ), response_buf.size() );
        auto response_body = res.get_body();

        wdump((response_body));
        return response_body;
      }
      virtual void close( int64_t code, const std::string& reason )override
      {
        _http_connection->close(code,reason);
      }

      bool is_server()const override
      {
        return http_connection::_is_server;
      }

      connection_ptr _http_connection;
    };

    class http_server_impl
    {
    public:
      http_server_impl()
      {
        _server.clear_access_channels( websocketpp::log::alevel::all );
        _server.clear_error_channels( websocketpp::log::elevel::all );
        _server.init_asio( &_io_service );
        _server.set_reuse_addr(true);
        _server.set_http_handler( [&]( connection_hdl hdl ){
          auto current_con = std::make_shared<http_connection_impl<server_type::connection_ptr>>( true, _server.get_con_from_hdl(hdl) );
          _on_connection( current_con );

          auto con = _server.get_con_from_hdl(hdl);
          con->defer_http_response();
          std::string request_body = con->get_request_body();
          wdump(("server")(request_body));

          std::async([current_con, request_body, con] {
            std::string response = current_con->on_http(request_body);
            con->set_body( response );
            con->set_status( websocketpp::http::status_code::ok );
            con->send_http_response();
            current_con->closed();
          });
        });

        static const auto terminate_handler = [&]( connection_hdl hdl )
        {
          if( _closed )
            _closed->set_value();
        };

        _server.set_close_handler( terminate_handler );
        _server.set_fail_handler( [&]( connection_hdl hdl ){
          if( _server.is_listening() )
            terminate_handler( hdl );
        });
      }
      void stop()
      {
        if( _server_thread )
        {
          _io_service.stop();
          _server_thread->join();
          _server_thread.reset();
        }
      }
      void run()
      {
        stop();
        _server_thread = std::make_shared< std::thread >( [&] { _server.run(); } );
      }
      ~http_server_impl()
      {
        if( _server.is_listening() )
          _server.stop_listening();

        if( _closed ) _closed->wait();

        stop();
      }

      thread_ptr              _server_thread;
      boost::asio::io_service _io_service;
      server_type             _server;
      on_connection_handler   _on_connection;
      fc::promise<void>::ptr  _closed;
    };

    class http_tls_server_impl
    {
    public:
      http_tls_server_impl( const string& server_pem, const string& ssl_password )
      {
        _server.set_tls_init_handler( [=]( websocketpp::connection_hdl hdl ) -> context_ptr {
          context_ptr ctx = websocketpp::lib::make_shared<boost::asio::ssl::context>(boost::asio::ssl::context::tlsv1);
          try {
            ctx->set_options(boost::asio::ssl::context::default_workarounds |
            boost::asio::ssl::context::no_sslv2 |
            boost::asio::ssl::context::no_sslv3 |
            boost::asio::ssl::context::single_dh_use);
            ctx->set_password_callback([=](std::size_t max_length, boost::asio::ssl::context::password_purpose){ return ssl_password;});
            ctx->use_certificate_chain_file(server_pem);
            ctx->use_private_key_file(server_pem, boost::asio::ssl::context::pem);
          } catch (std::exception& e) {
            std::cout << e.what() << std::endl;
          }
          return ctx;
        });

        _server.clear_access_channels( websocketpp::log::alevel::all );
        _server.clear_error_channels( websocketpp::log::elevel::all );
        _server.init_asio( &_io_service );
        _server.set_reuse_addr(true);
        _server.set_http_handler( [&]( connection_hdl hdl ){
          auto current_con = std::make_shared<http_connection_impl<tls_server_type::connection_ptr>>( true, _server.get_con_from_hdl(hdl) );
          _on_connection( current_con );

          auto con = _server.get_con_from_hdl(hdl);
          con->defer_http_response();
          std::string request_body = con->get_request_body();
          wdump(("server")(request_body));

          std::async([current_con, request_body, con] {
            std::string response = current_con->on_http(request_body);
            con->set_body( response );
            con->set_status( websocketpp::http::status_code::ok );
            con->send_http_response();
            current_con->closed();
          });
        });

        static const auto terminate_handler = [&]( connection_hdl hdl )
        {
          if( _closed )
            _closed->set_value();
        };

        _server.set_close_handler( terminate_handler );
        _server.set_fail_handler( [&]( connection_hdl hdl ){
          if( _server.is_listening() )
            terminate_handler( hdl );
        });
      }
      void stop()
      {
        if( _server_thread )
        {
          _io_service.stop();
          _server_thread->join();
          _server_thread.reset();
        }
      }
      void run()
      {
        stop();
        _server_thread = std::make_shared< std::thread >( [&] { _server.run(); } );
      }
      ~http_tls_server_impl()
      {
        if( _server.is_listening() )
          _server.stop_listening();

        if( _closed ) _closed->wait();

        stop();
      }

      thread_ptr                 _server_thread;
      boost::asio::io_service    _io_service;
      tls_server_type            _server;
      on_connection_handler      _on_connection;
      fc::promise<void>::ptr     _closed;
    };

    template< typename T >
    class http_client_impl
    {
    public:
      typedef typename std::decay< T >::type client_type;
      typedef typename client_type::message_ptr message_ptr;

      http_client_impl()
      {
        _client.clear_access_channels( websocketpp::log::alevel::all );
        _client.clear_error_channels( websocketpp::log::elevel::all );
        _client.set_close_handler( [=]( connection_hdl hdl ){
          if( _connection )
            _connection->closed();
          _connection.reset();
          if( _closed ) _closed->set_value();
        });
        _client.set_fail_handler( [=]( connection_hdl hdl ){
          auto con = _client.get_con_from_hdl(hdl);
          auto message = con->get_ec().message();
          if( _connection )
            _connection->closed();
          _connection.reset();
          if( _connected && !_connected->ready() )
            _connected->set_exception( exception_ptr( new FC_EXCEPTION( exception, "${message}", ("message",message)) ) );
          if( _closed )
            _closed->set_value();
        });

        _client.init_asio( &_io_service );
      }
      void stop()
      {
        if( _client_thread )
        {
          _io_service.stop();
          _client_thread->join();
          _client_thread.reset();
        }
      }
      void run()
      {
        stop();
        _client_thread = std::make_shared< std::thread >( [&] { _io_service.run(); } );
      }
      ~http_client_impl()
      {
        if( _connection )
        {
          _connection->close(0, "client closed");
          _connection.reset();
          _closed->wait();
        }
        stop();
      }

      fc::promise<void>::ptr   _connected;
      fc::promise<void>::ptr   _closed;
      thread_ptr               _client_thread;
      boost::asio::io_service  _io_service;
      client_type              _client;
      http_connection_ptr _connection;
      std::string              _uri;
    };

    class http_tls_client_impl : public http_client_impl< tls_client_type >
    {
    public:
      http_tls_client_impl( std::string ca_filename )
        : http_client_impl()
      {
        // ca_filename has special values:
        // "_none" disables cert checking (potentially insecure!)
        // "_default" uses default CA's provided by OS

        http_client_impl::_client.set_tls_init_handler( [=](websocketpp::connection_hdl) {
          context_ptr ctx = websocketpp::lib::make_shared<boost::asio::ssl::context>(boost::asio::ssl::context::tlsv1);
          try {
            ctx->set_options(boost::asio::ssl::context::default_workarounds |
            boost::asio::ssl::context::no_sslv2 |
            boost::asio::ssl::context::no_sslv3 |
            boost::asio::ssl::context::single_dh_use);

            setup_peer_verify( ctx, ca_filename );
          } catch (std::exception& e) {
            edump((e.what()));
            std::cout << e.what() << std::endl;
          }
          return ctx;
        });
      }

      std::string get_host()const
      {
        return websocketpp::uri( http_client_impl::_uri ).get_host();
      }

      void setup_peer_verify( context_ptr& ctx, const std::string& ca_filename )
      {
        if( ca_filename == "_none" )
          return;
        ctx->set_verify_mode( boost::asio::ssl::verify_peer );
        if( ca_filename == "_default" )
          ctx->set_default_verify_paths();
        else
          ctx->load_verify_file( ca_filename );
        ctx->set_verify_depth(10);
        ctx->set_verify_callback( boost::asio::ssl::rfc2818_verification( get_host() ) );
      }
    };

  } // namespace detail

  http_server::http_server()
    : server(), my( std::make_unique< detail::http_server_impl >() ) {}
  http_server::~http_server(){}

  void http_server::on_connection( const on_connection_handler& handler )
  {
    my->_on_connection = handler;
  }

  void http_server::listen( uint16_t port )
  {
    my->_server.listen(port);
  }
  void http_server::listen( const boost::asio::ip::tcp::endpoint& ep )
  {
    my->_server.listen( ep );
  }

  void http_server::start_accept() {
    my->_server.start_accept();
    my->run();
  }


  http_tls_server::http_tls_server( const string& server_pem, const string& ssl_password )
    : server( server_pem, ssl_password ), my( std::make_unique< detail::http_tls_server_impl >(server_pem, ssl_password) ) {}
  http_tls_server::~http_tls_server(){}

  void http_tls_server::on_connection( const on_connection_handler& handler )
  {
    my->_on_connection = handler;
  }

  void http_tls_server::listen( uint16_t port )
  {
    my->_server.listen(port);
  }
  void http_tls_server::listen( const boost::asio::ip::tcp::endpoint& ep )
  {
    my->_server.listen( ep );
  }

  void http_tls_server::start_accept() {
    my->_server.start_accept();
    my->run();
  }


  http_tls_client::http_tls_client( const std::string& ca_filename )
    : client( ca_filename ) {}
  http_tls_client::~http_tls_client(){ }


  http_client::http_client()
    : client() {}
  http_client::~http_client(){ }

  connection_ptr http_client::connect( const std::string& uri )
  {
    try
    {
      FC_ASSERT( uri.substr(0,5) == "http:" );

      // Create new connection
      auto my = std::make_shared< detail::http_client_impl< detail::client_type > >();

      websocketpp::lib::error_code ec;

      my->_uri = uri;
      my->_connected = fc::promise<void>::ptr( new fc::promise<void>("http::connect") );

      my->_client.set_open_handler( [=]( websocketpp::connection_hdl hdl ){
        my->_connection = std::make_shared<detail::http_connection_impl<detail::client_connection_type>>( false, my->_client.get_con_from_hdl(hdl) );
        my->_closed = fc::promise<void>::ptr( new fc::promise<void>("http::closed") );
        my->_connected->set_value();
      });

      auto con = my->_client.get_connection( uri, ec );

      FC_ASSERT( !ec, "error: ${e}", ("e",ec.message()) );

      my->_client.connect(con);
      my->run();
      my->_connected->wait();
      return my->_connection;
    } FC_CAPTURE_AND_RETHROW( (uri) );
  }

  connection_ptr http_tls_client::connect( const std::string& uri )
  {
    try
    {
      FC_ASSERT( uri.substr(0,6) == "https:" );

      // Create new connection
      auto my = std::make_shared< detail::http_tls_client_impl >( client::ca_filename );

      websocketpp::lib::error_code ec;

      my->_connected = fc::promise<void>::ptr( new fc::promise<void>("http::connect") );

      my->_client.set_open_handler( [=]( websocketpp::connection_hdl hdl ){
        my->_connection = std::make_shared<detail::http_connection_impl<detail::tls_client_connection_type>>( false, my->_client.get_con_from_hdl(hdl) );
        my->_closed = fc::promise<void>::ptr( new fc::promise<void>("http::closed") );
        my->_connected->set_value();
      });

      auto con = my->_client.get_connection( uri, ec );

      FC_ASSERT( !ec, "error: ${e}", ("e",ec.message()) );

      my->_client.connect(con);
      my->run();
      my->_connected->wait();
      return my->_connection;
    } FC_CAPTURE_AND_RETHROW( (uri) );
  }

} } // fc::http
