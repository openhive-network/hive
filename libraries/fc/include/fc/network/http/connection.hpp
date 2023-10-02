#pragma once
#include <fc/vector.hpp>
#include <fc/string.hpp>
#include <fc/network/tcp_socket.hpp>
#include <fc/network/ip.hpp>
#include <memory>

namespace fc {
  namespace ip { class endpoint; }
  class tcp_socket;
  class tcp_ssl_socket;

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
            NoContent           = 204,
            BadRequest          = 400,
            NotAuthorized       = 401,
            NotFound            = 404,
            Found               = 302,
            InternalServerError = 500,

            NotSet              = -1
        };
        reply( status_code c = status_code::NotSet):status(c){}
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

     class connection_base
     {
      public:

         virtual void         connect_to( const fc::ip::endpoint& ep ) = 0;
         virtual http::reply  request( const fc::string& method, const fc::string& url, const fc::string& body = std::string(), const headers& = headers()) = 0;

         virtual http::request    read_request()const = 0;
     };

     /**
      *  Connections have reference semantics, all copies refer to the same
      *  underlying socket.
      */
     class connection : public connection_base
     {
      public:
         connection();
         // used for clients
         virtual void         connect_to( const fc::ip::endpoint& ep );
         virtual http::reply  request( const fc::string& method, const fc::string& url, const fc::string& body = std::string(), const headers& = headers());

         // used for servers
         fc::tcp_socket& get_socket()const;

         virtual http::request    read_request()const;

      private:
        class impl
        {
        public:
          fc::tcp_socket sock;
          fc::ip::endpoint ep;
          int read_until( char* buffer, char* end, char c = '\n' );
          fc::http::reply parse_reply();
        };

        std::unique_ptr<impl> my;
     };

     typedef std::shared_ptr<connection> connection_ptr;

     /**
      *  Connections have reference semantics, all copies refer to the same underlying socket.
      */
     class ssl_connection : public connection_base
     {
      public:
         ssl_connection();
         // used for clients
         virtual void         connect_to( const fc::ip::endpoint& ep );
         virtual http::reply  request( const fc::string& method, const fc::string& url, const fc::string& body = std::string(), const headers& = headers());

         // used for servers
         fc::tcp_ssl_socket& get_socket()const;

         virtual http::request    read_request()const;

      private:
        class impl
        {
        public:
          fc::tcp_ssl_socket sock;
          fc::ip::endpoint ep;
          int read_until( char* buffer, char* end, char c = '\n' );
          fc::http::reply parse_reply();
        };

         std::unique_ptr<impl> my;
     };

     typedef std::shared_ptr<ssl_connection> ssl_connection_ptr;

} } // fc::http

#include <fc/reflect/reflect.hpp>
FC_REFLECT( fc::http::header, (key)(val) )

