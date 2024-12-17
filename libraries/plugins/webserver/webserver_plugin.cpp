#include <hive/plugins/webserver/webserver_plugin.hpp>

#include <hive/plugins/webserver/webserver_types.hpp>
#include <hive/plugins/webserver/webserver.hpp>
#include <hive/plugins/webserver/local_server.hpp>

#include <fc/network/resolve.hpp>

#include <thread>

namespace hive { namespace plugins { namespace webserver {

typedef uint32_t thread_pool_size_t;

namespace detail {

class webserver_base
{
  public:

    virtual void startup() = 0;
    virtual void start_webserver() = 0;
    virtual void stop_webserver() = 0;
    virtual ~webserver_base() {};

    virtual boost::signals2::connection add_connection( std::function<void(const collector_t&)> ) = 0;

    virtual void process_webserver_parameters(
        const std::optional<std::string>& webserver_http_endpoint,
        const std::optional<std::string>& webserver_https_endpoint,
        const std::optional<std::string>& webserver_ws_endpoint,
        const std::optional<std::string>& webserver_wss_endpoint,
        const std::optional<std::string>& webserver_https_certificate_file_name,
        const std::optional<std::string>& webserver_https_key_file_name,
        const std::optional<std::string>& webserver_wss_certificate_file_name,
        const std::optional<std::string>& webserver_wss_key_file_name
      ) = 0;

    local_server unix;
};

template<typename websocket_server_type>
class webserver_plugin_impl : public webserver_base
{
  public:
    webserver_plugin_impl( thread_pool_size_t _thread_pool_size, appbase::application& app ) :
      thread_pool_size( _thread_pool_size ), http( server_type::http ), ws( server_type::ws ), theApp( app )
    {
    }

    void startup() override;
    void prepare_threads();

    void start_webserver() override;
    void stop_webserver() override;

    thread_pool_size_t         thread_pool_size;

    webserver<websocket_server_type>  http;
    webserver<websocket_server_type>  ws;

    boost::thread_group        thread_pool;
    asio::io_service           thread_pool_ios;
    std::unique_ptr< asio::io_service::work > thread_pool_work;

    plugins::json_rpc::json_rpc_plugin* api = nullptr;

    using signal_t = boost::signals2::signal<void(const collector_t &)>;
    signal_t listen;
    boost::signals2::connection add_connection( std::function<void(const collector_t&)> func ) override;

    void process_webserver_parameters(
        const std::optional<std::string>& webserver_http_endpoint,
        const std::optional<std::string>& webserver_https_endpoint,
        const std::optional<std::string>& webserver_ws_endpoint,
        const std::optional<std::string>& webserver_wss_endpoint,
        const std::optional<std::string>& webserver_https_certificate_file_name,
        const std::optional<std::string>& webserver_https_key_file_name,
        const std::optional<std::string>& webserver_wss_certificate_file_name,
        const std::optional<std::string>& webserver_wss_key_file_name
      )override;

  private:

    appbase::application& theApp;

    void notify( const std::string& type, const std::optional< tcp::endpoint >& endpoint );
};

template<typename websocket_server_type>
void webserver_plugin_impl<websocket_server_type>::startup()
{
  api = theApp.find_plugin< plugins::json_rpc::json_rpc_plugin >();
  FC_ASSERT( api != nullptr, "Could not find API Register Plugin" );

  prepare_threads();
}

template<typename websocket_server_type>
void webserver_plugin_impl<websocket_server_type>::prepare_threads()
{
  thread_pool_work.reset( new asio::io_service::work( this->thread_pool_ios ) );

  for( uint32_t i = 0; i < thread_pool_size; ++i )
    thread_pool.create_thread( [&]() { fc::set_thread_name("api"); thread_pool_ios.run(); } );
}

template<typename websocket_server_type>
void webserver_plugin_impl<websocket_server_type>::notify( const std::string& type, const std::optional< tcp::endpoint >& endpoint )
{
  collector_t collector;

  FC_ASSERT( endpoint.has_value() );

  collector.assign_values(
    "type",     type,
    "address",  endpoint->address().to_string(),
    "port",     endpoint->port()
  );

  listen( collector );
  theApp.notify( "webserver listening", std::move( collector ) );
};

template<typename websocket_server_type>
void webserver_plugin_impl<websocket_server_type>::start_webserver()
{
  const bool _ws_and_http_uses_same_endpoint = http.endpoint && http.endpoint == ws.endpoint && http.endpoint->port() != 0;

  auto _notify = [this]( const std::string& type, const std::optional< tcp::endpoint >& endpoint)
  {
    notify( type, endpoint );
  };

  ws.start( _ws_and_http_uses_same_endpoint, api, &thread_pool_ios, _notify );
  http.start( _ws_and_http_uses_same_endpoint, api, &thread_pool_ios, _notify );
  unix.start( http.ios, api, &thread_pool_ios );
}

template<typename websocket_server_type>
void webserver_plugin_impl<websocket_server_type>::stop_webserver()
{
  thread_pool_ios.stop();
  thread_pool.join_all();
}

template<typename websocket_server_type>
boost::signals2::connection webserver_plugin_impl<websocket_server_type>::add_connection( std::function<void(const collector_t&)> func )
{
  return listen.connect( func );
}

template<typename websocket_server_type>
void webserver_plugin_impl<websocket_server_type>::process_webserver_parameters(
    const std::optional<std::string>& webserver_http_endpoint,
    const std::optional<std::string>& webserver_https_endpoint,
    const std::optional<std::string>& webserver_ws_endpoint,
    const std::optional<std::string>& webserver_wss_endpoint,
    const std::optional<std::string>& webserver_https_certificate_file_name,
    const std::optional<std::string>& webserver_https_key_file_name,
    const std::optional<std::string>& webserver_wss_certificate_file_name,
    const std::optional<std::string>& webserver_wss_key_file_name
  )
{

  auto _process_parameters = [&](
    webserver<websocket_server_type> & server,
    const std::optional<std::string>& non_secure_endpoint,
    const std::optional<std::string>& secure_endpoint,
    const std::optional<std::string>& certificate_file_name,
    const std::optional<std::string>& key_file_name
  )
  {
    if( secure_endpoint )
    {
      FC_ASSERT( certificate_file_name, "Option `webserver-" + server.name() + "s-certificate-file-name` is required" );
      FC_ASSERT( key_file_name, "Option `webserver-" + server.name() + "s-key-file-name` is required" );

      server.set_secure_connection( theApp, *certificate_file_name, *key_file_name );
    }
    else
    {
      if( certificate_file_name )
        ilog( "Option `webserver-" + server.name() + "s-certificate-file-name` is avoided. It's used only for " + server.name() + "s connection." );

      if( key_file_name )
        ilog( "Option `webserver-" + server.name() + "s-key-file-name` is avoided. It's used only for " + server.name() + "s connection." );
    }

    if( non_secure_endpoint || secure_endpoint )
    {
      std::string _non_secure_or_secure_endpoint = server.is_secure_connection() ? *secure_endpoint : *non_secure_endpoint;

      auto _endpoints = fc::resolve_string_to_ip_endpoints( _non_secure_or_secure_endpoint );

      FC_ASSERT( _endpoints.size(), "${endpoint_type} ${hostname} did not resolve",
                ("endpoint_type", "webserver-" + server.name() + "-endpoint")("hostname", _non_secure_or_secure_endpoint) );

      server.endpoint = tcp::endpoint( boost::asio::ip::address_v4::from_string( ( string )_endpoints[0].get_address() ), _endpoints[0].port() );
      ilog( "configured ${type} to listen on ${ep}", ("type", server.name())("ep", _endpoints[0]) );
    }
  };

  _process_parameters(
    http,
    webserver_http_endpoint,
    webserver_https_endpoint,
    webserver_https_certificate_file_name,
    webserver_https_key_file_name
  );

  _process_parameters(
    ws,
    webserver_ws_endpoint,
    webserver_wss_endpoint,
    webserver_wss_certificate_file_name,
    webserver_wss_key_file_name
  );
}

} // detail

webserver_plugin::webserver_plugin()
{
  set_pre_shutdown_order(webserver_order);
}

webserver_plugin::~webserver_plugin() {}

void webserver_plugin::set_program_options( options_description&, options_description& cfg )
{
  cfg.add_options()
    ("webserver-http-endpoint", bpo::value< string >(), "Local http endpoint for webserver requests.")
    ("webserver-https-endpoint", bpo::value< string >(), "Local https endpoint for webserver requests.")
    ("webserver-unix-endpoint", bpo::value< string >(), "Local unix http endpoint for webserver requests.")
    ("webserver-ws-endpoint", bpo::value< string >(), "Local websocket endpoint for webserver requests.")
    ("webserver-wss-endpoint", bpo::value< string >(), "Local secure websocket endpoint for webserver requests.")
    // TODO: maybe add a flag to make this optional
    ("webserver-ws-deflate", bpo::value<bool>()->default_value( false ), "Enable the RFC-7692 permessage-deflate extension for the WebSocket server (only used if the client requests it).  This may save bandwidth at the expense of CPU")
    ("webserver-thread-pool-size", bpo::value<thread_pool_size_t>()->default_value(32),
      "Number of threads used to handle queries. Default: 32.")
    ("webserver-https-certificate-file-name", bpo::value< string >(), "File name with a server's certificate for HTTPS requests." )
    ("webserver-https-key-file-name", bpo::value< string >(), "File name with a server's private key for HTTPS requests." )
    ("webserver-wss-certificate-file-name", bpo::value< string >(), "File name with a server's certificate for WSS requests." )
    ("webserver-wss-key-file-name", bpo::value< string >(), "File name with a server's private key for WSS requests." );
    ;
}

void webserver_plugin::plugin_initialize( const variables_map& options )
{
  ilog("initializing webserver plugin");
  auto thread_pool_size = options.at("webserver-thread-pool-size").as<thread_pool_size_t>();
  FC_ASSERT(thread_pool_size > 0, "webserver-thread-pool-size must be greater than 0");
  ilog("configured with ${tps} thread pool size", ("tps", thread_pool_size));

  auto _ws_deflate_enabled = options.at( "webserver-ws-deflate" ).as< bool >();
  ilog("Compression in webserver is ${_ws_deflate_enabled}", ("_ws_deflate_enabled", _ws_deflate_enabled ? "enabled" : "disabled"));

  std::optional<std::string> webserver_http_endpoint                = options.count( "webserver-http-endpoint" ) ? options.at( "webserver-http-endpoint" ).as< string >() : std::optional<std::string>();
  std::optional<std::string> webserver_https_endpoint               = options.count( "webserver-https-endpoint" ) ? options.at( "webserver-https-endpoint" ).as< string >() : std::optional<std::string>();
  std::optional<std::string> webserver_ws_endpoint                  = options.count( "webserver-ws-endpoint" ) ? options.at( "webserver-ws-endpoint" ).as< string >() : std::optional<std::string>();
  std::optional<std::string> webserver_wss_endpoint                 = options.count( "webserver-wss-endpoint" ) ? options.at( "webserver-wss-endpoint" ).as< string >() : std::optional<std::string>();

  std::optional<std::string> webserver_https_certificate_file_name  = options.count( "webserver-https-certificate-file-name" ) ? options.at( "webserver-https-certificate-file-name" ).as< string >() : std::optional<std::string>();
  std::optional<std::string> webserver_https_key_file_name          = options.count( "webserver-https-key-file-name" ) ? options.at( "webserver-https-key-file-name" ).as< string >() : std::optional<std::string>();
  std::optional<std::string> webserver_wss_certificate_file_name    = options.count( "webserver-wss-certificate-file-name" ) ? options.at( "webserver-wss-certificate-file-name" ).as< string >() : std::optional<std::string>();
  std::optional<std::string> webserver_wss_key_file_name            = options.count( "webserver-wss-key-file-name" ) ? options.at( "webserver-wss-key-file-name" ).as< string >() : std::optional<std::string>();

  if( webserver_https_endpoint || webserver_wss_endpoint )
  {
    if( _ws_deflate_enabled )
      my.reset( new detail::webserver_plugin_impl<detail::websocket_tls_server_type_deflate>( thread_pool_size, get_app() ) );
    else
      my.reset( new detail::webserver_plugin_impl<detail::websocket_tls_server_type_nondeflate>( thread_pool_size, get_app() ) );
  }
  else
  {
    if( _ws_deflate_enabled )
      my.reset( new detail::webserver_plugin_impl<detail::websocket_server_type_deflate>( thread_pool_size, get_app() ) );
    else
      my.reset( new detail::webserver_plugin_impl<detail::websocket_server_type_nondeflate>( thread_pool_size, get_app() ) );
  }

  if( options.count( "webserver-unix-endpoint" ) )
  {
    auto unix_endpoint = options.at( "webserver-unix-endpoint" ).as< string >();
    boost::asio::local::stream_protocol::endpoint ep(unix_endpoint);
    my->unix.endpoint = ep;
    ilog( "configured http to listen on ${ep}", ("ep", unix_endpoint ));
  }

  my->process_webserver_parameters(
    webserver_http_endpoint,
    webserver_https_endpoint,
    webserver_ws_endpoint,
    webserver_wss_endpoint,
    webserver_https_certificate_file_name,
    webserver_https_key_file_name,
    webserver_wss_certificate_file_name,
    webserver_wss_key_file_name
  );
}

void webserver_plugin::plugin_startup()
{
  my->startup();
}

void webserver_plugin::plugin_pre_shutdown()
{
  ilog("Shutting down webserver_plugin...");
  my->stop_webserver();
}

void webserver_plugin::plugin_shutdown()
{
  // everything moved to pre_shutdown
}

void webserver_plugin::start_webserver()
{
  my->start_webserver();
}

boost::signals2::connection webserver_plugin::add_connection( std::function<void(const collector_t &)> func )
{
  return my->add_connection( func );
}

} } } // hive::plugins::webserver
