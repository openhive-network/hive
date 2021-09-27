#include <fc/network/http/server.hpp>
#include <fc/thread/thread.hpp>
#include <fc/network/tcp_socket.hpp>
#include <fc/io/sstream.hpp>
#include <fc/network/ip.hpp>
#include <fc/io/stdio.hpp>
#include <fc/log/logger.hpp>


namespace fc { namespace http {

  class response::impl : public fc::retainable
  {
    public:
      impl( const fc::http::connection_ptr& c, const std::function<void()>& cont = std::function<void()>() )
      :body_bytes_sent(0),body_length(0),con(c),handle_next_req(cont)
      {}

      void send_header() {
         //ilog( "sending header..." );
         fc::stringstream ss;
         ss << "HTTP/1.1 " << rep.status << " ";
         switch( rep.status ) {
            case fc::http::reply::OK: ss << "OK\r\n"; break;
            case fc::http::reply::RecordCreated: ss << "Record Created\r\n"; break;
            case fc::http::reply::NotFound: ss << "Not Found\r\n"; break;
            case fc::http::reply::Found: ss << "Found\r\n"; break;
            default: ss << "Internal Server Error\r\n"; break;
         }
         for( uint32_t i = 0; i < rep.headers.size(); ++i ) {
            ss << rep.headers[i].key <<": "<<rep.headers[i].val <<"\r\n";
         }
         ss << "Content-Length: "<<body_length<<"\r\n\r\n";
         auto s = ss.str();
         //fc::cerr<<s<<"\n";
         con->get_socket().write( s.c_str(), s.size() );
      }

      http::reply           rep;
      int64_t               body_bytes_sent;
      uint64_t              body_length;
      http::connection_ptr      con;
      std::function<void()> handle_next_req;
  };

  response::response(){}
  response::response( const response& s ):my(s.my){}
  response::response( response&& s ):my(fc::move(s.my)){}
  response::response( const fc::shared_ptr<response::impl>& m ):my(m){}

  response& response::operator=(const response& s) { my = s.my; return *this; }
  response& response::operator=(response&& s)      { fc_swap(my,s.my); return *this; }

  void response::add_header( const fc::string& key, const fc::string& val )const {
     my->rep.headers.push_back( fc::http::header( key, val ) );
  }
  void response::set_status( const http::reply::status_code& s )const {
     if( my->body_bytes_sent != 0 ) {
       wlog( "Attempt to set status after sending headers" );
     }
     my->rep.status = s;
  }
  void response::set_length( uint64_t s )const {
    if( my->body_bytes_sent != 0 ) {
      wlog( "Attempt to set length after sending headers" );
    }
    my->body_length = s; 
  }
  void response::write( const char* data, uint64_t len )const {
    if( my->body_bytes_sent + len > my->body_length ) {
      wlog( "Attempt to send to many bytes.." );
      len = my->body_bytes_sent + len - my->body_length;
    }
    if( my->body_bytes_sent == 0 ) {
      my->send_header();
    }
    my->body_bytes_sent += len;
    my->con->get_socket().write( data, static_cast<size_t>(len) ); 
    if( my->body_bytes_sent == int64_t(my->body_length) ) {
      if( false || my->handle_next_req ) {
        ilog( "handle next request..." );
        //fc::async( std::function<void()>(my->handle_next_req) );
        fc::async( my->handle_next_req, "http_server handle_next_req" );
      }
    }
  }


} }
