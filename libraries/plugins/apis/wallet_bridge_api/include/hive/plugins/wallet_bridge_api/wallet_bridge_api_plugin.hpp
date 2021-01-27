#pragma once
#include <hive/plugins/chain/chain_plugin.hpp>
#include <hive/plugins/json_rpc/json_rpc_plugin.hpp>
#include <appbase/plugin.hpp>

namespace hive { namespace plugins { namespace wallet_bridge_api {

using namespace appbase;

#define HIVE_WALLET_BRIDGE_API_PLUGIN_NAME "wallet_bridge_api"

class wallet_bridge_api_plugin : public plugin< wallet_bridge_api_plugin >
{
  public:
    wallet_bridge_api_plugin();
    virtual ~wallet_bridge_api_plugin();

    APPBASE_PLUGIN_REQUIRES(
      (chain::chain_plugin)
      (json_rpc::json_rpc_plugin)
    )

    static const std::string& name() { static std::string name = HIVE_WALLET_BRIDGE_API_PLUGIN_NAME; return name; }

    virtual void set_program_options(
      options_description& cli,
      options_description& cfg ) override;
    virtual void plugin_initialize( const variables_map& options ) override;
    virtual void plugin_startup() override;
    virtual void plugin_shutdown() override;

    std::shared_ptr< class wallet_bridge_api > api;
};

} } } //hive::plugins::wallet_bridge_api