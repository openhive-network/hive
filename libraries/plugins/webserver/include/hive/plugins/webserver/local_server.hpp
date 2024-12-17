#pragma once

#include <hive/plugins/webserver/webserver_types.hpp>
#include <hive/plugins/json_rpc/json_rpc_plugin.hpp>

#include <boost/asio.hpp>

#include<optional>

namespace hive { namespace plugins { namespace webserver { namespace detail {

namespace asio = boost::asio;

using boost::asio::ip::tcp;
using websocketpp::connection_hdl;

struct local_server
{
  std::optional< boost::asio::local::stream_protocol::endpoint > endpoint;

  std::shared_ptr< std::thread >  thread;
  websocket_local_server_type     server;

  void handle_http_request( plugins::json_rpc::json_rpc_plugin* api, asio::io_service* thread_pool_ios, connection_hdl hdl );
  void start( asio::io_service& http_ios, plugins::json_rpc::json_rpc_plugin* api, asio::io_service* thread_pool_ios );

  ~local_server();
};

} } } } // hive::plugins::webserver::detail
