#include <hive/plugins/node_status_api/node_status_api.hpp>
#include <hive/plugins/node_status_api/node_status_api_plugin.hpp>

namespace hive { namespace plugins { namespace node_status_api {

node_status_api_plugin::node_status_api_plugin() {}
node_status_api_plugin::~node_status_api_plugin() {}

void node_status_api_plugin::set_program_options(options_description& cli, options_description& cfg) {}

void node_status_api_plugin::plugin_initialize(const variables_map& options)
{
  api = std::make_shared<node_status_api>(get_app());
}

void node_status_api_plugin::plugin_startup() {}
void node_status_api_plugin::plugin_shutdown() {}

} } } // hive::plugins::test_api
