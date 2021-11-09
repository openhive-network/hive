#pragma once
#include <websocketpp/config/asio_client.hpp>
#include <websocketpp/config/asio.hpp>
#include <websocketpp/server.hpp>
#include <websocketpp/config/asio_client.hpp>
#include <websocketpp/extensions/permessage_deflate/enabled.hpp>
#include <websocketpp/client.hpp>
#include <websocketpp/logger/stub.hpp>


namespace fc { namespace http { namespace detail {

  typedef std::shared_ptr< std::thread > thread_ptr;

#define FC_HTTP_CONFIG_PERMESSAGE_DEFLATE 1 // Undef this to disable the compression

  struct asio_with_stub_log : public websocketpp::config::asio {
    typedef asio_with_stub_log type;
    typedef asio base;

    typedef base::concurrency_type concurrency_type;

    typedef base::request_type request_type;
    typedef base::response_type response_type;

    typedef base::message_type message_type;
    typedef base::con_msg_manager_type con_msg_manager_type;
    typedef base::endpoint_msg_manager_type endpoint_msg_manager_type;

    typedef websocketpp::log::stub elog_type;
    typedef websocketpp::log::stub alog_type;

    typedef base::rng_type rng_type;

    struct transport_config : public base::transport_config {
      typedef type::concurrency_type concurrency_type;
      typedef type::alog_type alog_type;
      typedef type::elog_type elog_type;
      typedef type::request_type request_type;
      typedef type::response_type response_type;
      typedef websocketpp::transport::asio::basic_socket::endpoint socket_type;
    };

    typedef websocketpp::transport::asio::endpoint<transport_config> transport_type;

    static const long timeout_open_handshake = 0;

#ifdef FC_HTTP_CONFIG_PERMESSAGE_DEFLATE
    struct permessage_deflate_config {};

    typedef websocketpp::extensions::permessage_deflate::enabled<permessage_deflate_config> permessage_deflate_type;
#endif
  };

  struct asio_tls_stub_log : public websocketpp::config::asio_tls {
    typedef asio_tls_stub_log type;
    typedef asio_tls base;

    typedef base::concurrency_type concurrency_type;

    typedef base::request_type request_type;
    typedef base::response_type response_type;

    typedef base::message_type message_type;
    typedef base::con_msg_manager_type con_msg_manager_type;
    typedef base::endpoint_msg_manager_type endpoint_msg_manager_type;

    typedef websocketpp::log::stub elog_type;
    typedef websocketpp::log::stub alog_type;

    typedef base::rng_type rng_type;

    struct transport_config : public base::transport_config {
      typedef type::concurrency_type concurrency_type;
      typedef type::alog_type alog_type;
      typedef type::elog_type elog_type;
      typedef type::request_type request_type;
      typedef type::response_type response_type;
      typedef websocketpp::transport::asio::tls_socket::endpoint socket_type;
    };

    typedef websocketpp::transport::asio::endpoint<transport_config> transport_type;

#ifdef FC_HTTP_CONFIG_PERMESSAGE_DEFLATE
    struct permessage_deflate_config {};

    typedef websocketpp::extensions::permessage_deflate::enabled<permessage_deflate_config> permessage_deflate_type;
#endif
  };

  using websocketpp::connection_hdl;
  typedef websocketpp::server< asio_with_stub_log > server_type;
  typedef websocketpp::server< asio_tls_stub_log  > tls_server_type;

  typedef websocketpp::lib::shared_ptr< boost::asio::ssl::context > context_ptr;

  typedef websocketpp::client< asio_with_stub_log > client_type;
  typedef websocketpp::client< asio_tls_stub_log  > tls_client_type;

  typedef client_type::connection_ptr     client_connection_type;
  typedef tls_client_type::connection_ptr tls_client_connection_type;

  typedef server_type::connection_ptr     server_connection_type;
  typedef tls_server_type::connection_ptr tls_server_connection_type;

} } } // fc::http::detail