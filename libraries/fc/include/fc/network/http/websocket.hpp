#pragma once
#include <functional>
#include <memory>
#include <string>
#include <fc/any.hpp>
#include <fc/network/ip.hpp>
#include <fc/signals.hpp>

namespace fc { namespace http {
   namespace detail {
      class websocket_server_impl;
      class websocket_tls_server_impl;
      class websocket_client_impl;
      class websocket_tls_client_impl;
   } // namespace detail;

   class websocket_server
   {
      public:
         websocket_server();
         ~websocket_server();

         void on_connection( const on_connection_handler& handler);
         void listen( uint16_t port );
         void listen( const fc::ip::endpoint& ep );
         void start_accept();

      private:
         friend class detail::websocket_server_impl;
         std::unique_ptr<detail::websocket_server_impl> my;
   };


   class websocket_tls_server
   {
      public:
         websocket_tls_server( const std::string& server_pem = std::string(),
                           const std::string& ssl_password = std::string());
         ~websocket_tls_server();

         void on_connection( const on_connection_handler& handler);
         void listen( uint16_t port );
         void listen( const fc::ip::endpoint& ep );
         void start_accept();

      private:
         friend class detail::websocket_tls_server_impl;
         std::unique_ptr<detail::websocket_tls_server_impl> my;
   };

   class websocket_client
   {
      public:
         websocket_client( const std::string& ca_filename = "_default" );
         ~websocket_client();

         websocket_connection_ptr connect( const std::string& uri );
         websocket_connection_ptr secure_connect( const std::string& uri );
      private:
         std::unique_ptr<detail::websocket_client_impl> my;
         std::unique_ptr<detail::websocket_tls_client_impl> smy;
   };
   class websocket_tls_client
   {
      public:
         websocket_tls_client( const std::string& ca_filename = "_default" );
         ~websocket_tls_client();

         websocket_connection_ptr connect( const std::string& uri );
      private:
         std::unique_ptr<detail::websocket_tls_client_impl> my;
   };

} }
