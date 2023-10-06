#include <hive/plugins/test_api/test_api_plugin.hpp>

#include <fc/log/logger_config.hpp>

namespace hive { namespace plugins { namespace test_api {

test_api_plugin::test_api_plugin(): appbase::plugin<test_api_plugin>()
{
  auto& app = get_app();
  JSON_RPC_REGISTER_API( name() );
}

test_api_plugin::~test_api_plugin() {}

void test_api_plugin::plugin_initialize( const variables_map& options )
{
}

void test_api_plugin::plugin_startup() {}
void test_api_plugin::plugin_shutdown() {}

test_api_a_return test_api_plugin::test_api_a( const test_api_a_args& args, bool lock )
{
  FC_UNUSED( lock )
  test_api_a_return result;
  result.value = "A";
  return result;
}

test_api_b_return test_api_plugin::test_api_b( const test_api_b_args& args, bool lock )
{
  FC_UNUSED( lock )
  test_api_b_return result;
  result.value = "B";
  return result;
}

} } } // hive::plugins::test_api
