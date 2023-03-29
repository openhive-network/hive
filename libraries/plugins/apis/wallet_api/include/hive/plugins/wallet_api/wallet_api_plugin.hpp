#pragma once
#include <hive/chain/hive_fwd.hpp>
#include <hive/plugins/wallet/wallet_plugin.hpp>
#include <hive/plugins/json_rpc/json_rpc_plugin.hpp>

#include <appbase/application.hpp>

#define HIVE_WALLET_API_PLUGIN_NAME "wallet_api"


namespace hive { namespace plugins { namespace wallet_api {

using namespace appbase;

class wallet_api_plugin : public appbase::plugin< wallet_api_plugin >
{
public:
  APPBASE_PLUGIN_REQUIRES(
    (hive::plugins::wallet::wallet_plugin)
    (hive::plugins::json_rpc::json_rpc_plugin)
  )

  wallet_api_plugin();
  virtual ~wallet_api_plugin();

  static const std::string& name() { static std::string name = HIVE_WALLET_API_PLUGIN_NAME; return name; }

  virtual void set_program_options( options_description& cli, options_description& cfg ) override;

  virtual void plugin_initialize( const variables_map& options ) override;
  virtual void plugin_startup() override;
  virtual void plugin_shutdown() override;

  std::shared_ptr< class wallet_api > api;
};

} } } // hive::plugins::wallet_api
