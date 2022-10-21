#pragma once
#include <fc/io/json.hpp>
#include <fc/network/http/connection.hpp>
#include <fc/network/http/server.hpp>
#include <fc/reflect/variant.hpp>
#include <fc/rpc/api_connection.hpp>
#include <fc/rpc/http_base_api.hpp>
#include <fc/rpc/state.hpp>
#include <fc/network/url.hpp>
#include <fc/network/ip.hpp>
#include <fc/network/resolve.hpp>

#include <memory>

namespace fc { namespace rpc {

   class http_api_connection : public http_base_api_connection
   {
      public:
         http_api_connection( const std::string& _url );

         virtual variant send_call(
            api_id_type api_id,
            string method_name,
            variants args = variants() ) override;
         virtual variant send_call(
            string api_name,
            string method_name,
            variants args = variants() ) override;
         virtual variant send_callback(
            uint64_t callback_id,
            variants args = variants() ) override;
         virtual void send_notice(
            uint64_t callback_id,
            variants args = variants() ) override;

      private:
         bool is_ssl;

      protected:
         fc::variant do_request( const fc::rpc::request& request );

         void connect_with( fc::http::connection_base& con );

         fc::url            _url;
   };

} } // namespace fc::rpc
