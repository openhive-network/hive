#pragma once
#include <fc/vector.hpp>
#include <fc/string.hpp>
#include <fc/signals.hpp>
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
     
   class connection
   {
      public:
         connection() = default;
         virtual ~connection() = default;
         virtual void send_message( const std::string& message ) = 0;
         virtual void close( int64_t code, const std::string& reason  ){};
         void on_message( const std::string& message ) { _on_message(message); }
         string on_http( const std::string& message ) { return _on_http(message); }

         void on_message_handler( const std::function<void(const std::string&)>& h ) { _on_message = h; }
         void on_http_handler( const std::function<std::string(const std::string&)>& h ) { _on_http = h; }

         void     set_session_data( fc::any d ){ _session_data = std::move(d); }
         fc::any& get_session_data() { return _session_data; }

         fc::signal<void()> closed;

      protected:
         fc::any                                   _session_data;
         std::function<void(const std::string&)>   _on_message;
         std::function<string(const std::string&)> _on_http;
   };

   typedef std::shared_ptr< connection > connection_ptr;

   typedef std::function< void(const connection_ptr&) > on_connection_handler;

   class server
   {
      public:
         server( const std::string& server_pem = std::string{},
                           const std::string& ssl_password = std::string{});
         virtual ~server() = default;

         virtual void on_connection( const on_connection_handler& handler) = 0;
         virtual void listen( uint16_t port ) = 0;
         virtual void listen( const fc::ip::endpoint& ep ) = 0;
         virtual void start_accept() = 0;

      protected:
        std::string server_pem;
        std::string ssl_password;
   };

   class client
   {
      public:
         client( const std::string& ca_filename = std::string{} );
         virtual ~client() = default;

         virtual connection_ptr connect( const std::string& uri ) = 0;

      protected:
        std::string ca_filename;
   };

} } // fc::http

#include <fc/reflect/reflect.hpp>
FC_REFLECT( fc::http::header, (key)(val) )

