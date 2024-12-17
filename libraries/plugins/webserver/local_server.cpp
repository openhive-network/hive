
#include <hive/plugins/webserver/local_server.hpp>

#include <fc/thread/thread.hpp>

namespace hive { namespace plugins { namespace webserver { namespace detail {

local_server::~local_server()
{
  if( thread )
  {
    thread->join();
    thread.reset();
  }
}

void local_server::handle_http_request( plugins::json_rpc::json_rpc_plugin* api, asio::io_service* thread_pool_ios, connection_hdl hdl ) {
  auto con = server.get_con_from_hdl( std::move( hdl ) );
  con->defer_http_response();

  thread_pool_ios->post( [con, api]()
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

void local_server::start( asio::io_service& http_ios, plugins::json_rpc::json_rpc_plugin* api, asio::io_service* thread_pool_ios )
{
  if( endpoint ) {
    thread = std::make_shared<std::thread>( [&]() {
      ilog( "start processing unix http thread" );
      fc::set_thread_name("unix_http");
      fc::thread::current().set_name("unix_http");
      try {
        server.clear_access_channels( websocketpp::log::alevel::all );
        server.clear_error_channels( websocketpp::log::elevel::all );
        server.init_asio( &http_ios );

        server.set_http_handler( boost::bind( &local_server::handle_http_request, this, api, thread_pool_ios, boost::placeholders::_1 ) );
        //unix_server.set_http_handler([&](connection_hdl hdl) {
        //   webserver_plugin_impl::handle_http_request<detail::asio_local_with_stub_log>( unix_server.get_con_from_hdl(hdl) );
        //});

        ilog( "start listening for unix http requests" );
        server.listen( *endpoint );
        ilog( "start accepting unix http requests" );
        server.start_accept();

        ilog( "start running unix http requests" );
        http_ios.run();
        ilog( "unix http io service exit" );
      } catch( ... ) {
        elog( "error thrown from unix http io service" );
      }
    });
  }
}

} } } } // hive::plugins::webserver::detail