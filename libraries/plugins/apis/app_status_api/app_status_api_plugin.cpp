#include <hive/plugins/app_status_api/app_status_api.hpp>
#include <hive/plugins/app_status_api/app_status_api_plugin.hpp>

namespace hive { namespace plugins { namespace app_status_api {

app_status_api_plugin::app_status_api_plugin() {}
app_status_api_plugin::~app_status_api_plugin() {}

void app_status_api_plugin::set_program_options(options_description& cli, options_description& cfg) {}

void app_status_api_plugin::plugin_initialize(const variables_map& options)
{
  api = std::make_shared<app_status_api>( get_app() );
}

void app_status_api_plugin::plugin_startup() {}
void app_status_api_plugin::plugin_shutdown() {}

} } } // hive::plugins::test_api
