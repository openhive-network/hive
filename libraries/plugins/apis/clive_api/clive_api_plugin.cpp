#include <hive/plugins/clive_api/clive_api_plugin.hpp>

namespace hive { namespace plugins { namespace clive_api {

void clive_api_plugin::set_program_options( appbase::options_description& cli, appbase::options_description& cfg ) {}

void clive_api_plugin::plugin_initialize( const appbase::variables_map& options )
{
  _api = std::make_unique<clive_api>();
}

void clive_api_plugin::plugin_startup() {}
void clive_api_plugin::plugin_shutdown() {}

} } } // hive::plugins::clive_api
