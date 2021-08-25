#pragma once
#include <chainbase/hive_fwd.hpp>
#include <hive/plugins/json_rpc/json_rpc_plugin.hpp>
#include <hive/plugins/chain/chain_plugin.hpp>
#include <hive/plugins/p2p/p2p_plugin.hpp>
#include <hive/plugins/rc/rc_plugin.hpp>

#include <appbase/application.hpp>

#define HIVE_NETWORK_BROADCAST_API_PLUGIN_NAME "network_broadcast_api"

namespace hive { namespace plugins { namespace network_broadcast_api {

using namespace appbase;

class network_broadcast_api_plugin : public appbase::plugin< network_broadcast_api_plugin >
{
public:
  APPBASE_PLUGIN_REQUIRES(
    (hive::plugins::json_rpc::json_rpc_plugin)
    (hive::plugins::rc::rc_plugin)
    (hive::plugins::chain::chain_plugin)
    (hive::plugins::p2p::p2p_plugin)
  )

  network_broadcast_api_plugin();
  virtual ~network_broadcast_api_plugin();

  static const std::string& name() { static std::string name = HIVE_NETWORK_BROADCAST_API_PLUGIN_NAME; return name; }

  virtual void set_program_options( options_description& cli, options_description& cfg ) override;
  virtual void plugin_initialize( const variables_map& options ) override;
  virtual void plugin_startup() override;
  virtual void plugin_shutdown() override;

  std::shared_ptr< class network_broadcast_api > api;
};

} } } // hive::plugins::network_broadcast_api
