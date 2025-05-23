#include <fc/network/http/websocket.hpp>
#include <websocketpp/config/asio_client.hpp>
#include <websocketpp/config/asio.hpp>
#include <websocketpp/server.hpp>
#include <websocketpp/config/asio_client.hpp>
#include <websocketpp/client.hpp>
#include <websocketpp/logger/stub.hpp>

#include <fc/optional.hpp>
#include <fc/variant.hpp>
#include <fc/thread/thread.hpp>
#include <fc/asio.hpp>

#ifdef DEFAULT_LOGGER
# undef DEFAULT_LOGGER
#endif
#define DEFAULT_LOGGER "rpc"

namespace fc { namespace http {

   namespace detail {

      struct asio_with_stub_log : public websocketpp::config::asio {

          typedef asio_with_stub_log type;
          typedef asio base;

          typedef base::concurrency_type concurrency_type;

          typedef base::request_type request_type;
          typedef base::response_type response_type;

          typedef base::message_type message_type;
          typedef base::con_msg_manager_type con_msg_manager_type;
          typedef base::endpoint_msg_manager_type endpoint_msg_manager_type;

          /// Custom Logging policies
          /*typedef websocketpp::log::syslog<concurrency_type,
              websocketpp::log::elevel> elog_type;
          typedef websocketpp::log::syslog<concurrency_type,
              websocketpp::log::alevel> alog_type;
          */
          //typedef base::alog_type alog_type;
          //typedef base::elog_type elog_type;
          typedef websocketpp::log::stub elog_type;
          typedef websocketpp::log::stub alog_type;

          typedef base::rng_type rng_type;

          struct transport_config : public base::transport_config {
              typedef type::concurrency_type concurrency_type;
              typedef type::alog_type alog_type;
              typedef type::elog_type elog_type;
              typedef type::request_type request_type;
              typedef type::response_type response_type;
              typedef websocketpp::transport::asio::basic_socket::endpoint
                  socket_type;
          };

          typedef websocketpp::transport::asio::endpoint<transport_config>
              transport_type;

          static const long timeout_open_handshake = 0;
      };
      struct asio_tls_with_stub_log : public websocketpp::config::asio_tls {

          typedef asio_with_stub_log type;
          typedef asio_tls base;

          typedef base::concurrency_type concurrency_type;

          typedef base::request_type request_type;
          typedef base::response_type response_type;

          typedef base::message_type message_type;
          typedef base::con_msg_manager_type con_msg_manager_type;
          typedef base::endpoint_msg_manager_type endpoint_msg_manager_type;

          /// Custom Logging policies
          /*typedef websocketpp::log::syslog<concurrency_type,
              websocketpp::log::elevel> elog_type;
          typedef websocketpp::log::syslog<concurrency_type,
              websocketpp::log::alevel> alog_type;
          */
          //typedef base::alog_type alog_type;
          //typedef base::elog_type elog_type;
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

          typedef websocketpp::transport::asio::endpoint<transport_config>
              transport_type;

          static const long timeout_open_handshake = 0;
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

         //typedef base::alog_type alog_type;
         //typedef base::elog_type elog_type;
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

         typedef websocketpp::transport::asio::endpoint<transport_config>
         transport_type;
      };

      template<typename T>
      class websocket_connection_impl : public websocket_connection
      {
         public:
            websocket_connection_impl( T con )
            :_ws_connection(con){
            }

            ~websocket_connection_impl()
            {
            }

            virtual void send_message( const std::string& message )override
            {
               idump((message));
               //std::cerr<<"send: "<<message<<"\n";
               auto ec = _ws_connection->send( message );
               FC_ASSERT( !ec, "websocket send failed: ${msg}", ("msg",ec.message() ) );
            }
            virtual void close( int64_t code, const std::string& reason  )override
            {
               _ws_connection->close(code,reason);
            }

            T _ws_connection;
      };

      using websocketpp::connection_hdl;

      template< typename ServerConfigType >
      class websocket_server_base
      {
      private:
         typedef std::map<connection_hdl, websocket_connection_ptr,std::owner_less<connection_hdl> > con_map;

      protected:
         typedef websocketpp::server< ServerConfigType > websocket_server_type;

         websocket_server_base( fc::thread& server_thread )
            : _server_thread( server_thread )
         {
            _server.clear_access_channels( websocketpp::log::alevel::all );
            _server.init_asio(&fc::asio::default_io_service());
            _server.set_reuse_addr(true);
            _server.set_open_handler( [&]( connection_hdl hdl ){
               _server_thread.async( [&](){
                  auto conn = _server.get_con_from_hdl(hdl);
                  boost::asio::ip::tcp::no_delay option(true);
                  conn->get_raw_socket().set_option(option);

                  auto new_con = std::make_shared<websocket_connection_impl<typename websocket_server_type::connection_ptr>>( conn );
                  _on_connection( _connections[hdl] = new_con );
               }).wait();
            });
            _server.set_message_handler( [&]( connection_hdl hdl, typename websocket_server_type::message_ptr msg ){
               auto response_future = _server_thread.async( [&](){
                  auto current_con = _connections.find(hdl);
                  assert( current_con != _connections.end() );
                  wdump(("server")(msg->get_payload()));
                  //std::cerr<<"recv: "<<msg->get_payload()<<"\n";
                  auto payload = msg->get_payload();
                  std::shared_ptr<websocket_connection> con = current_con->second;
                  ++_pending_messages;
                  auto f = fc::async([this,con,payload](){ if( _pending_messages ) --_pending_messages; con->on_message( payload ); });
                  if( _pending_messages > 100 )
                     f.wait();
               });

               emplace_request(fc::async([&](){ _on_message( response_future ); }, "websocket_server handle_connection"));
               response_future.wait();
            });

            _server.set_http_handler( [&]( connection_hdl hdl ){
               auto response_future = _server_thread.async( [&](){
                  auto current_con = std::make_shared<websocket_connection_impl<typename websocket_server_type::connection_ptr>>( _server.get_con_from_hdl(hdl) );
                  _on_connection( current_con );

                  auto con = _server.get_con_from_hdl(hdl);
                  con->defer_http_response();
                  std::string request_body = con->get_request_body();
                  wdump(("server")(request_body));

                  fc::async([current_con, request_body, con] {
                     std::string response = current_con->on_http(request_body);
                     con->set_body( response );
                     con->set_status( websocketpp::http::status_code::ok );
                     con->send_http_response();
                     current_con->closed();
                  }, "call on_http");
               });

               emplace_request(fc::async([&](){ _on_message( response_future ); }, "websocket_server handle_connection"));
               response_future.wait();
            });

            _server.set_close_handler( [&]( connection_hdl hdl ){
               _server_thread.async( [&](){
                  if( _connections.find(hdl) != _connections.end() )
                  {
                     _connections[hdl]->closed();
                     _connections.erase( hdl );
                  }
                  else
                  {
                        wlog( "unknown connection closed" );
                  }
                  if( _connections.empty() && _closed )
                     _closed->set_value();
               }).wait();
            });

            _server.set_fail_handler( [&]( connection_hdl hdl ){
               if( _server.is_listening() )
               {
                  _server_thread.async( [&](){
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
                  }).wait();
               }
            });
         }

         ~websocket_server_base()
         {
            if( _server.is_listening() )
               _server.stop_listening();

            for (fc::future<void>& request_in_progress : requests_in_progress)
            {
               try
               {
                  request_in_progress.cancel_and_wait();
               }
               catch (const fc::exception& e)
               {
                  wlog("Caught exception while canceling websocket request task: ${error}", ("error", e));
               }
               catch (const std::exception& e)
               {
                  wlog("Caught exception while canceling websocket request task: ${error}", ("error", e.what()));
               }
               catch (...)
               {
                  wlog("Caught unknown exception while canceling websocket request task");
               }
            }
            requests_in_progress.clear();

            if( _connections.size() )
               _closed = fc::promise<void>::create("");

            auto cpy_con = _connections;
            for( auto item : cpy_con )
               _server.close( item.first, 0, "server exit" );

            if( _closed ) _closed->wait();
         }

         void emplace_request( const fc::future<void>& req )
         {
            // clean up futures for any completed requests
            for (auto iter = requests_in_progress.begin(); iter != requests_in_progress.end();)
               if (!iter->valid() || iter->ready())
                  iter = requests_in_progress.erase(iter);
               else
                  ++iter;

            requests_in_progress.emplace_back( req );
         }

         fc::thread&           _server_thread;

         con_map               _connections;
         uint32_t              _pending_messages = 0;

         std::vector<fc::future<void>> requests_in_progress;

      public:
         websocket_server_type       _server;

         on_connection_handler       _on_connection;
         on_message_handler          _on_message;
         fc::promise<void>::ptr      _closed;
      };

      typedef websocketpp::lib::shared_ptr<boost::asio::ssl::context> context_ptr;

      class websocket_server_impl : public websocket_server_base< asio_with_stub_log >
      {
         public:
            websocket_server_impl()
               : websocket_server_base( fc::thread::current() )
            {
            }

            ~websocket_server_impl() = default;
      };

      class websocket_tls_server_impl : public websocket_server_base< asio_tls_stub_log >
      {
         public:
            websocket_tls_server_impl( const string& server_pem, const string& ssl_password )
               : websocket_server_base( fc::thread::current() )
            {
               //if( server_pem.size() )
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
                     } FC_CAPTURE_AND_LOG(())
                     return ctx;
                  });
               }
            }

            ~websocket_tls_server_impl() = default;
      };


      typedef websocketpp::client<asio_with_stub_log> websocket_client_type;
      typedef websocketpp::client<asio_tls_stub_log> websocket_tls_client_type;

      typedef websocket_client_type::connection_ptr  websocket_client_connection_type;
      typedef websocket_tls_client_type::connection_ptr  websocket_tls_client_connection_type;

      class websocket_client_impl
      {
         public:
            typedef websocket_client_type::message_ptr message_ptr;

            websocket_client_impl()
            :_client_thread( fc::thread::current() )
            {
                _client.clear_access_channels( websocketpp::log::alevel::all );
                _client.set_message_handler( [&]( connection_hdl hdl, message_ptr msg ){
                   _client_thread.async( [&](){
                        wdump((msg->get_payload()));
                        //std::cerr<<"recv: "<<msg->get_payload()<<"\n";
                        auto received = msg->get_payload();
                        fc::async( [=](){
                           if( _connection )
                               _connection->on_message(received);
                        });
                   }).wait();
                });
                _client.set_close_handler( [=]( connection_hdl hdl ){
                   _client_thread.async( [&](){ if( _connection ) {_connection->closed(); _connection.reset();} } ).wait();
                   if( _closed ) _closed->set_value();
                });
                _client.set_fail_handler( [=]( connection_hdl hdl ){
                   auto con = _client.get_con_from_hdl(hdl);
                   auto message = con->get_ec().message();
                   if( _connection )
                      _client_thread.async( [&](){ if( _connection ) _connection->closed(); _connection.reset(); } ).wait();
                   if( _connected && !_connected->ready() )
                       _connected->set_exception( exception_ptr( new FC_EXCEPTION( exception, "${message}", ("message",message)) ) );
                   if( _closed )
                       _closed->set_value();
                });

                _client.init_asio( &fc::asio::default_io_service() );
            }
            ~websocket_client_impl()
            {
              /*
                Basic information:
                  The rule is that if you do let an exception propagate out of a destructor,
                  and that destructor was for an automatic object that was being directly destroyed by stack unwinding, then std::terminate would be called

                More information:
                  In C++11, a destructor will be implicitly declared noexcept, and therefore,
                  allowing an exception to propagate out of it will call std::terminate unconditionally.

                Solution:
                  The exception has to be caught in a destructor.
              */
              if(_connection )
              {
                try
                {
                  _connection->close(0, "client closed");
                  _connection.reset();
                  _closed->wait();
                }
                catch(const std::exception& e)
                {
                  wlog( "closing connection failed: ${ex}", ("ex", e.what()) );
                }
                catch(...)
                {
                  wlog( "closing connection failed" );
                }
              }
            }
            fc::promise<void>::ptr             _connected;
            fc::promise<void>::ptr             _closed;
            fc::thread&                        _client_thread;
            websocket_client_type              _client;
            websocket_connection_ptr           _connection;
            std::string                        _uri;
      };



      class websocket_tls_client_impl
      {
         public:
            typedef websocket_tls_client_type::message_ptr message_ptr;

            websocket_tls_client_impl( const std::string& ca_filename )
            :_client_thread( fc::thread::current() )
            {
                // ca_filename has special values:
                // "_none" disables cert checking (potentially insecure!)
                // "_default" uses default CA's provided by OS

                _client.clear_access_channels( websocketpp::log::alevel::all );
                _client.set_message_handler( [&]( connection_hdl hdl, message_ptr msg ){
                   _client_thread.async( [&](){
                        wdump((msg->get_payload()));
                      _connection->on_message( msg->get_payload() );
                   }).wait();
                });
                _client.set_close_handler( [=]( connection_hdl hdl ){
                   if( _connection )
                   {
                      try {
                         _client_thread.async( [&](){
                                 wlog(". ${p}", ("p",uint64_t(_connection.get())));
                                 if( !_shutting_down && !_closed && _connection )
                                    _connection->closed();
                                 _connection.reset();
                         } ).wait();
                      } catch ( const fc::exception& e )
                      {
                          if( _closed ) _closed->set_exception( e.dynamic_copy_exception() );
                      }
                      if( _closed ) _closed->set_value();
                   }
                });
                _client.set_fail_handler( [=]( connection_hdl hdl ){
                   elog( "." );
                   auto con = _client.get_con_from_hdl(hdl);
                   auto message = con->get_ec().message();
                   if( _connection )
                      _client_thread.async( [&](){ if( _connection ) _connection->closed(); _connection.reset(); } ).wait();
                   if( _connected && !_connected->ready() )
                       _connected->set_exception( exception_ptr( new FC_EXCEPTION( exception, "${message}", ("message",message)) ) );
                   if( _closed )
                       _closed->set_value();
                });

                //
                // We need ca_filename to be copied into the closure, as the referenced object might be destroyed by the caller by the time
                // tls_init_handler() is called.  According to [1], capture-by-value results in the desired behavior (i.e. creation of
                // a copy which is stored in the closure) on standards compliant compilers, but some compilers on some optimization levels
                // are buggy and are not standards compliant in this situation.  Also, keep in mind this is the opinion of a single forum
                // poster and might be wrong.
                //
                // To be safe, the following line explicitly creates a non-reference string which is captured by value, which should have the
                // correct behavior on all compilers.
                //
                // [1] http://www.cplusplus.com/forum/general/142165/
                // [2] http://stackoverflow.com/questions/21443023/capturing-a-reference-by-reference-in-a-c11-lambda
                //

                std::string ca_filename_copy = ca_filename;

                _client.set_tls_init_handler( [=](websocketpp::connection_hdl) {
                   context_ptr ctx = websocketpp::lib::make_shared<boost::asio::ssl::context>(boost::asio::ssl::context::tlsv1);
                   try {
                      ctx->set_options(boost::asio::ssl::context::default_workarounds |
                      boost::asio::ssl::context::no_sslv2 |
                      boost::asio::ssl::context::no_sslv3 |
                      boost::asio::ssl::context::single_dh_use);

                      setup_peer_verify( ctx, ca_filename_copy );
                   } catch (std::exception& e) {
                      edump((e.what()));
                      std::cout << e.what() << std::endl;
                   }
                   return ctx;
                });

                _client.init_asio( &fc::asio::default_io_service() );
            }
            ~websocket_tls_client_impl()
            {
               if(_connection )
               {
                  wlog(".");
                  _shutting_down = true;
                  _connection->close(0, "client closed");
                  _closed->wait();
               }
            }

            std::string get_host()const
            {
               return websocketpp::uri( _uri ).get_host();
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

            bool                               _shutting_down = false;
            fc::promise<void>::ptr             _connected;
            fc::promise<void>::ptr             _closed;
            fc::thread&                        _client_thread;
            websocket_tls_client_type          _client;
            websocket_connection_ptr           _connection;
            std::string                        _uri;
      };


   } // namespace detail

   websocket_server::websocket_server():my( new detail::websocket_server_impl() ) {}
   websocket_server::~websocket_server(){}

   void websocket_server::on_connection( const on_connection_handler& handler )
   {
      my->_on_connection = handler;
   }

   void websocket_server::on_message( const on_message_handler& handler )
   {
      my->_on_message = handler;
   }

   void websocket_server::listen( uint16_t port )
   {
      my->_server.listen(port);
   }
   void websocket_server::listen( const fc::ip::endpoint& ep )
   {
      my->_server.listen( boost::asio::ip::tcp::endpoint( boost::asio::ip::address_v4(uint32_t(ep.get_address())),ep.port()) );
   }

   void websocket_server::start_accept() {
      my->_server.start_accept();
   }




   websocket_tls_server::websocket_tls_server( const string& server_pem, const string& ssl_password ):my( new detail::websocket_tls_server_impl(server_pem, ssl_password) ) {}
   websocket_tls_server::~websocket_tls_server(){}

   void websocket_tls_server::on_connection( const on_connection_handler& handler )
   {
      my->_on_connection = handler;
   }

   void websocket_tls_server::on_message( const on_message_handler& handler )
   {
      my->_on_message = handler;
   }

   void websocket_tls_server::listen( uint16_t port )
   {
      my->_server.listen(port);
   }
   void websocket_tls_server::listen( const fc::ip::endpoint& ep )
   {
      my->_server.listen( boost::asio::ip::tcp::endpoint( boost::asio::ip::address_v4(uint32_t(ep.get_address())),ep.port()) );
   }

   void websocket_tls_server::start_accept() {
      my->_server.start_accept();
   }


   websocket_tls_client::websocket_tls_client( const std::string& ca_filename ):my( new detail::websocket_tls_client_impl( ca_filename ) ) {}
   websocket_tls_client::~websocket_tls_client(){ }



   websocket_client::websocket_client( const std::string& ca_filename ):my( new detail::websocket_client_impl() ),smy(new detail::websocket_tls_client_impl( ca_filename )) {}
   websocket_client::~websocket_client(){ }

   websocket_connection_ptr websocket_client::connect( const std::string& uri )
   { try {
       if( uri.substr(0,4) == "wss:" )
          return secure_connect(uri);
       FC_ASSERT( uri.substr(0,3) == "ws:" );

       // wlog( "connecting to ${uri}", ("uri",uri));
       websocketpp::lib::error_code ec;

       my->_uri = uri;
       my->_connected = fc::promise<void>::create("websocket::connect");

       my->_client.set_open_handler( [=]( websocketpp::connection_hdl hdl ){
          auto con =  my->_client.get_con_from_hdl(hdl);
          my->_connection = std::make_shared<detail::websocket_connection_impl<detail::websocket_client_connection_type>>( con );
          my->_closed = fc::promise<void>::create("websocket::closed");
          my->_connected->set_value();
       });

       auto con = my->_client.get_connection( uri, ec );

       if( ec ) FC_ASSERT( !ec, "error: ${e}", ("e",ec.message()) );

       my->_client.connect(con);
       my->_connected->wait();
       return my->_connection;
   } FC_CAPTURE_AND_RETHROW( (uri) ) }

   websocket_connection_ptr websocket_client::secure_connect( const std::string& uri )
   { try {
       if( uri.substr(0,3) == "ws:" )
          return connect(uri);
       FC_ASSERT( uri.substr(0,4) == "wss:" );
       // wlog( "connecting to ${uri}", ("uri",uri));
       websocketpp::lib::error_code ec;

       smy->_uri = uri;
       smy->_connected = fc::promise<void>::create("websocket::connect");

       smy->_client.set_open_handler( [=]( websocketpp::connection_hdl hdl ){
          auto con =  smy->_client.get_con_from_hdl(hdl);
          smy->_connection = std::make_shared<detail::websocket_connection_impl<detail::websocket_tls_client_connection_type>>( con );
          smy->_closed = fc::promise<void>::create("websocket::closed");
          smy->_connected->set_value();
       });

       auto con = smy->_client.get_connection( uri, ec );
       if( ec )
          FC_ASSERT( !ec, "error: ${e}", ("e",ec.message()) );
       smy->_client.connect(con);
       smy->_connected->wait();
       return smy->_connection;
   } FC_CAPTURE_AND_RETHROW( (uri) ) }

   websocket_connection_ptr websocket_tls_client::connect( const std::string& uri )
   { try {
       // wlog( "connecting to ${uri}", ("uri",uri));
       websocketpp::lib::error_code ec;

       my->_connected = fc::promise<void>::create("websocket::connect");

       my->_client.set_open_handler( [=]( websocketpp::connection_hdl hdl ){
          auto con =  my->_client.get_con_from_hdl(hdl);
          my->_connection = std::make_shared<detail::websocket_connection_impl<detail::websocket_tls_client_connection_type>>( con );
          my->_closed = fc::promise<void>::create("websocket::closed");
          my->_connected->set_value();
       });

       auto con = my->_client.get_connection( uri, ec );
       if( ec )
       {
          FC_ASSERT( !ec, "error: ${e}", ("e",ec.message()) );
       }
       my->_client.connect(con);
       my->_connected->wait();
       return my->_connection;
   } FC_CAPTURE_AND_RETHROW( (uri) ) }

} } // fc::http
