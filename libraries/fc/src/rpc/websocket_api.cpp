#include <fc/rpc/websocket_api.hpp>

namespace fc { namespace rpc {

  websocket_api_connection::~websocket_api_connection()
  {
  }

  websocket_api_connection::websocket_api_connection( fc::http::websocket_connection& c )
    : network_api_connection(c)
  {
    c.on_http_handler( [&]( const std::string& msg ){ return network_api_connection::on_message(msg,false); } );
  }

} } // fc::rpc