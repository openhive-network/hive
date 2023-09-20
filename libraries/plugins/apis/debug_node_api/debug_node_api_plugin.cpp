#include <hive/plugins/debug_node_api/debug_node_api_plugin.hpp>
#include <hive/plugins/debug_node_api/debug_node_api.hpp>


namespace hive { namespace plugins { namespace debug_node {

debug_node_api_plugin::debug_node_api_plugin( appbase::application& app ): appbase::plugin<debug_node_api_plugin>( app ) {}
debug_node_api_plugin::~debug_node_api_plugin() {}

void debug_node_api_plugin::set_program_options( options_description& cli, options_description& cfg ) {}

void debug_node_api_plugin::plugin_initialize( const variables_map& options )
{
  api = std::make_shared< debug_node_api >( theApp );
}

void debug_node_api_plugin::plugin_startup() {}
void debug_node_api_plugin::plugin_shutdown() {}

} } } // hive::plugins::debug_node
