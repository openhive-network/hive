#pragma once
#include <functional>
#include <memory>
#include <string>
#include <fc/any.hpp>
#include <fc/network/ip.hpp>
#include <fc/signals.hpp>
#include <fc/network/http/connection.hpp>

namespace fc { namespace http {
   namespace detail {
      class websocket_server_impl;
      class websocket_tls_server_impl;
      class websocket_client_impl;
      class websocket_tls_client_impl;
   } // namespace detail;

   class websocket_connection : public connection
   {
   public:
      websocket_connection() = default;
      virtual ~websocket_connection() = default;

      virtual void send_message( const std::string& message ) = 0;
      virtual void close( int64_t code, const std::string& reason ) = 0;

      void on_http_handler( const std::function<std::string(const std::string&)>& h ) { _on_http = h; }

      string on_http( const std::string& message ) { return _on_http(message); }

   private:
      std::function<string(const std::string&)> _on_http;
   };

   typedef std::shared_ptr< websocket_connection > websocket_connection_ptr;

   class websocket_server : public server
   {
      public:
         websocket_server();
         virtual ~websocket_server();

         virtual void on_connection( const on_connection_handler& handler) override;
         virtual void listen( uint16_t port ) override;
         virtual void listen( const fc::ip::endpoint& ep ) override;
         virtual void start_accept() override;

      private:
         std::unique_ptr<detail::websocket_server_impl> my;
   };


   class websocket_tls_server : public server
   {
      public:
         websocket_tls_server( const std::string& server_pem = std::string(),
                           const std::string& ssl_password = std::string());
         virtual ~websocket_tls_server();

         virtual void on_connection( const on_connection_handler& handler) override;
         virtual void listen( uint16_t port ) override;
         virtual void listen( const fc::ip::endpoint& ep ) override;
         virtual void start_accept() override;

      private:
         std::unique_ptr<detail::websocket_tls_server_impl> my;
   };

   class websocket_client : public client
   {
      public:
         websocket_client();
         virtual ~websocket_client();

         virtual connection_ptr connect( const std::string& uri ) override;
      private:
         std::unique_ptr<detail::websocket_client_impl> my;
   };
   class websocket_tls_client : public client
   {
      public:
         websocket_tls_client( const std::string& ca_filename = "_default" );
         virtual ~websocket_tls_client();

         virtual connection_ptr connect( const std::string& uri ) override;
      private:
         std::unique_ptr<detail::websocket_tls_client_impl> my;
   };

} }
