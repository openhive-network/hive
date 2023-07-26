#pragma once
#include <hive/chain/hive_fwd.hpp>

#include <hive/plugins/json_rpc/json_rpc_plugin.hpp>

#include <hive/plugins/account_history_api/account_history_api_plugin.hpp>
#include <hive/plugins/database_api/database_api_plugin.hpp>
#include <hive/plugins/rc_api/rc_api_plugin.hpp>
#include <hive/plugins/reputation_api/reputation_api_plugin.hpp>

#include <appbase/application.hpp>

#define HIVE_CLIVE_API_PLUGIN_NAME "clive_api"


namespace hive { namespace plugins { namespace clive_api {

/** Plugin provides specialized API for clive tool (encapsulating all required data in single call)
*/
class clive_api_plugin final : public appbase::plugin< clive_api_plugin >
{
public:
  APPBASE_PLUGIN_REQUIRES(
    (hive::plugins::json_rpc::json_rpc_plugin)
    (hive::plugins::database_api::database_api_plugin)
    (hive::plugins::account_history::account_history_api_plugin)
    (hive::plugins::rc::rc_api_plugin)
    (hive::plugins::reputation::reputation_api_plugin)
  )

  clive_api_plugin() = default;
  virtual ~clive_api_plugin() = default;

  static const std::string& name()
     { static std::string name = HIVE_CLIVE_API_PLUGIN_NAME; return name; }

  virtual void set_program_options(appbase::options_description& cli, appbase::options_description& cfg ) override;

  void plugin_initialize( const appbase::variables_map& options ) override;
  void plugin_startup() override;
  void plugin_shutdown() override;
};

} } } // hive::plugins::chain
