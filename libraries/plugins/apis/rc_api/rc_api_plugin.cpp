#include <hive/plugins/rc_api/rc_api_plugin.hpp>
#include <hive/plugins/rc_api/rc_api.hpp>


namespace hive { namespace plugins { namespace rc {

rc_api_plugin::rc_api_plugin( appbase::application& app ): appbase::plugin<rc_api_plugin>( app ) {}
rc_api_plugin::~rc_api_plugin() {}

void rc_api_plugin::set_program_options( options_description& cli, options_description& cfg ) {}

void rc_api_plugin::plugin_initialize( const variables_map& options )
{
  api = std::make_shared< rc_api >( theApp );
}

void rc_api_plugin::plugin_startup() {}
void rc_api_plugin::plugin_shutdown() {}

} } } // hive::plugins::rc
