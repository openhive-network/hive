#pragma once
#include <hive/plugins/json_rpc/json_rpc_plugin.hpp>
#include <hive/plugins/chain/chain_plugin.hpp>

#include <appbase/application.hpp>

#define HIVE_NODE_STATUS_API_PLUGIN_NAME "node_status_api"

namespace hive { namespace plugins { namespace node_status_api {

using namespace appbase;

class node_status_api_plugin : public appbase::plugin< node_status_api_plugin >
{
  public:
    APPBASE_PLUGIN_REQUIRES((hive::plugins::json_rpc::json_rpc_plugin)
                            (hive::plugins::chain::chain_plugin))

    node_status_api_plugin();
    virtual ~node_status_api_plugin();

    static const std::string& name()
    {
      static std::string name = HIVE_NODE_STATUS_API_PLUGIN_NAME;
      return name;
    }

    virtual void set_program_options(options_description& cli, options_description& cfg) override;
    virtual void plugin_initialize(const variables_map& options) override;
    virtual void plugin_startup() override;
    virtual void plugin_shutdown() override;

    std::shared_ptr<class node_status_api> api;
};

} } } // hive::plugins::node_status_api
