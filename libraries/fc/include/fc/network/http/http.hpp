#pragma once
#include <fc/network/http/connection.hpp>
#include <fc/shared_ptr.hpp>
#include <vector>
#include <functional>
#include <memory>

namespace fc { namespace http {

  namespace detail {
      class http_server_impl;
      class http_tls_server_impl;
      class http_client_impl;
      class http_tls_client_impl;
   } // namespace detail;

   typedef connection                         http_connection;
   typedef std::shared_ptr< http_connection > http_connection_ptr;

   class http_server : public server
   {
      public:
         http_server();
         virtual ~http_server();

         virtual void on_connection( const on_connection_handler& handler) override;
         virtual void listen( uint16_t port ) override;
         virtual void listen( const fc::ip::endpoint& ep ) override;
         virtual void start_accept() override;

      private:
         std::unique_ptr<detail::http_server_impl> my;
   };


   class http_tls_server : public server
   {
      public:
         http_tls_server( const std::string& server_pem = std::string(),
                           const std::string& ssl_password = std::string());
         virtual ~http_tls_server();

         virtual void on_connection( const on_connection_handler& handler) override;
         virtual void listen( uint16_t port ) override;
         virtual void listen( const fc::ip::endpoint& ep ) override;
         virtual void start_accept() override;

      private:
         std::unique_ptr<detail::http_tls_server_impl> my;
   };

   class http_client : public client
   {
      public:
         http_client();
         virtual ~http_client();

         virtual connection_ptr connect( const std::string& uri ) override;

      private:
         std::unique_ptr<detail::http_client_impl> my;
   };
   class http_tls_client : public client
   {
      public:
         http_tls_client( const std::string& ca_filename = "_default" );
         virtual ~http_tls_client();

         virtual connection_ptr connect( const std::string& uri ) override;

      private:
         std::unique_ptr<detail::http_tls_client_impl> my;
   };

} } // fc::http
