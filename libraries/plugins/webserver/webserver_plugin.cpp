#include <hive/plugins/webserver/webserver_plugin.hpp>
#include <hive/plugins/webserver/local_endpoint.hpp>

#include <hive/plugins/json_rpc/utility.hpp>

#include <hive/plugins/chain/chain_plugin.hpp>

#include <fc/network/ip.hpp>
#include <fc/log/logger_config.hpp>
#include <fc/io/json.hpp>
#include <fc/network/resolve.hpp>

#include <boost/asio.hpp>
#include <boost/bind/bind.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/optional.hpp>
#include <boost/preprocessor/stringize.hpp>
#include <boost/system/error_code.hpp>

#include <websocketpp/config/asio_client.hpp>
#include <websocketpp/config/asio.hpp>
#include <websocketpp/server.hpp>
#include <websocketpp/config/asio_client.hpp>
#include <websocketpp/extensions/permessage_deflate/enabled.hpp>
#include <websocketpp/client.hpp>
#include <websocketpp/logger/stub.hpp>
#include <websocketpp/logger/syslog.hpp>

#include <thread>
#include <memory>
#include <iostream>

using namespace boost::placeholders;

namespace hive { namespace plugins { namespace webserver {

namespace asio = boost::asio;

using std::string;
using boost::optional;
using boost::asio::ip::tcp;
using std::shared_ptr;
using websocketpp::connection_hdl;

typedef uint32_t thread_pool_size_t;

namespace detail {

  struct asio_with_stub_log : public websocketpp::config::asio
  {
    typedef asio_with_stub_log type;
    typedef asio base;

    typedef base::concurrency_type concurrency_type;

    typedef base::request_type request_type;
    typedef base::response_type response_type;

    typedef base::message_type message_type;
    typedef base::con_msg_manager_type con_msg_manager_type;
    typedef base::endpoint_msg_manager_type endpoint_msg_manager_type;

    typedef base::alog_type alog_type;
    typedef base::elog_type elog_type;
    //typedef websocketpp::log::stub elog_type;
    //typedef websocketpp::log::stub alog_type;

    typedef base::rng_type rng_type;

    struct transport_config : public base::transport_config
    {
      typedef type::concurrency_type concurrency_type;
      typedef type::alog_type alog_type;
      typedef type::elog_type elog_type;
      typedef type::request_type request_type;
      typedef type::response_type response_type;
      typedef websocketpp::transport::asio::basic_socket::endpoint
        socket_type;
    };

    typedef websocketpp::transport::asio::endpoint< transport_config >
      transport_type;

    static const long timeout_open_handshake = 0;
  };
  struct asio_with_stub_log_and_permessage_deflate : public asio_with_stub_log
  {
    struct permessage_deflate_config {};

    typedef websocketpp::extensions::permessage_deflate::enabled<permessage_deflate_config> permessage_deflate_type;
  };
  struct asio_local_with_stub_log : public websocketpp::config::asio
  {
    typedef asio_local_with_stub_log type;
    typedef asio base;

    typedef base::concurrency_type concurrency_type;

    typedef base::request_type request_type;
    typedef base::response_type response_type;

    typedef base::message_type message_type;
    typedef base::con_msg_manager_type con_msg_manager_type;
    typedef base::endpoint_msg_manager_type endpoint_msg_manager_type;

    typedef base::alog_type alog_type;
    typedef base::elog_type elog_type;

    typedef base::rng_type rng_type;

    struct transport_config : public base::transport_config {
      typedef type::concurrency_type concurrency_type;
      typedef type::alog_type alog_type;
      typedef type::elog_type elog_type;
      typedef type::request_type request_type;
      typedef type::response_type response_type;
      typedef websocketpp::transport::asio::basic_socket::local_endpoint socket_type;
    };

    typedef websocketpp::transport::asio::local_endpoint<transport_config> transport_type;

    static const long timeout_open_handshake = 0;
  };
  struct asio_local_with_stub_log_and_permessage_deflate : public asio_local_with_stub_log
  {
    struct permessage_deflate_config {};

    typedef websocketpp::extensions::permessage_deflate::enabled<permessage_deflate_config> permessage_deflate_type;
  };

using websocket_server_type = websocketpp::server< detail::asio_with_stub_log_and_permessage_deflate >;
using websocket_local_server_type = websocketpp::server<detail::asio_local_with_stub_log_and_permessage_deflate>;

class webserver_plugin_impl
{
  public:
    webserver_plugin_impl( thread_pool_size_t _thread_pool_size, plugins::chain::chain_plugin& c ) :
      thread_pool_size( _thread_pool_size ), chain( c )
    {
    }

    void prepare_threads();

    void start_webserver();
    void stop_webserver();

    void handle_ws_message( websocket_server_type*, connection_hdl, const detail::websocket_server_type::message_ptr& );
    void handle_http_message( websocket_server_type*, connection_hdl );
    void handle_http_request( websocket_local_server_type*, connection_hdl );

    thread_pool_size_t         thread_pool_size;

    shared_ptr< std::thread >  http_thread;
    asio::io_service           http_ios;
    optional< tcp::endpoint >  http_endpoint;
    websocket_server_type      http_server;

    shared_ptr< std::thread >              unix_thread;
    asio::io_service                       unix_ios;
    optional< boost::asio::local::stream_protocol::endpoint >  unix_endpoint;
    websocket_local_server_type            unix_server;

    shared_ptr< std::thread >  ws_thread;
    asio::io_service           ws_ios;
    optional< tcp::endpoint >  ws_endpoint;
    websocket_server_type      ws_server;

    boost::thread_group        thread_pool;
    asio::io_service           thread_pool_ios;
    std::unique_ptr< asio::io_service::work > thread_pool_work;

    plugins::json_rpc::json_rpc_plugin* api = nullptr;
    boost::signals2::connection         chain_sync_con;

    plugins::chain::chain_plugin& chain;

  private:
    void update_http_endpoint();
    void update_ws_endpoint();
};

void webserver_plugin_impl::prepare_threads()
{
  thread_pool_work.reset( new asio::io_service::work( this->thread_pool_ios ) );

  for( uint32_t i = 0; i < thread_pool_size; ++i )
    thread_pool.create_thread( [&]() { fc::set_thread_name("api"); thread_pool_ios.run(); } );
}

void webserver_plugin_impl::start_webserver()
{
  const bool ws_and_http_uses_same_endpoint = http_endpoint && http_endpoint == ws_endpoint && http_endpoint->port() != 0;

  if( ws_endpoint )
  {
    ws_thread = std::make_shared<std::thread>( [&, ws_and_http_uses_same_endpoint]()
    {
      ilog( "start processing ws thread" );
      fc::set_thread_name("websocket");
      fc::thread::current().set_name("websocket");
      try
      {
        ws_server.clear_access_channels( websocketpp::log::alevel::all );
        ws_server.clear_error_channels( websocketpp::log::elevel::all );
        ws_server.init_asio( &ws_ios );
        ws_server.set_reuse_addr( true );

        ws_server.set_message_handler( boost::bind( &webserver_plugin_impl::handle_ws_message, this, &ws_server, _1, _2 ) );

        if( ws_and_http_uses_same_endpoint )
        {
          ws_server.set_http_handler( boost::bind( &webserver_plugin_impl::handle_http_message, this, &ws_server, _1 ) );
        }

        ws_server.listen( *ws_endpoint );
        update_ws_endpoint();
        if( ws_and_http_uses_same_endpoint )
        {
          ilog( "start listening for http requests on ${endpoint}", ( "endpoint", boost::lexical_cast<fc::string>( *http_endpoint ) ) );
        }
        ilog( "start listening for ws requests on ${endpoint}", ( "endpoint", boost::lexical_cast<fc::string>( *ws_endpoint ) ) );

        hive::notify( "webserver listening",
      // {
          "type", "WS",
          "address", ws_endpoint->address().to_string(),
          "port", ws_endpoint->port()
      // }
      );

        ws_server.start_accept();

        ws_ios.run();
        ilog( "ws io service exit" );
      }
      catch( ... )
      {
        elog( "error thrown from http io service" );
      }
    });
  }

  if( http_endpoint && ( !ws_and_http_uses_same_endpoint || !ws_endpoint ) )
  {
    http_thread = std::make_shared<std::thread>( [&]()
    {
      ilog( "start processing http thread" );
      fc::set_thread_name("http");
      fc::thread::current().set_name("http");
      try
      {
        http_server.clear_access_channels( websocketpp::log::alevel::all );
        http_server.clear_error_channels( websocketpp::log::elevel::all );
        http_server.init_asio( &http_ios );
        http_server.set_reuse_addr( true );

        http_server.set_http_handler( boost::bind( &webserver_plugin_impl::handle_http_message, this, &http_server, _1 ) );

        http_server.listen( *http_endpoint );
        http_server.start_accept();
        update_http_endpoint();
        ilog( "start listening for http requests on ${endpoint}", ( "endpoint", boost::lexical_cast<fc::string>( *http_endpoint ) ) );

        hive::notify( "webserver listening",
        // {
            "type", "HTTP",
            "address", http_endpoint->address().to_string(),
            "port", http_endpoint->port()
        // }
        );

        http_ios.run();
        ilog( "http io service exit" );
      }
      catch( ... )
      {
        elog( "error thrown from http io service" );
      }
    });
  }

  if( unix_endpoint ) {
    unix_thread = std::make_shared<std::thread>( [&]() {
      ilog( "start processing unix http thread" );
      fc::set_thread_name("unix_http");
      fc::thread::current().set_name("unix_http");
      try {
        unix_server.clear_access_channels( websocketpp::log::alevel::all );
        unix_server.clear_error_channels( websocketpp::log::elevel::all );
        unix_server.init_asio( &http_ios );

        unix_server.set_http_handler( boost::bind( &webserver_plugin_impl::handle_http_request, this, &unix_server, _1 ) );
        //unix_server.set_http_handler([&](connection_hdl hdl) {
        //   webserver_plugin_impl::handle_http_request<detail::asio_local_with_stub_log>( unix_server.get_con_from_hdl(hdl) );
        //});

        ilog( "start listening for unix http requests" );
        unix_server.listen( *unix_endpoint );
        ilog( "start accpeting unix http requests" );
        unix_server.start_accept();

        ilog( "start running unix http requests" );
        http_ios.run();
        ilog( "unix http io service exit" );
      } catch( ... ) {
        elog( "error thrown from unix http io service" );
      }
    });
  }
}

void update_endpoint(websocket_server_type& server, optional< tcp::endpoint >& endpoint)
{
  if (endpoint->port() == 0)
  {
    boost::system::error_code error;
    auto server_port = server.get_local_endpoint(error).port();
    FC_ASSERT(!error);
    endpoint->port(server_port);
  }
}

void webserver_plugin_impl::update_http_endpoint()
{
  update_endpoint(http_server, http_endpoint);
}

void webserver_plugin_impl::update_ws_endpoint()
{
  update_endpoint(ws_server, ws_endpoint);
}

void webserver_plugin_impl::stop_webserver()
{
  if( ws_server.is_listening() )
  ws_server.stop_listening();

  if( http_server.is_listening() )
    http_server.stop_listening();

  if( unix_server.is_listening() )
    unix_server.stop_listening();

  thread_pool_ios.stop();
  thread_pool.join_all();

  if( ws_thread )
  {
    ws_ios.stop();
    ws_thread->join();
    ws_thread.reset();
  }

  if( http_thread )
  {
    http_ios.stop();
    http_thread->join();
    http_thread.reset();
  }

  if( unix_thread )
  {
    unix_ios.stop();
    unix_thread->join();
    unix_thread.reset();
  }
}

void webserver_plugin_impl::handle_ws_message( websocket_server_type* server, connection_hdl hdl, const detail::websocket_server_type::message_ptr& msg )
{
  auto con = server->get_con_from_hdl( std::move( hdl ) );

  fc::time_point arrival_time = fc::time_point::now();
  thread_pool_ios.post( [con, msg, this, arrival_time]()
  {
    LOG_DELAY(arrival_time, fc::seconds(2), "Excessive delay to begin processing ws API call");

    try
    {
      if( msg->get_opcode() == websocketpp::frame::opcode::text )
      {
        auto body = msg->get_payload();
        LOG_DELAY(arrival_time, fc::seconds(4), "Excessive delay to get ws payload");

        auto response =  api->call( body );
        LOG_DELAY(arrival_time, fc::seconds(10), "Excessive delay to process ws API call");

        con->send( response );
      }
      else
        con->send( "error: string payload expected" );
    }
    catch( fc::exception& e )
    {
      con->send( "error calling API " + e.to_string() );
      ulog("${e}",("e",e.to_string()));
    }
    catch( ... )
    {
      auto eptr = std::current_exception();

      try
      {
        if( eptr )
          std::rethrow_exception( eptr );

        con->send( "unknown error occurred" );
      }
      catch( const std::exception& e )
      {
        std::stringstream s;
        s << "unknown exception: " << e.what();
        con->send( s.str() );
        ulog("${e}", ("e", s.str()) );
      }
    }
  });
}

void webserver_plugin_impl::handle_http_message( websocket_server_type* server, connection_hdl hdl )
{
  auto con = server->get_con_from_hdl( std::move( hdl ) );
  con->defer_http_response();

  fc::time_point arrival_time = fc::time_point::now();
  thread_pool_ios.post( [con, this, arrival_time]()
  {
    LOG_DELAY(arrival_time, fc::seconds(2), "Excessive delay to begin processing API call");

    auto body = con->get_request_body();
    LOG_DELAY(arrival_time, fc::seconds(4), "Excessive delay to get request_body");

    try
    {
      con->set_body( api->call( body ) );
      con->append_header( "Content-Type", "application/json" );
      con->set_status( websocketpp::http::status_code::ok );
    }
    catch( fc::exception& e )
    {
      edump( (e) );
      ulog("${e}", (e) );
      con->set_body( "Could not call API" );
      con->set_status( websocketpp::http::status_code::not_found );
    }
    catch( ... )
    {
      auto eptr = std::current_exception();

      try
      {
        if( eptr )
          std::rethrow_exception( eptr );
        ulog("unknown error trying to process API call");
        con->set_body( "unknown error occurred" );
        con->set_status( websocketpp::http::status_code::internal_server_error );
      }
      catch( const std::exception& e )
      {
        std::stringstream s;
        s << "unknown exception: " << e.what();
        con->set_body( s.str() );
        con->set_status( websocketpp::http::status_code::internal_server_error );
        ulog("${e}", ("e", s.str()) );
      }
    }

    LOG_DELAY(arrival_time, fc::seconds(10), "Excessive delay to process API call");
    con->send_http_response();
  });
}

void webserver_plugin_impl::handle_http_request(websocket_local_server_type* server, connection_hdl hdl ) {
  auto con = server->get_con_from_hdl( std::move( hdl ) );
  con->defer_http_response();

  thread_pool_ios.post( [con, this]()
  {
    auto body = con->get_request_body();

    try
    {
      con->set_body( api->call( body ) );
      con->append_header( "Content-Type", "application/json" );
      con->set_status( websocketpp::http::status_code::ok );
    }
    catch( fc::exception& e )
    {
      edump( (e) );
      con->set_body( "Could not call API" );
      con->set_status( websocketpp::http::status_code::not_found );
    }
    catch( ... )
    {
      auto eptr = std::current_exception();

      try
      {
        if( eptr )
          std::rethrow_exception( eptr );

        con->set_body( "unknown error occurred" );
        con->set_status( websocketpp::http::status_code::internal_server_error );
      }
      catch( const std::exception& e )
      {
        std::stringstream s;
        s << "unknown exception: " << e.what();
        con->set_body( s.str() );
        con->set_status( websocketpp::http::status_code::internal_server_error );
      }
    }

    con->send_http_response();
  });
}

} // detail

webserver_plugin::webserver_plugin() {}
webserver_plugin::~webserver_plugin() {}

void webserver_plugin::set_program_options( options_description&, options_description& cfg )
{
  cfg.add_options()
    ("webserver-http-endpoint", bpo::value< string >(), "Local http endpoint for webserver requests.")
    ("webserver-unix-endpoint", bpo::value< string >(), "Local unix http endpoint for webserver requests.")
    ("webserver-ws-endpoint", bpo::value< string >(), "Local websocket endpoint for webserver requests.")
    // TODO: maybe add a flag to make this optional
    ("webserver-enable-permessage-deflate", bpo::value< string >(), "Enable the RFC-7692 permessage-deflate extension for the WebSocket server (only used if the client requests it).  This may save bandwidth at the expense of CPU")
    ("rpc-endpoint", bpo::value< string >(), "Local http and websocket endpoint for webserver requests. Deprecated in favor of webserver-http-endpoint and webserver-ws-endpoint" )
    ("webserver-thread-pool-size", bpo::value<thread_pool_size_t>()->default_value(32),
      "Number of threads used to handle queries. Default: 32.")
    ;
}

void webserver_plugin::plugin_initialize( const variables_map& options )
{
  auto thread_pool_size = options.at("webserver-thread-pool-size").as<thread_pool_size_t>();
  FC_ASSERT(thread_pool_size > 0, "webserver-thread-pool-size must be greater than 0");
  ilog("configured with ${tps} thread pool size", ("tps", thread_pool_size));
  my.reset( new detail::webserver_plugin_impl( thread_pool_size, appbase::app().get_plugin< plugins::chain::chain_plugin >() ) );

  if( options.count( "webserver-http-endpoint" ) )
  {
    auto http_endpoint = options.at( "webserver-http-endpoint" ).as< string >();
    auto endpoints = fc::resolve_string_to_ip_endpoints( http_endpoint );
    FC_ASSERT( endpoints.size(), "webserver-http-endpoint ${hostname} did not resolve", ("hostname", http_endpoint) );
    my->http_endpoint = tcp::endpoint( boost::asio::ip::address_v4::from_string( ( string )endpoints[0].get_address() ), endpoints[0].port() );
    ilog( "configured http to listen on ${ep}", ("ep", endpoints[0]) );
  }

  if( options.count( "webserver-unix-endpoint" ) )
  {
    auto unix_endpoint = options.at( "webserver-unix-endpoint" ).as< string >();
    boost::asio::local::stream_protocol::endpoint ep(unix_endpoint);
    my->unix_endpoint = ep;
    ilog( "configured http to listen on ${ep}", ("ep", unix_endpoint ));
  }

  if( options.count( "webserver-ws-endpoint" ) )
  {
    auto ws_endpoint = options.at( "webserver-ws-endpoint" ).as< string >();
    auto endpoints = fc::resolve_string_to_ip_endpoints( ws_endpoint );
    FC_ASSERT( endpoints.size(), "ws-server-endpoint ${hostname} did not resolve", ("hostname", ws_endpoint) );
    my->ws_endpoint = tcp::endpoint( boost::asio::ip::address_v4::from_string( ( string )endpoints[0].get_address() ), endpoints[0].port() );
    ilog( "configured ws to listen on ${ep}", ("ep", endpoints[0]) );
  }

  if( options.count( "rpc-endpoint" ) )
  {
    auto endpoint = options.at( "rpc-endpoint" ).as< string >();
    auto endpoints = fc::resolve_string_to_ip_endpoints( endpoint );
    FC_ASSERT( endpoints.size(), "rpc-endpoint ${hostname} did not resolve", ("hostname", endpoint) );

    auto tcp_endpoint = tcp::endpoint( boost::asio::ip::address_v4::from_string( ( string )endpoints[0].get_address() ), endpoints[0].port() );

    if( !my->http_endpoint )
    {
      my->http_endpoint = tcp_endpoint;
      ilog( "configured http to listen on ${ep}", ("ep", endpoints[0]) );
    }

    if( !my->ws_endpoint )
    {
      my->ws_endpoint = tcp_endpoint;
      ilog( "configured ws to listen on ${ep}", ("ep", endpoints[0]) );
    }
  }
}

void webserver_plugin::plugin_startup()
{
  my->api = appbase::app().find_plugin< plugins::json_rpc::json_rpc_plugin >();
  FC_ASSERT( my->api != nullptr, "Could not find API Register Plugin" );

  my->prepare_threads();

  if( my->chain.get_state() != appbase::abstract_plugin::started )
  {
    ilog( "Waiting for chain plugin to start" );
    my->chain_sync_con = my->chain.on_sync.connect( 0, [this]()
    {
      my->start_webserver();
    });
  }
  else
  {
    my->start_webserver();
  }
}

void webserver_plugin::plugin_shutdown()
{
  my->stop_webserver();
}

} } } // hive::plugins::webserver
