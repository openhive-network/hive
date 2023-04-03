#pragma once
#include <hive/chain/hive_fwd.hpp>
#include <hive/plugins/clive/clive_plugin.hpp>
#include <hive/plugins/json_rpc/json_rpc_plugin.hpp>

#include <appbase/application.hpp>

#define HIVE_CLIVE_API_PLUGIN_NAME "clive_api"


namespace hive { namespace plugins { namespace clive_api {

using namespace appbase;

class clive_api_plugin : public appbase::plugin< clive_api_plugin >
{
public:
  APPBASE_PLUGIN_REQUIRES(
    (hive::plugins::clive::clive_plugin)
    (hive::plugins::json_rpc::json_rpc_plugin)
  )

  clive_api_plugin();
  virtual ~clive_api_plugin();

  static const std::string& name() { static std::string name = HIVE_CLIVE_API_PLUGIN_NAME; return name; }

  virtual void set_program_options( options_description& cli, options_description& cfg ) override;

  virtual void plugin_initialize( const variables_map& options ) override;
  virtual void plugin_startup() override;
  virtual void plugin_shutdown() override;

  std::shared_ptr< class clive_api > api;
};

} } } // hive::plugins::clive_api
