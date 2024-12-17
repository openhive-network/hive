
#include <websocketpp/server.hpp>

#include <hive/plugins/json_rpc/json_rpc_plugin.hpp>
#include <hive/plugins/json_rpc/utility.hpp>

#include <fc/time.hpp>

#include <boost/asio.hpp>

#include<memory>

namespace hive { namespace plugins { namespace webserver {

using websocketpp::connection_hdl;
namespace asio = boost::asio;

template<typename websocket_server_type>
struct webserver
{
  std::string name;

  std::shared_ptr< std::thread >  thread;
  asio::io_service                ios;
  websocket_server_type           server;

  webserver( const std::string& name ): name( name )
  {

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

  void run()
  {

  }

};

} } } // hive::plugins::webserver
