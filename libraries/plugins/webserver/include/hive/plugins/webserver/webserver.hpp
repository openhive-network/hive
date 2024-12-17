#pragma once

#include <websocketpp/server.hpp>

#include <hive/plugins/json_rpc/json_rpc_plugin.hpp>
#include <hive/plugins/json_rpc/utility.hpp>

#include <hive/plugins/webserver/tls_server.hpp>

#include <fc/time.hpp>
#include <fc/thread/thread.hpp>

#include <boost/asio.hpp>
#include <boost/algorithm/string.hpp>

#include<memory>

namespace hive { namespace plugins { namespace webserver { namespace detail {

using websocketpp::connection_hdl;
using boost::asio::ip::tcp;

namespace asio = boost::asio;

enum class server_type{ http, https, ws, wss };

template<typename websocket_server_type>
struct webserver
{
  bool the_same_endpoint = false;

  server_type type;

  std::optional< tcp::endpoint >       endpoint;

  std::optional<tls_server>       tls;

  std::shared_ptr< std::thread >  thread;
  asio::io_service                ios;
  websocket_server_type           server;


  webserver( const server_type& type ): type( type )
  {
  }

  ~webserver()
  {
    if( thread )
    {
      ios.stop();
      thread->join();
      thread.reset();
    }
  }

  std::string name() const
  {
    switch( type )
    {
      case server_type::http:   return "http";
      case server_type::https:  return "https";
      case server_type::ws:     return "ws";
      case server_type::wss:    return "wss";
      default:                  return "";
    }
  }

  void update_endpoint()
  {
    FC_ASSERT( endpoint, "endpoint is empty" );

    if( endpoint->port() == 0 )
    {
      boost::system::error_code error;
      auto server_port = server.get_local_endpoint(error).port();
      FC_ASSERT( !error, "${err}", ( "err", error.message() ) );
      endpoint->port( server_port );
    }
  }

  void set_secure_connection( appbase::application& app, const std::string& server_certificate_file_name, const std::string& server_key_file_name )
  {
    FC_ASSERT( !tls && ( type == server_type::http || type == server_type::ws ) );

    tls = detail::tls_server( app );

    tls->server_certificate_file_name = server_certificate_file_name;
    tls->server_key_file_name = server_key_file_name;

    if( type == server_type::http )
      type = server_type::https;
    else
      type = server_type::wss;
  }

  bool is_secure_connection() const
  {
    FC_ASSERT( ( tls && ( type == server_type::https || type == server_type::wss ) ) || ( !tls && ( type == server_type::http || type == server_type::ws ) ) );
    return tls.has_value();
  }

  void handle_ws_message( plugins::json_rpc::json_rpc_plugin* api, asio::io_service* thread_pool_ios, connection_hdl hdl, const typename websocket_server_type::message_ptr& msg )
  {
    auto con = server.get_con_from_hdl( std::move( hdl ) );

    fc::time_point arrival_time = fc::time_point::now();
    thread_pool_ios->post( [con, msg, api, arrival_time]()
    {
      LOG_DELAY(arrival_time, fc::seconds(2), "Excessive delay to begin processing ws API call");

      try
      {
        if( msg->get_opcode() == websocketpp::frame::opcode::text )
        {
          auto body = msg->get_payload();
          LOG_DELAY(arrival_time, fc::seconds(4), "Excessive delay to get ws payload");

          auto response =  api->call( body );
          LOG_DELAY_EX(arrival_time, fc::seconds(10), "Excessive delay to process ws API call: ${body}", (body));

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

  void handle_http_message( plugins::json_rpc::json_rpc_plugin* api, asio::io_service* thread_pool_ios, connection_hdl hdl )
  {
    auto con = server.get_con_from_hdl( std::move( hdl ) );
    con->defer_http_response();

    fc::time_point arrival_time = fc::time_point::now();
    thread_pool_ios->post( [con, api, arrival_time]()
    {
      LOG_DELAY(arrival_time, fc::seconds(2), "Excessive delay to begin processing API call");

      auto body = con->get_request_body();
      LOG_DELAY(arrival_time, fc::seconds(4), "Excessive delay to get request_body");

      try
      {
        con->set_body( api->call( body ) );
        con->append_header( "Content-Type", "application/json" );

        /*
          HTTP/1.1 applications that do not support persistent connections MUST include the "close" connection option in every message. 
          See: https://www.w3.org/Protocols/rfc2616/rfc2616-sec14.html

          Additional details: https://github.com/zaphoyd/websocketpp/issues/890
        */
        con->append_header( "Connection", "close" );

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

      LOG_DELAY_EX(arrival_time, fc::seconds(10), "Excessive delay to process API call ${body}",(body));
      con->send_http_response();
    });
  }

  void start( bool is_the_same_endpoint, plugins::json_rpc::json_rpc_plugin* api, asio::io_service* thread_pool_ios, std::function<void(const std::string&, const std::optional< tcp::endpoint >&)> notify )
  {
    if( !endpoint )
      return;

    if( is_the_same_endpoint && ( type == server_type::http || type == server_type::https ) )
      return;

    thread = std::make_shared<std::thread>( [is_the_same_endpoint, notify, api, thread_pool_ios, this]()
    {
      ilog( "start processing ${name} thread", ("name", name()) );
      fc::set_thread_name("websocket");
      fc::thread::current().set_name("websocket");
      try
      {
        server.clear_access_channels( websocketpp::log::alevel::all );
        server.clear_error_channels( websocketpp::log::elevel::all );
        server.init_asio( &ios );
        server.set_reuse_addr( true );

        if( type == server_type::http || type == server_type::https )
          server.set_http_handler( boost::bind( &webserver<websocket_server_type>::handle_http_message, this, api, thread_pool_ios, boost::placeholders::_1 ) );
        else
          server.set_message_handler( boost::bind( &webserver<websocket_server_type>::handle_ws_message, this, api, thread_pool_ios, boost::placeholders::_1, boost::placeholders::_2 ) );

        if( is_the_same_endpoint && ( type == server_type::ws || type == server_type::wss ) )
        {
          server.set_http_handler( boost::bind( &webserver<websocket_server_type>::handle_http_message, this, api, thread_pool_ios, boost::placeholders::_1 ) );
          ilog( "start listening for ${name} requests on ${endpoint}", ( "name", name() )( "endpoint", boost::lexical_cast<fc::string>( *endpoint ) ) );
        }

        if( is_secure_connection() )
          tls->set_tls_handlers( server );

        ilog( "start listening for ${name} requests on ${endpoint}", ( "name", name() )( "endpoint", boost::lexical_cast<fc::string>( *endpoint ) ) );
        server.listen( *endpoint );

        ilog( "start accepting ${name} requests", ( "name", name() ) );
        server.start_accept();

        ilog( "update ${name} endpoint if necessary", ( "name", name() ) );
        update_endpoint();

        ilog( "send a notification about ${address}:${port} endpoint", ( "address", endpoint->address().to_string() )( "port", endpoint->port() ) );
        notify( boost::to_upper_copy<std::string>( name() ), endpoint );

        ios.run();
        ilog( "${name} io service exit", ( "name", name() ) );
      }
      catch( const fc::exception& e )
      {
        elog( "error thrown from ${name} io service. Details: ${message}", ( "name", name() )( "message", e.what() ) );
      }
      catch ( const boost::exception& e )
      {
        elog( "error thrown from ${name} io service. Details: ${message}", ( "name", name() )( "message", boost::diagnostic_information( e ) ) );
      }
      catch( std::exception& e )
      {
        elog( "error thrown from ${name} io service. Details: ${message}", ( "name", name() )( "message", e.what() ) );
      }
      catch( ... )
      {
        elog( "error thrown from ${name} io service", ( "name", name() ) );
      }
    });
  }

};

} } } } // hive::plugins::webserver::detail
