
#include <fc/rpc/http_base_api.hpp>

namespace fc { namespace rpc {

[[ noreturn ]] variant http_base_api_connection::send_call(
   api_id_type api_id,
   string method_name,
   variants args )
   {
      FC_ASSERT( false, "Using unimplemented send_call method from the http base api" );
   }

[[ noreturn ]] variant http_base_api_connection::send_call(
   string api_name,
   string method_name,
   variants args )
   {
      FC_ASSERT( false, "Using unimplemented send_call method from the http base api" );
   }

[[ noreturn ]] variant http_base_api_connection::send_callback(
   uint64_t callback_id,
   variants args )
   {
      FC_ASSERT( false, "Using unimplemented send_call method from the http base api" );
   }

[[ noreturn ]] void http_base_api_connection::send_notice(
   uint64_t callback_id,
   variants args )
   {
      FC_ASSERT( false, "Using unimplemented send_call method from the http base api" );
   }

void http_base_api_connection::on_request( const fc::http::request& req, const fc::http::server::response& resp )
{
   // this must be called by outside HTTP server's on_request method
   std::string resp_body;
   http::reply::status_code resp_status;

   try
   {
      resp.add_header( "Content-Type", "application/json" );
      std::string req_body( req.body.begin(), req.body.end() );
      auto var = fc::json::from_string( req_body );
      const auto& var_obj = var.get_object();

      if( var_obj.contains( "method" ) )
      {
         auto call = var.as<fc::rpc::request>();
         try
         {
            try
            {
               auto result = _rpc_state.local_call( call.method, call.params );
               resp_body = fc::json::to_string( fc::rpc::response( *call.id, result ) );
               resp_status = http::reply::OK;
            }
            FC_CAPTURE_AND_RETHROW( (call.method)(call.params) );
         }
         catch ( const fc::exception& e )
         {
            resp_body = fc::json::to_string( fc::rpc::response( *call.id, error_object{ 1, e.to_detail_string(), fc::variant(e)} ) );
            resp_status = http::reply::InternalServerError;
         }
      }
      else
      {
         resp_status = http::reply::BadRequest;
         resp_body = "";
      }
   }
   catch ( const fc::exception& e )
   {
      resp_status = http::reply::InternalServerError;
      resp_body = "";
      wdump((e.to_detail_string()));
   }
   try
   {
      resp.set_status( resp_status );
      resp.set_length( resp_body.length() );
      resp.write( resp_body.c_str(), resp_body.length() );
   }
   catch( const fc::exception& e )
   {
      wdump((e.to_detail_string()));
   }
   return;
}

} } // namespace fc::rpc
