#pragma once
#include <fc/io/json.hpp>
#include <fc/network/http/connection.hpp>
#include <fc/network/http/server.hpp>
#include <fc/reflect/variant.hpp>
#include <fc/rpc/api_connection.hpp>
#include <fc/rpc/state.hpp>
#include <fc/network/url.hpp>
#include <fc/network/ip.hpp>
#include <fc/network/resolve.hpp>

#include <memory>

namespace fc { namespace rpc {

   class http_api_connection : public api_connection
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

         void on_request(
            const fc::http::request& req,
            const fc::http::server::response& resp );

      private:
         bool is_ssl;

         fc::http::connection _http_connection;
         fc::http::ssl_connection _ssl_connection;

      protected:
         fc::variant do_request( const fc::rpc::request& request );

         fc::http::connection_base& get_connection();

         fc::url            _url;
         fc::rpc::state     _rpc_state;
   };

} } // namespace fc::rpc
