#pragma once
#include <hive/plugins/json_rpc/json_rpc_plugin.hpp>

#include <appbase/application.hpp>

#define HIVE_API_STATUS_API_PLUGIN_NAME "app_status_api"

namespace hive { namespace plugins { namespace app_status_api {

using namespace appbase;

class app_status_api_plugin : public appbase::plugin< app_status_api_plugin >
{
  public:
    APPBASE_PLUGIN_REQUIRES((hive::plugins::json_rpc::json_rpc_plugin))

    app_status_api_plugin();
    virtual ~app_status_api_plugin();

    static const std::string& name()
    {
      static std::string name = HIVE_API_STATUS_API_PLUGIN_NAME;
      return name;
    }

    virtual void set_program_options(options_description& cli, options_description& cfg) override;
    virtual void plugin_initialize(const variables_map& options) override;
    virtual void plugin_startup() override;
    virtual void plugin_shutdown() override;

    std::shared_ptr<class app_status_api> api;
};

} } } // hive::plugins::app_status_api
