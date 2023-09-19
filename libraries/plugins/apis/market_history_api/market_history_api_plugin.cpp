#include <hive/plugins/market_history_api/market_history_api_plugin.hpp>
#include <hive/plugins/market_history_api/market_history_api.hpp>


namespace hive { namespace plugins { namespace market_history {

market_history_api_plugin::market_history_api_plugin( appbase::application& app ): appbase::plugin<market_history_api_plugin>( app ) {}
market_history_api_plugin::~market_history_api_plugin() {}

void market_history_api_plugin::set_program_options( options_description& cli, options_description& cfg ) {}

void market_history_api_plugin::plugin_initialize( const variables_map& options )
{
  api = std::make_shared< market_history_api >();
}

void market_history_api_plugin::plugin_startup() {}
void market_history_api_plugin::plugin_shutdown() {}

} } } // hive::plugins::market_history
