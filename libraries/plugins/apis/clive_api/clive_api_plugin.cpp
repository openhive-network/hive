#include <hive/plugins/clive_api/clive_api_plugin.hpp>
#include <hive/plugins/clive_api/clive_api.hpp>


namespace hive { namespace plugins { namespace clive_api {

clive_api_plugin::clive_api_plugin() {}
clive_api_plugin::~clive_api_plugin() {}

void clive_api_plugin::set_program_options( options_description& cli, options_description& cfg ) {}

void clive_api_plugin::plugin_initialize( const variables_map& options )
{
  ilog("initializing clive api plugin");
  api = std::make_shared< clive_api >();
}

void clive_api_plugin::plugin_startup() {}
void clive_api_plugin::plugin_shutdown() {}

} } } // hive::plugins::clive_api
