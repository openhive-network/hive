#pragma once
#include <appbase/application.hpp>

#include <hive/plugins/json_rpc/json_rpc_plugin.hpp>

#include <boost/thread.hpp>
#include <boost/container/vector.hpp>

#define HIVE_WEBSERVER_PLUGIN_NAME "webserver"

namespace hive { namespace plugins { namespace webserver {

namespace detail { class webserver_base; }

using namespace appbase;
using collector_t = hive::utilities::notifications::collector_t;

/**
  * This plugin starts an HTTP/ws webserver and dispatches queries to
  * registered handles based on payload. The payload must be conform
  * to the JSONRPC 2.0 spec.
  *
  * The handler will be called from the appbase application io_service
  * thread.  The callback can be called from any thread and will
  * automatically propagate the call to the http thread.
  *
  * The HTTP service will run in its own thread with its own io_service to
  * make sure that HTTP request processing does not interfer with other
  * plugins.
  */
class webserver_plugin : public appbase::plugin< webserver_plugin >
{
  public:
    webserver_plugin( appbase::application& app );
    virtual ~webserver_plugin();

    APPBASE_PLUGIN_REQUIRES( (hive::plugins::json_rpc::json_rpc_plugin) );

    static const std::string& name() { static std::string name = HIVE_WEBSERVER_PLUGIN_NAME; return name; }

    virtual void set_program_options(options_description&, options_description& cfg) override;

    void start_webserver();

    boost::signals2::connection add_connection( std::function<void(const collector_t&)> func );

  protected:
    virtual void plugin_initialize(const variables_map& options) override;
    virtual void plugin_startup() override;
    virtual void plugin_pre_shutdown() override;
    virtual void plugin_shutdown() override;

  private:
    std::unique_ptr< detail::webserver_base > my;
};

} } } // hive::plugins::webserver
