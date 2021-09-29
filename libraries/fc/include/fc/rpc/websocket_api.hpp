#pragma once

#include <fc/rpc/network_api_connection.hpp>
#include <fc/network/http/websocket.hpp>

namespace fc { namespace rpc {

   class websocket_api_connection : public network_api_connection
   {
      public:
         websocket_api_connection( fc::http::websocket_connection& c );
         ~websocket_api_connection();
   };

} } // fc::rpc