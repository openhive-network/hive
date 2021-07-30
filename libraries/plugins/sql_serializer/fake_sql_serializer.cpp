#include <hive/plugins/sql_serializer/sql_serializer_plugin.hpp>

namespace hive
{
namespace plugins
{
namespace sql_serializer
{

namespace detail
{

class sql_serializer_plugin_impl final {};

} /// namespace detail

sql_serializer_plugin::sql_serializer_plugin() {}
sql_serializer_plugin::~sql_serializer_plugin() {}

void sql_serializer_plugin::set_program_options(appbase::options_description& cli, appbase::options_description& cfg)
{
  /// Nothing to add here.
}

void sql_serializer_plugin::plugin_initialize(const appbase::variables_map& options)
{
  wlog("Initializing FAKE IMPLEMENTATION of sql_serializer plugin. To use actual one, please check your libpqxx installation");
}

void sql_serializer_plugin::plugin_startup()
{
  wlog("Startup of FAKE IMPLEMENTATION of sql_serializer plugin. To use actual one, please check your libpqxx installation");
}

void sql_serializer_plugin::plugin_shutdown()
{

}

} // namespace sql_serializer
} // namespace plugins
} // namespace hive
