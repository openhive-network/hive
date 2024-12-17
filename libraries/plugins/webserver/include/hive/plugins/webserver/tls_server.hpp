#pragma once

#include <hive/plugins/webserver/webserver_types.hpp>

#include <appbase/application.hpp>

#include <boost/bind/bind.hpp>

namespace hive { namespace plugins { namespace webserver { namespace detail {

struct tls_server
{
  typedef websocketpp::lib::shared_ptr<boost::asio::ssl::context> context_ptr;

  bool initialization_stage     = true;
  appbase::application* theApp  = nullptr;

  std::string server_certificate_file_name;
  std::string server_key_file_name;

  tls_server( appbase::application& app );

  context_ptr on_tls_init( websocketpp::connection_hdl hdl );

  template<typename websocket_server_type>
  void on_fail( websocket_server_type& server, websocketpp::connection_hdl hdl );

  template<typename websocket_server_type>
  void set_tls_handlers( websocket_server_type& server );
};

tls_server::tls_server( appbase::application& app ): theApp( &app ) {}

tls_server::context_ptr tls_server::on_tls_init( websocketpp::connection_hdl hdl )
{
  dlog("TLS initialization started");
  context_ptr ctx(new boost::asio::ssl::context(boost::asio::ssl::context::sslv23));

  bool status = true;

  try
  {
    ctx->set_options(boost::asio::ssl::context::default_workarounds |
                      boost::asio::ssl::context::no_sslv2 |
                      boost::asio::ssl::context::no_sslv3 );

    ctx->use_certificate_file( server_certificate_file_name, boost::asio::ssl::context::pem );
    ctx->use_private_key_file( server_key_file_name, boost::asio::ssl::context::pem );
  }
  catch ( const boost::exception& e )
  {
    status = false;
    elog( boost::diagnostic_information(e) );
  }
  catch(std::exception& e )
  {
    status = false;
    elog( e.what() );
  }
  catch(...)
  {
    status = false;
    elog( "Unexpected error. TLS initialization failed" );
  }

  dlog("TLS initialization ${status}", ("status", status ? "passed" : "failed") );

  if( initialization_stage && !status )
  {
    FC_ASSERT( theApp );
    theApp->kill();
  }

  initialization_stage = false;

  return ctx;
}

template<typename websocket_server_type>
void tls_server::on_fail( websocket_server_type& server, websocketpp::connection_hdl hdl )
{
  websocketpp::lib::error_code _ec;
  typename websocket_server_type::connection_ptr _con = server.get_con_from_hdl( hdl, _ec );
  if( _ec )
    ilog( "TLS connection failed: '${message}'. Error code: '${code}'", ( "message", _con->get_ec().message() )( "code", _ec.message() ) );
  else
    ilog( "TLS connection failed: '${message}'", ( "message", _con->get_ec().message() ) );
}

template<typename websocket_server_type>
void tls_server::set_tls_handlers( websocket_server_type& server )
{
  server.set_tls_init_handler( boost::bind( &tls_server::on_tls_init, this, boost::placeholders::_1 ) );
  server.set_fail_handler( boost::bind( &tls_server::on_fail<websocket_server_type>, this, std::ref( server ), boost::placeholders::_1 ) );
}

template<> void tls_server::set_tls_handlers<websocket_server_type_nondeflate>( websocket_server_type_nondeflate& server ){}
template<> void tls_server::set_tls_handlers<websocket_server_type_deflate>( websocket_server_type_deflate& server ){}

} } } } // hive::plugins::webserver::detail
