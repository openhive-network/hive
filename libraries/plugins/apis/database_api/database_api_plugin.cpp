#include <hive/plugins/database_api/database_api.hpp>
#include <hive/plugins/database_api/database_api_plugin.hpp>

namespace hive { namespace plugins { namespace database_api {

database_api_plugin::database_api_plugin( appbase::application& app ): appbase::plugin<database_api_plugin>( app ) {}
database_api_plugin::~database_api_plugin() {}

void database_api_plugin::set_program_options(
  options_description& cli,
  options_description& cfg ) {}

void database_api_plugin::plugin_initialize( const variables_map& options )
{
  api = std::make_shared< database_api >( theApp );
}

void database_api_plugin::plugin_startup() {}

void database_api_plugin::plugin_shutdown() {}

} } } // hive::plugins::database_api
