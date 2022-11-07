#include <fc/network/http/connection.hpp>
#include <fc/io/sstream.hpp>
#include <fc/io/iostream.hpp>
#include <fc/exception/exception.hpp>
#include <fc/crypto/hex.hpp>
#include <fc/log/logger.hpp>
#include <fc/io/stdio.hpp>
#include <fc/network/url.hpp>
#include <boost/algorithm/string.hpp>

   int fc::http::ssl_connection::impl::read_until( char* buffer, char* end, char c ) {
      char* p = buffer;
     // try {
          while( p < end && 1 == sock.readsome(p,1) ) {
            if( *p == c ) {
              *p = '\0';
              return (p - buffer)-1;
            }
            ++p;
          }
     // } catch ( ... ) {
     //   elog("%s", fc::current_exception().diagnostic_information().c_str() );
        //elog( "%s", fc::except_str().c_str() );
     // }
      return (p-buffer);
   }

   fc::http::reply fc::http::ssl_connection::impl::parse_reply() {
      fc::http::reply rep;
      try {
        std::vector<char> line(1024*8);
        int s = read_until( line.data(), line.data()+line.size(), ' ' ); // HTTP/1.1
        s = read_until( line.data(), line.data()+line.size(), ' ' ); // CODE
        rep.status = static_cast<int>(to_int64(fc::string(line.data())));
        s = read_until( line.data(), line.data()+line.size(), '\n' ); // DESCRIPTION
        
        while( (s = read_until( line.data(), line.data()+line.size(), '\n' )) > 1 ) {
          fc::http::header h;
          char* end = line.data();
          while( *end != ':' )++end;
          h.key = fc::string(line.data(),end);
          ++end; // skip ':'
          ++end; // skip space
          char* skey = end;
          while( *end != '\r' ) ++end;
          h.val = fc::string(skey,end);
          rep.headers.push_back(h);
          if( boost::iequals(h.key, "Content-Length") ) {
             rep.body.resize( static_cast<size_t>(to_uint64( fc::string(h.val) ) ));
          }
        }
        if( rep.body.size() ) {
          sock.read( rep.body.data(), rep.body.size() );
        }
        return rep;
      } catch ( fc::exception& e ) {
        elog( "${exception}", ("exception",e.to_detail_string() ) );
        sock.close();
        rep.status = http::reply::InternalServerError;
        return rep;
      } 
   }



namespace fc { namespace http {

         ssl_connection::ssl_connection()
         :my( new ssl_connection::impl() ){}


// used for clients
void       ssl_connection::connect_to( const fc::ip::endpoint& ep, const std::string& hostname ) {
  my->sock.close();
  my->sock.connect_to( my->ep = ep, my->hostname = hostname );
}

http::reply ssl_connection::request( const fc::string& method, 
                                const fc::string& url, 
                                const fc::string& body, const headers& he ) {
	
  fc::url parsed_url(url);
  if( !my->sock.is_open() ) {
    wlog( "Re-open socket!" );
    my->sock.connect_to( my->ep, my->hostname );
  }
  try {
      fc::stringstream req;
      req << method <<" "<<parsed_url.path()->generic_string()<<" HTTP/1.1\r\n";
      req << "Host: "<<*parsed_url.host()<<"\r\n";
      req << "Content-Type: application/json\r\n";
      for( auto i = he.begin(); i != he.end(); ++i )
      {
          req << i->key <<": " << i->val<<"\r\n";
      }
      if( body.size() ) req << "Content-Length: "<< body.size() << "\r\n";
      req << "\r\n"; 
      fc::string head = req.str();

      my->sock.write( head.c_str(), head.size() );
    //  fc::cerr.write( head.c_str() );

      if( body.size() )  {
          my->sock.write( body.c_str(), body.size() );
    //      fc::cerr.write( body.c_str() );
      }
    //  fc::cerr.flush();

      return my->parse_reply();
  } catch ( ... ) {
      my->sock.close();
      FC_THROW_EXCEPTION( exception, "Error Sending HTTPS Request" ); // TODO: provide more info
   //  return http::reply( http::reply::InternalServerError ); // TODO: replace with ssl_connection error
  }
}

// used for servers
fc::tcp_ssl_socket& ssl_connection::get_socket()const {
  return my->sock;
}

http::request    ssl_connection::read_request()const {
  http::request req;
  req.remote_endpoint = fc::variant(get_socket().remote_endpoint()).as_string();
  std::vector<char> line(1024*8);
  int s = my->read_until( line.data(), line.data()+line.size(), ' ' ); // METHOD
  req.method = line.data();
  s = my->read_until( line.data(), line.data()+line.size(), ' ' ); // PATH
  req.path = line.data();
  s = my->read_until( line.data(), line.data()+line.size(), '\n' ); // HTTP/1.0
  
  while( (s = my->read_until( line.data(), line.data()+line.size(), '\n' )) > 1 ) {
    fc::http::header h;
    char* end = line.data();
    while( *end != ':' )++end;
    h.key = fc::string(line.data(),end);
    ++end; // skip ':'
    ++end; // skip space
    char* skey = end;
    while( *end != '\r' ) ++end;
    h.val = fc::string(skey,end);
    req.headers.push_back(h);
    if( boost::iequals(h.key, "Content-Length")) {
       auto s = static_cast<size_t>(to_uint64( fc::string(h.val) ) );
       FC_ASSERT( s < 1024*1024 );
       req.body.resize( static_cast<size_t>(to_uint64( fc::string(h.val) ) ));
    }
    if( boost::iequals(h.key, "Host") ) {
       req.domain = h.val;
    }
  }
  // TODO: some common servers won't give a Content-Length, they'll use 
  // Transfer-Encoding: chunked.  handle that here.

  if( req.body.size() ) {
    my->sock.read( req.body.data(), req.body.size() );
  }
  return req;
}

} } // fc::http
