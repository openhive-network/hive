#include <hive/plugins/metadata_api/metadata_api_plugin.hpp>
#include <hive/plugins/metadata_api/metadata_api.hpp>


namespace hive { namespace plugins { namespace metadata {

metadata_api_plugin::metadata_api_plugin() {}
metadata_api_plugin::~metadata_api_plugin() {}

void metadata_api_plugin::set_program_options( options_description& cli, options_description& cfg ) {}

void metadata_api_plugin::plugin_initialize( const variables_map& options )
{
  api = std::make_shared< metadata_api >( get_app() );
}

void metadata_api_plugin::plugin_startup() {}
void metadata_api_plugin::plugin_shutdown() {}

} } } // hive::plugins::metadata
