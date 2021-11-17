
#include <fc/rpc/http_api.hpp>
#include <fc/network/http/connection.hpp>
#include <fc/io/json.hpp>
#include <fc/reflect/variant.hpp>
#include <fc/rpc/state.hpp>

namespace fc { namespace rpc {

http_api_connection::~http_api_connection()
{
}

http_api_connection::http_api_connection( fc::http::http_connection& c )
   : _connection(c)
{
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

   _connection.on_http_handler( [&]( const std::string& msg ){ return on_message(msg); } );
   _connection.closed.connect( [this](){ closed(); } );
}

variant http_api_connection::send_call(
   api_id_type api_id,
   string method_name,
   variants args /* = variants() */ )
{
   // HTTP has no way to do this, so do nothing
   return variant();
}

variant http_api_connection::send_call(
   string api_name,
   string method_name,
   variants args /* = variants() */ )
{
   // HTTP has no way to do this, so do nothing
   return variant();
}

variant http_api_connection::send_callback(
   uint64_t callback_id,
   variants args /* = variants() */ )
{
   // HTTP has no way to do this, so do nothing
   return variant();
}

void http_api_connection::send_notice(
   uint64_t callback_id,
   variants args /* = variants() */ )
{
   // HTTP has no way to do this, so do nothing
   return;
}

std::string http_api_connection::on_message( const std::string& message )
{
   wdump((message));
   try
   {
      auto var = fc::json::from_string(message);
      const auto& var_obj = var.get_object();
      if( var_obj.contains( "method" ) )
      {
         auto call = var.as<fc::rpc::request>();
         exception_ptr optexcept;
         try
         {
            try
            {
#ifdef LOG_LONG_API
               auto start = time_point::now();
#endif

               auto result = _rpc_state.local_call( call.method, call.params );

#ifdef LOG_LONG_API
               auto end = time_point::now();

               if( end - start > fc::milliseconds( LOG_LONG_API_MAX_MS ) )
                  elog( "API call execution time limit exceeded. method: ${m} params: ${p} time: ${t}", ("m",call.method)("p",call.params)("t", end - start) );
               else if( end - start > fc::milliseconds( LOG_LONG_API_WARN_MS ) )
                  wlog( "API call execution time nearing limit. method: ${m} params: ${p} time: ${t}", ("m",call.method)("p",call.params)("t", end - start) );
#endif

               if( call.id )
               {
                  auto reply = fc::json::to_string( fc::rpc::response( *call.id, result ) );
                  return reply;
               }
            }
            FC_CAPTURE_AND_RETHROW( (call.method)(call.params) )
         }
         catch ( const fc::exception& e )
         {
            if( call.id )
            {
               optexcept = e.dynamic_copy_exception();
            }
         }
         if( optexcept ) {

               auto reply = fc::json::to_string( fc::rpc::response( *call.id, fc::rpc::error_object{ 1, optexcept->to_detail_string(), fc::variant(*optexcept)}  ) );

               return reply;
         }
      }
      else
      {
         auto reply = var.as<fc::rpc::response>();
         _rpc_state.handle_reply( reply );
      }
   }
   catch ( const fc::exception& e )
   {
      wdump((e.to_detail_string()));
      return e.to_detail_string();
   }
   return string();
}

} } // namespace fc::rpc
