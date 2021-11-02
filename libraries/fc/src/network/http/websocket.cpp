#include <fc/network/http/websocket.hpp>
#include <websocketpp/config/asio_client.hpp>
#include <websocketpp/config/asio.hpp>
#include <websocketpp/server.hpp>
#include <websocketpp/config/asio_client.hpp>
#include <websocketpp/extensions/permessage_deflate/enabled.hpp>
#include <websocketpp/client.hpp>
#include <websocketpp/logger/stub.hpp>

#include <future>
#include <type_traits>

#include <fc/optional.hpp>
#include <fc/variant.hpp>
#include <fc/thread/thread.hpp>
#include <fc/asio.hpp>
#include <fc/network/http/connection.hpp>

#include <boost/asio/io_service.hpp>

#ifdef DEFAULT_LOGGER
# undef DEFAULT_LOGGER
#endif
#define DEFAULT_LOGGER "rpc"

namespace fc { namespace http {

  namespace detail {

    typedef std::shared_ptr< std::thread >             thread_ptr;

#define FC_HTTP_CONFIG_PERMESSAGE_DEFLATE 1 // Undef this to disable the compression

    struct asio_with_stub_log : public websocketpp::config::asio {
      typedef asio_with_stub_log type;
      typedef asio base;

      typedef base::concurrency_type concurrency_type;

      typedef base::request_type request_type;
      typedef base::response_type response_type;

      typedef base::message_type message_type;
      typedef base::con_msg_manager_type con_msg_manager_type;
      typedef base::endpoint_msg_manager_type endpoint_msg_manager_type;

      typedef websocketpp::log::stub elog_type;
      typedef websocketpp::log::stub alog_type;

      typedef base::rng_type rng_type;

      struct transport_config : public base::transport_config {
        typedef type::concurrency_type concurrency_type;
        typedef type::alog_type alog_type;
        typedef type::elog_type elog_type;
        typedef type::request_type request_type;
        typedef type::response_type response_type;
        typedef websocketpp::transport::asio::basic_socket::endpoint socket_type;
      };

      typedef websocketpp::transport::asio::endpoint<transport_config> transport_type;

      static const long timeout_open_handshake = 0;

#ifdef FC_HTTP_CONFIG_PERMESSAGE_DEFLATE
      struct permessage_deflate_config {};

      typedef websocketpp::extensions::permessage_deflate::enabled<permessage_deflate_config> permessage_deflate_type;
#endif
    };

    struct asio_tls_stub_log : public websocketpp::config::asio_tls {
      typedef asio_tls_stub_log type;
      typedef asio_tls base;

      typedef base::concurrency_type concurrency_type;

      typedef base::request_type request_type;
      typedef base::response_type response_type;

      typedef base::message_type message_type;
      typedef base::con_msg_manager_type con_msg_manager_type;
      typedef base::endpoint_msg_manager_type endpoint_msg_manager_type;

      typedef websocketpp::log::stub elog_type;
      typedef websocketpp::log::stub alog_type;

      typedef base::rng_type rng_type;

      struct transport_config : public base::transport_config {
        typedef type::concurrency_type concurrency_type;
        typedef type::alog_type alog_type;
        typedef type::elog_type elog_type;
        typedef type::request_type request_type;
        typedef type::response_type response_type;
        typedef websocketpp::transport::asio::tls_socket::endpoint socket_type;
      };

      typedef websocketpp::transport::asio::endpoint<transport_config> transport_type;

#ifdef FC_HTTP_CONFIG_PERMESSAGE_DEFLATE
      struct permessage_deflate_config {};

      typedef websocketpp::extensions::permessage_deflate::enabled<permessage_deflate_config> permessage_deflate_type;
#endif
    };

    using websocketpp::connection_hdl;
    typedef websocketpp::server< asio_with_stub_log > websocket_server_type;
    typedef websocketpp::server< asio_tls_stub_log  > websocket_tls_server_type;

    template< typename T >
    class websocket_connection_impl : public websocket_connection
    {
    public:
      typedef typename std::decay<T>::type connection_ptr;

      websocket_connection_impl( connection_ptr con )
        : _ws_connection( con ) {}

      ~websocket_connection_impl() {}

      virtual void send_message( const std::string& message )override
      {
        idump((message));
        auto ec = _ws_connection->send( message );
        FC_ASSERT( !ec, "websocket send failed: ${msg}", ("msg",ec.message() ) );
      }
      virtual void close( int64_t code, const std::string& reason  )override
      {
        _ws_connection->close(code,reason);
      }

      connection_ptr _ws_connection;
    };

    typedef websocketpp::lib::shared_ptr< boost::asio::ssl::context > context_ptr;

    class websocket_server_impl
    {
    public:
      websocket_server_impl()
      {
        _server.clear_access_channels( websocketpp::log::alevel::all );
        _server.clear_error_channels( websocketpp::log::elevel::all );
        _server.init_asio( &_io_service );
        _server.set_reuse_addr(true);
        _server.set_open_handler( [&]( connection_hdl hdl ){
          auto new_con = std::make_shared<websocket_connection_impl<websocket_server_type::connection_ptr>>( _server.get_con_from_hdl(hdl) );
          _on_connection( _connections[hdl] = new_con );
        });
        _server.set_message_handler( [&]( connection_hdl hdl, websocket_server_type::message_ptr msg ){
          auto current_con = _connections.find(hdl);
          FC_ASSERT( current_con != _connections.end(), "Unknown connection" );
          auto payload = msg->get_payload();
          wdump(("server")(payload));
          auto con = current_con->second;
          ++_pending_messages;
          auto f = std::async( [this, con, payload]{ if( _pending_messages ) --_pending_messages; con->on_message( payload ); });
          if( _pending_messages > 100 )
            f.wait();
        });

        _server.set_http_handler( [&]( connection_hdl hdl ){
          auto current_con = std::make_shared<websocket_connection_impl<websocket_server_type::connection_ptr>>( _server.get_con_from_hdl(hdl) );
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
          if( _connections.find(hdl) != _connections.end() )
          {
            _connections[hdl]->closed();
            _connections.erase( hdl );
          }
          else
          {
            wlog( "unknown connection failed" );
          }
          if( _connections.empty() && _closed )
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
      ~websocket_server_impl()
      {
        if( _server.is_listening() )
          _server.stop_listening();

        if( _connections.size() )
          _closed = new fc::promise<void>();

        for( auto con : _connections )
          _server.close( con.first, 0, "server exit" );

        if( _closed ) _closed->wait();

        stop();
      }

      typedef std::map<connection_hdl, websocket_connection_ptr,std::owner_less<connection_hdl> > con_map;

      con_map                 _connections;
      thread_ptr              _server_thread;
      boost::asio::io_service _io_service;
      websocket_server_type   _server;
      on_connection_handler   _on_connection;
      fc::promise<void>::ptr  _closed;
      uint32_t                _pending_messages = 0;
    };

    class websocket_tls_server_impl
    {
    public:
      websocket_tls_server_impl( const string& server_pem, const string& ssl_password )
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
        _server.set_open_handler( [&]( connection_hdl hdl ){
          auto new_con = std::make_shared<websocket_connection_impl<websocket_tls_server_type::connection_ptr>>( _server.get_con_from_hdl(hdl) );
          _on_connection( _connections[hdl] = new_con );
        });
        _server.set_message_handler( [&]( connection_hdl hdl, websocket_server_type::message_ptr msg ){
          auto current_con = _connections.find(hdl);
          FC_ASSERT( current_con != _connections.end(), "Unknown connection" );
          auto payload = msg->get_payload();
          wdump(("server")(payload));
          auto con = current_con->second;
          ++_pending_messages;
          auto f = std::async([this,con,payload]{ if( _pending_messages ) --_pending_messages; con->on_message( payload ); });
          if( _pending_messages > 100 )
            f.wait();
        });

        _server.set_http_handler( [&]( connection_hdl hdl ){
          auto current_con = std::make_shared<websocket_connection_impl<websocket_tls_server_type::connection_ptr>>( _server.get_con_from_hdl(hdl) );
          try{
            _on_connection( current_con );

            auto con = _server.get_con_from_hdl(hdl);
            wdump(("server")(con->get_request_body()));
            auto response = current_con->on_http( con->get_request_body() );

            con->set_body( response );
            con->set_status( websocketpp::http::status_code::ok );
          } catch ( const fc::exception& e )
          {
            edump((e.to_detail_string()));
          }
          current_con->closed();
        });

        static const auto terminate_handler = [&]( connection_hdl hdl )
        {
          if( _connections.find(hdl) != _connections.end() )
          {
            _connections[hdl]->closed();
            _connections.erase( hdl );
          }
          else
          {
            wlog( "unknown connection failed" );
          }
          if( _connections.empty() && _closed )
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
        _server_thread = std::make_shared< std::thread >( [&] { _io_service.run(); } );
      }
      ~websocket_tls_server_impl()
      {
        if( _server.is_listening() )
          _server.stop_listening();

        if( _connections.size() )
          _closed = new fc::promise<void>();

        for( auto con : _connections )
          _server.close( con.first, 0, "server exit" );

        if( _closed ) _closed->wait();

        stop();
      }

      typedef std::map< connection_hdl, websocket_connection_ptr, std::owner_less< connection_hdl > > con_map;

      con_map                    _connections;
      thread_ptr                 _server_thread;
      boost::asio::io_service    _io_service;
      websocket_tls_server_type  _server;
      on_connection_handler      _on_connection;
      fc::promise<void>::ptr     _closed;
      uint32_t                   _pending_messages = 0;
    };


    typedef websocketpp::client< asio_with_stub_log > websocket_client_type;
    typedef websocketpp::client< asio_tls_stub_log  > websocket_tls_client_type;

    typedef websocket_client_type::connection_ptr      websocket_client_connection_type;
    typedef websocket_tls_client_type::connection_ptr  websocket_tls_client_connection_type;

    template< typename T >
    class websocket_client_impl
    {
    public:
      typedef typename std::decay< T >::type client_type;
      typedef typename client_type::message_ptr message_ptr;

      websocket_client_impl()
      {
        _client.clear_access_channels( websocketpp::log::alevel::all );
        _client.clear_error_channels( websocketpp::log::elevel::all );
        _client.set_message_handler( [&]( connection_hdl hdl, message_ptr msg ){
          wdump((msg->get_payload()));
          auto received = msg->get_payload();
          std::async( [this,received](){ if( _connection ) _connection->on_message(received); });
        });
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
      ~websocket_client_impl()
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
      websocket_connection_ptr _connection;
      std::string              _uri;
    };

    class websocket_tls_client_impl : public websocket_client_impl< websocket_tls_client_type >
    {
    public:
      websocket_tls_client_impl( std::string ca_filename )
        : websocket_client_impl()
      {
        // ca_filename has special values:
        // "_none" disables cert checking (potentially insecure!)
        // "_default" uses default CA's provided by OS

        websocket_client_impl::_client.set_tls_init_handler( [=](websocketpp::connection_hdl) {
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
        return websocketpp::uri( websocket_client_impl::_uri ).get_host();
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

  websocket_server::websocket_server()
    : server(), my( std::make_unique< detail::websocket_server_impl >() ) {}
  websocket_server::~websocket_server(){}

  void websocket_server::on_connection( const on_connection_handler& handler )
  {
    my->_on_connection = handler;
  }

  void websocket_server::listen( uint16_t port )
  {
    my->_server.listen(port);
  }
  void websocket_server::listen( const boost::asio::ip::tcp::endpoint& ep )
  {
    my->_server.listen( ep );
  }

  void websocket_server::start_accept() {
    my->_server.start_accept();
    my->run();
  }


  websocket_tls_server::websocket_tls_server( const string& server_pem, const string& ssl_password )
    : server( server_pem, ssl_password ), my( std::make_unique< detail::websocket_tls_server_impl >(server_pem, ssl_password) ) {}
  websocket_tls_server::~websocket_tls_server(){}

  void websocket_tls_server::on_connection( const on_connection_handler& handler )
  {
    my->_on_connection = handler;
  }

  void websocket_tls_server::listen( uint16_t port )
  {
    my->_server.listen(port);
  }
  void websocket_tls_server::listen( const boost::asio::ip::tcp::endpoint& ep )
  {
    my->_server.listen( ep );
  }

  void websocket_tls_server::start_accept() {
    my->_server.start_accept();
    my->run();
  }


  websocket_tls_client::websocket_tls_client( const std::string& ca_filename )
    : client( ca_filename ) {}
  websocket_tls_client::~websocket_tls_client(){ }


  websocket_client::websocket_client()
    : client() {}
  websocket_client::~websocket_client(){ }

  connection_ptr websocket_client::connect( const std::string& uri )
  {
    try
    {
      FC_ASSERT( uri.substr(0,3) == "ws:" );

      // Create new connection
      auto my = std::make_shared< detail::websocket_client_impl< detail::websocket_client_type > >();

      websocketpp::lib::error_code ec;

      my->_uri = uri;
      my->_connected = fc::promise<void>::ptr( new fc::promise<void>("websocket::connect") );

      my->_client.set_open_handler( [=]( websocketpp::connection_hdl hdl ){
        my->_connection = std::make_shared<detail::websocket_connection_impl<detail::websocket_client_connection_type>>( my->_client.get_con_from_hdl(hdl) );
        my->_closed = fc::promise<void>::ptr( new fc::promise<void>("websocket::closed") );
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

  connection_ptr websocket_tls_client::connect( const std::string& uri )
  {
    try
    {
      FC_ASSERT( uri.substr(0,4) == "wss:" );

      // Create new connection
      auto my = std::make_shared< detail::websocket_tls_client_impl >( client::ca_filename );

      websocketpp::lib::error_code ec;

      my->_connected = fc::promise<void>::ptr( new fc::promise<void>("websocket::connect") );

      my->_client.set_open_handler( [=]( websocketpp::connection_hdl hdl ){
        my->_connection = std::make_shared<detail::websocket_connection_impl<detail::websocket_tls_client_connection_type>>( my->_client.get_con_from_hdl(hdl) );
        my->_closed = fc::promise<void>::ptr( new fc::promise<void>("websocket::closed") );
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
