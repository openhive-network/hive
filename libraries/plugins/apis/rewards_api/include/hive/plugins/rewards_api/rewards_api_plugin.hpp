#pragma once
#include <chainbase/forward_declarations.hpp>
#include <hive/plugins/rewards_api/rewards_api.hpp>
#include <hive/plugins/json_rpc/json_rpc_plugin.hpp>

#include <appbase/application.hpp>

namespace hive { namespace plugins { namespace rewards_api {

#define HIVE_REWARDS_API_PLUGIN_NAME "rewards_api"

class rewards_api_plugin : public appbase::plugin< rewards_api_plugin >
{
  public:
    rewards_api_plugin();
    virtual ~rewards_api_plugin();

    APPBASE_PLUGIN_REQUIRES(
      (hive::plugins::json_rpc::json_rpc_plugin)
    )

    static const std::string& name() { static std::string name = HIVE_REWARDS_API_PLUGIN_NAME; return name; }

    virtual void set_program_options(
      boost::program_options::options_description& cli,
      boost::program_options::options_description& cfg ) override;
    virtual void plugin_initialize( const boost::program_options::variables_map& options ) override;
    virtual void plugin_startup() override;
    virtual void plugin_shutdown() override;

  private:
    std::unique_ptr< rewards_api > api;
};

} } } // hive::plugins::rewards_api

