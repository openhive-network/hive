#include <hive/plugins/block_api/block_api.hpp>
#include <hive/plugins/block_api/block_api_plugin.hpp>

namespace hive { namespace plugins { namespace block_api {

block_api_plugin::block_api_plugin() {}
block_api_plugin::~block_api_plugin() {}

void block_api_plugin::set_program_options(
  options_description& cli,
  options_description& cfg ) {}

void block_api_plugin::plugin_initialize( const variables_map& options )
{
  api = std::make_shared< block_api >( get_app() );
}

void block_api_plugin::plugin_startup() {}

void block_api_plugin::plugin_shutdown() {}

} } } // hive::plugins::block_api
