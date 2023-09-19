#include <hive/plugins/transaction_status_api/transaction_status_api_plugin.hpp>
#include <hive/plugins/transaction_status_api/transaction_status_api.hpp>

namespace hive { namespace plugins { namespace transaction_status_api {

transaction_status_api_plugin::transaction_status_api_plugin( appbase::application& app ): appbase::plugin<transaction_status_api_plugin>( app ) {}
transaction_status_api_plugin::~transaction_status_api_plugin() {}

void transaction_status_api_plugin::set_program_options( boost::program_options::options_description& cli, boost::program_options::options_description& cfg ) {}

void transaction_status_api_plugin::plugin_initialize( const boost::program_options::variables_map& options )
{
  api = std::make_unique< transaction_status_api >();
}

void transaction_status_api_plugin::plugin_startup() {}

void transaction_status_api_plugin::plugin_shutdown() {}

} } } // hive::plugins::transaction_status_api
