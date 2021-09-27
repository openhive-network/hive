#pragma once
#include <fc/network/http/connection.hpp>
#include <fc/shared_ptr.hpp>
#include <vector>
#include <functional>
#include <memory>

namespace fc {
  namespace ip { class endpoint; }
  class tcp_socket;

  namespace http {

    struct header
    {
      header( fc::string k, fc::string v )
      :key(fc::move(k)),val(fc::move(v)){}
      header(){}
      fc::string key;
      fc::string val;
    };

    typedef std::vector<header> headers;

    struct reply
    {
      enum status_code {
          OK                  = 200,
          RecordCreated       = 201,
          BadRequest          = 400,
          NotAuthorized       = 401,
          NotFound            = 404,
          Found               = 302,
          InternalServerError = 500
      };
      reply( status_code c = OK):status(c){}
      int                     status;
      std::vector<header>      headers;
      std::vector<char>        body;
    };

    struct request
    {
      fc::string get_header( const fc::string& key )const;
      fc::string              remote_endpoint;
      fc::string              method;
      fc::string              domain;
      fc::string              path;
      std::vector<header>     headers;
      std::vector<char>       body;
    };

    std::vector<header> parse_urlencoded_params( const fc::string& f );

    class response
    {
    public:
      class impl;

      response();
      response( const fc::shared_ptr<impl>& my);
      response( const response& r);
      response( response&& r );
      ~response();
      response& operator=(const response& );
      response& operator=( response&& );

      void add_header( const fc::string& key, const fc::string& val )const;
      void set_status( const http::reply::status_code& s )const;
      void set_length( uint64_t s )const;

      void write( const char* data, uint64_t len )const;

    private:
      fc::shared_ptr<impl> my;
    };

  namespace detail {
      class http_server_impl;
      class http_tls_server_impl;
      class http_client_impl;
      class http_tls_client_impl;
   } // namespace detail;

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

#include <fc/reflect/reflect.hpp>
FC_REFLECT( fc::http::header, (key)(val) )
