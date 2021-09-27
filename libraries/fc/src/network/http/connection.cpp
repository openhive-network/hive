#include <fc/network/http/connection.hpp>
#include <string>

namespace fc { namespace http {

  server::server( const std::string& server_pem, const std::string& ssl_password )
    : server_pem( server_pem ), ssl_password( ssl_password )
  {}

  client::client( const std::string& ca_filename )
    : ca_filename( ca_filename )
  {}

} }