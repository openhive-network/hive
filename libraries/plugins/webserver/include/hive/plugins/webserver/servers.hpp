#include <boost/asio.hpp>

#include<memory>

namespace hive { namespace plugins { namespace webserver {

namespace asio = boost::asio;

template<typename websocket_server_type>
struct server
{
  std::shared_ptr< std::thread >  thread;
  asio::io_service                ios;
  websocket_server_type           server;
};

} } } // hive::plugins::webserver
