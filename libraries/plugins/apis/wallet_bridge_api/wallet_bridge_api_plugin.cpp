#include <hive/plugins/wallet_bridge_api/wallet_bridge_api.hpp>
#include <hive/plugins/wallet_bridge_api/wallet_bridge_api_plugin.hpp>

namespace hive { namespace plugins { namespace wallet_bridge_api {

wallet_bridge_api_plugin::wallet_bridge_api_plugin() {}
wallet_bridge_api_plugin::~wallet_bridge_api_plugin() {}

void wallet_bridge_api_plugin::set_program_options(
  options_description& cli,
  options_description& cfg ) {}

void wallet_bridge_api_plugin::plugin_initialize( const variables_map& options )
{
  api = std::make_shared< wallet_bridge_api >();
}

void wallet_bridge_api_plugin::plugin_startup() 
{
  ilog("Wallet bridge api plugin initialization...");
  api->api_startup();
}

void wallet_bridge_api_plugin::plugin_shutdown() {}

} } } //hive::plugins::wallet_bridge_api
