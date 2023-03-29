#include <hive/plugins/wallet_api/wallet_api_plugin.hpp>
#include <hive/plugins/wallet_api/wallet_api.hpp>


namespace hive { namespace plugins { namespace wallet_api {

wallet_api_plugin::wallet_api_plugin() {}
wallet_api_plugin::~wallet_api_plugin() {}

void wallet_api_plugin::set_program_options( options_description& cli, options_description& cfg ) {}

void wallet_api_plugin::plugin_initialize( const variables_map& options )
{
  api = std::make_shared< wallet_api >();
}

void wallet_api_plugin::plugin_startup() {}
void wallet_api_plugin::plugin_shutdown() {}

} } } // hive::plugins::wallet_api
