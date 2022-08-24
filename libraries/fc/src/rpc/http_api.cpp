
#include <fc/rpc/http_api.hpp>

#include <fc/network/url.hpp>

namespace fc { namespace rpc {

fc::http::connection_base& http_api_connection::get_connection()
{
   if( is_ssl )
      return _ssl_connection;
   else
      return _http_connection;
}


http_api_connection::http_api_connection( const std::string& _url )
{
   this->_url = fc::url{ _url };

   if( this->_url.proto() == "http" )
      this->is_ssl = false;
   else if( this->_url.proto() == "https" )
      this->is_ssl = true;
   else
      FC_ASSERT( false, "Invalid protocol type for the http_api: Expected http or https. Got: ${proto}", ("proto", this->_url.proto())("url", _url) );

   _rpc_state.add_method( "call", [this]( const variants& args ) -> variant
   {
      // TODO: This logic is duplicated between http_api_connection and websocket_api_connection
      // it should be consolidated into one place instead of copy-pasted
      FC_ASSERT( args.size() == 3 && args[2].is_array() );
      api_id_type api_id;
      if( args[0].is_string() )
      {
         variants subargs;
         subargs.push_back( args[0] );
         variant subresult = this->receive_call( 1, "get_api_by_name", subargs );
         api_id = subresult.as_uint64();
      }
      else
         api_id = args[0].as_uint64();

      return this->receive_call(
         api_id,
         args[1].as_string(),
         args[2].get_array() );
   } );

   _rpc_state.add_method( "notice", [this]( const variants& args ) -> variant
   {
      FC_ASSERT( args.size() == 2 && args[1].is_array() );
      this->receive_notice(
         args[0].as_uint64(),
         args[1].get_array() );
      return variant();
   } );

   _rpc_state.add_method( "callback", [this]( const variants& args ) -> variant
   {
      FC_ASSERT( args.size() == 2 && args[1].is_array() );
      this->receive_callback(
         args[0].as_uint64(),
         args[1].get_array() );
      return variant();
   } );

   _rpc_state.on_unhandled( [&]( const std::string& method_name, const variants& args )
   {
      return this->receive_call( 0, method_name, args );
   } );
}

variant http_api_connection::send_call(
   api_id_type api_id,
   string method_name,
   variants args /* = variants() */ )
{
   idump( (api_id)(method_name)(args) );
   auto request = _rpc_state.start_remote_call(  "call", {api_id, std::move(method_name), std::move(args) } );
   return do_request(request);
}

variant http_api_connection::send_call(
   string api_name,
   string method_name,
   variants args /* = variants() */ )
{
   idump( (api_name)(method_name)(args) );
   auto request = _rpc_state.start_remote_call(  "call", {std::move(api_name), std::move(method_name), std::move(args) } );
   return do_request(request);
}

variant http_api_connection::send_callback(
   uint64_t callback_id,
   variants args /* = variants() */ )
{
   idump( (callback_id)(args) );
   auto request = _rpc_state.start_remote_call( "callback", {callback_id, std::move(args) } );
   return do_request(request);
}

void http_api_connection::send_notice(
   uint64_t callback_id,
   variants args /* = variants() */ )
{
   // HTTP has no way to do this, so do nothing
   wlog("Calling notice on http api");
   return;
}

fc::variant http_api_connection::do_request(
   const fc::rpc::request& request
)
{
   idump( (request) );

   try
   {
      get_connection().connect_to( fc::resolve( *_url.host(), *_url.port() )[0] ); // First try to resolve the domain name
   }
   catch( const fc::exception& e )
   {
      try
      {
         get_connection().connect_to( fc::ip::endpoint( *_url.host(), *_url.port() ) );
      } FC_CAPTURE_AND_RETHROW( (_url) )
   }

   std::vector< char > _body = get_connection().request( "POST", _url, fc::json::to_string(request) ).body;

   const auto message = fc::json::from_string( std::string{ _body.begin(), _body.end() } );

   idump((message));

   const auto _reply = message.as<fc::rpc::response>();

   _rpc_state.handle_reply( _reply );

   if( _reply.error )
      FC_THROW("${error}", ("error", *_reply.error)("data", message));

   return *_reply.result;
}

void http_api_connection::on_request( const fc::http::request& req, const fc::http::server::response& resp )
{
   idump( (request) );

   std::vector< char > _body;

   if( is_ssl )
   {
      fc::http::ssl_connection _ssl_connection;
      if( _skip_cert_check )
         _ssl_connection.get_socket().set_verify_peer( false );

      try
      {
         if( _is_ip_url )
            _ssl_connection.connect_to( fc::ip::endpoint( *_url.host(), *_url.port() ), *_url.host() );
         else
            _ssl_connection.connect_to( fc::resolve( *_url.host(), *_url.port() )[0], *_url.host() );
      }
      FC_CAPTURE_AND_RETHROW( (_url) )
      _body = _ssl_connection.request( "POST", _url, fc::json::to_string(request) ).body;
   }
   else
   {
      fc::http::connection _http_connection;
      try
      {
         if( _is_ip_url )
            _http_connection.connect_to( fc::ip::endpoint( *_url.host(), *_url.port() ) );
         else
            _http_connection.connect_to( fc::resolve( *_url.host(), *_url.port() )[0] );
      }
      FC_CAPTURE_AND_RETHROW( (_url) )
      _body = _http_connection.request( "POST", _url, fc::json::to_string(request) ).body;
   }

   const auto message = fc::json::from_string( std::string{ _body.begin(), _body.end() } );

   idump((message));

   const auto _reply = message.as<fc::rpc::response>();

   _rpc_state.handle_reply( _reply );

   if( _reply.error )
      FC_THROW("${error}", ("error", *_reply.error)("data", message));

   return *_reply.result;
}

} } // namespace fc::rpc
