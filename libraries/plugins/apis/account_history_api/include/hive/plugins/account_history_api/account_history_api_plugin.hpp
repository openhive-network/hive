#pragma once
#include <hive/chain/hive_fwd.hpp>
#include <hive/plugins/json_rpc/json_rpc_plugin.hpp>
#include <hive/plugins/account_history_rocksdb/account_history_rocksdb_plugin.hpp>

#include <appbase/application.hpp>

#define HIVE_ACCOUNT_HISTORY_API_PLUGIN_NAME "account_history_api"


namespace hive { namespace plugins { namespace account_history {

using namespace appbase;

class account_history_api_plugin : public plugin< account_history_api_plugin >
{
public:
  APPBASE_PLUGIN_REQUIRES(
    (hive::plugins::json_rpc::json_rpc_plugin)
    (hive::plugins::account_history_rocksdb::account_history_rocksdb_plugin)
  )

  account_history_api_plugin( appbase::application& app );
  virtual ~account_history_api_plugin();

  static const std::string& name() { static std::string name = HIVE_ACCOUNT_HISTORY_API_PLUGIN_NAME; return name; }

  virtual void set_program_options( options_description& cli, options_description& cfg ) override;

  virtual void plugin_initialize( const variables_map& options ) override;
  virtual void plugin_startup() override;
  virtual void plugin_shutdown() override;

  std::shared_ptr< class account_history_api > api;
};

} } } // hive::plugins::account_history
