#pragma once
#include <chainbase/hive_fwd.hpp>
#include <hive/plugins/market_history/market_history_plugin.hpp>
#include <hive/plugins/json_rpc/json_rpc_plugin.hpp>

#include <appbase/application.hpp>

#define HIVE_MARKET_HISTORY_API_PLUGIN_NAME "market_history_api"


namespace hive { namespace plugins { namespace market_history {

using namespace appbase;

class market_history_api_plugin : public appbase::plugin< market_history_api_plugin >
{
public:
  APPBASE_PLUGIN_REQUIRES(
    (hive::plugins::market_history::market_history_plugin)
    (hive::plugins::json_rpc::json_rpc_plugin)
  )

  market_history_api_plugin();
  virtual ~market_history_api_plugin();

  static const std::string& name() { static std::string name = HIVE_MARKET_HISTORY_API_PLUGIN_NAME; return name; }

  virtual void set_program_options( options_description& cli, options_description& cfg ) override;

  virtual void plugin_initialize( const variables_map& options ) override;
  virtual void plugin_startup() override;
  virtual void plugin_shutdown() override;

  std::shared_ptr< class market_history_api > api;
};

} } } // hive::plugins::market_history
