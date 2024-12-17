#pragma once

#include <hive/plugins/webserver/local_endpoint.hpp>

#include <websocketpp/config/asio_client.hpp>
#include <websocketpp/config/asio.hpp>
#include <websocketpp/server.hpp>
#include <websocketpp/config/asio_client.hpp>
#include <websocketpp/extensions/permessage_deflate/enabled.hpp>
#include <websocketpp/client.hpp>
#include <websocketpp/logger/stub.hpp>
#include <websocketpp/logger/syslog.hpp>

namespace hive { namespace plugins { namespace webserver {

namespace detail {

  struct asio_tls_with_stub_log : public websocketpp::config::asio_tls
  {
  };

  struct asio_with_stub_log : public websocketpp::config::asio
  {
    typedef asio_with_stub_log type;
    typedef asio base;

    typedef base::concurrency_type concurrency_type;

    typedef base::request_type request_type;
    typedef base::response_type response_type;

    typedef base::message_type message_type;
    typedef base::con_msg_manager_type con_msg_manager_type;
    typedef base::endpoint_msg_manager_type endpoint_msg_manager_type;

    typedef base::alog_type alog_type;
    typedef base::elog_type elog_type;
    //typedef websocketpp::log::stub elog_type;
    //typedef websocketpp::log::stub alog_type;

    typedef base::rng_type rng_type;

    struct transport_config : public base::transport_config
    {
      typedef type::concurrency_type concurrency_type;
      typedef type::alog_type alog_type;
      typedef type::elog_type elog_type;
      typedef type::request_type request_type;
      typedef type::response_type response_type;
      typedef websocketpp::transport::asio::basic_socket::endpoint
        socket_type;
    };

    typedef websocketpp::transport::asio::endpoint< transport_config >
      transport_type;

    static const long timeout_open_handshake = 0;
  };

  struct asio_tls_with_stub_log_and_permessage_deflate_enabled : public asio_tls_with_stub_log
  {
    struct permessage_deflate_config {};

    typedef websocketpp::extensions::permessage_deflate::enabled<permessage_deflate_config> permessage_deflate_type;
  };

  struct asio_tls_with_stub_log_and_permessage_deflate_disabled : public asio_tls_with_stub_log
  {
    struct permessage_deflate_config {};

    typedef websocketpp::extensions::permessage_deflate::disabled<permessage_deflate_config> permessage_deflate_type;
  };

  struct asio_with_stub_log_and_permessage_deflate_enabled : public asio_with_stub_log
  {
    struct permessage_deflate_config {};

    typedef websocketpp::extensions::permessage_deflate::enabled<permessage_deflate_config> permessage_deflate_type;
  };

  struct asio_with_stub_log_and_permessage_deflate_disabled : public asio_with_stub_log
  {
    struct permessage_deflate_config {};

    typedef websocketpp::extensions::permessage_deflate::disabled<permessage_deflate_config> permessage_deflate_type;
  };

  struct asio_local_with_stub_log : public websocketpp::config::asio
  {
    typedef asio_local_with_stub_log type;
    typedef asio base;

    typedef base::concurrency_type concurrency_type;

    typedef base::request_type request_type;
    typedef base::response_type response_type;

    typedef base::message_type message_type;
    typedef base::con_msg_manager_type con_msg_manager_type;
    typedef base::endpoint_msg_manager_type endpoint_msg_manager_type;

    typedef base::alog_type alog_type;
    typedef base::elog_type elog_type;

    typedef base::rng_type rng_type;

    struct transport_config : public base::transport_config {
      typedef type::concurrency_type concurrency_type;
      typedef type::alog_type alog_type;
      typedef type::elog_type elog_type;
      typedef type::request_type request_type;
      typedef type::response_type response_type;
      typedef websocketpp::transport::asio::basic_socket::local_endpoint socket_type;
    };

    typedef websocketpp::transport::asio::local_endpoint<transport_config> transport_type;

    static const long timeout_open_handshake = 0;
  };

  struct asio_local_with_stub_log_and_permessage_deflate : public asio_local_with_stub_log
  {
    struct permessage_deflate_config {};

    typedef websocketpp::extensions::permessage_deflate::enabled<permessage_deflate_config> permessage_deflate_type;
  };

  using websocket_tls_server_type_deflate = websocketpp::server<asio_tls_with_stub_log_and_permessage_deflate_enabled>;
  using websocket_tls_server_type_nondeflate = websocketpp::server<asio_tls_with_stub_log_and_permessage_deflate_disabled>;
  using websocket_server_type_deflate = websocketpp::server<asio_with_stub_log_and_permessage_deflate_enabled>;
  using websocket_server_type_nondeflate = websocketpp::server<asio_with_stub_log_and_permessage_deflate_disabled>;
  using websocket_local_server_type = websocketpp::server<detail::asio_local_with_stub_log_and_permessage_deflate>;

} // detail
} } } // hive::plugins::webserver
