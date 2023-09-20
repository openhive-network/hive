#include <hive/plugins/rewards_api/rewards_api_plugin.hpp>
#include <hive/plugins/rewards_api/rewards_api.hpp>

namespace hive { namespace plugins { namespace rewards_api {

rewards_api_plugin::rewards_api_plugin( appbase::application& app ): appbase::plugin<rewards_api_plugin>( app ) {}
rewards_api_plugin::~rewards_api_plugin() {}

void rewards_api_plugin::set_program_options( boost::program_options::options_description& cli, boost::program_options::options_description& cfg ) {}

void rewards_api_plugin::plugin_initialize( const boost::program_options::variables_map& options )
{
  api = std::make_unique< rewards_api >( theApp );
}

void rewards_api_plugin::plugin_startup()
{
  elog( "NOTIFYALERT! ${name} is for testing purposes only, do not run in production", ("name", name()) );
}

void rewards_api_plugin::plugin_shutdown() {}

} } } // hive::plugins::rewards_api

