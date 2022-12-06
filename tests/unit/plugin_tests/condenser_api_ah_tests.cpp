#if defined IS_TEST_NET
#include <boost/test/unit_test.hpp>
#include <hive/plugins/account_history_api/account_history_api_plugin.hpp>
#include <hive/plugins/account_history_api/account_history_api.hpp>
#include <hive/plugins/condenser_api/condenser_api_plugin.hpp>
#include <hive/plugins/condenser_api/condenser_api.hpp>
#include <hive/plugins/database_api/database_api_plugin.hpp>

#include "../db_fixture/database_fixture.hpp"

using namespace hive::chain;
using namespace hive::protocol;

struct condenser_api_ah_fixture : database_fixture
{
  condenser_api_ah_fixture()
  {
    auto _data_dir = common_init( [&]( appbase::application& app, int argc, char** argv )
    {
      app.register_plugin< hive::plugins::account_history::account_history_api_plugin >();
      app.register_plugin< hive::plugins::condenser_api::condenser_api_plugin >();
      app.register_plugin< hive::plugins::database_api::database_api_plugin >();
      db_plugin = &app.register_plugin< hive::plugins::debug_node::debug_node_plugin >();

      int test_argc = 1;
      const char* test_argv[] = { boost::unit_test::framework::master_test_suite().argv[ 0 ] };

      db_plugin->logging = false;
      app.initialize<
        hive::plugins::account_history::account_history_api_plugin,
        hive::plugins::condenser_api::condenser_api_plugin,
        hive::plugins::database_api::database_api_plugin,
        hive::plugins::debug_node::debug_node_plugin >( test_argc, ( char** ) test_argv );

      db = &app.get_plugin< hive::plugins::chain::chain_plugin >().db();
      BOOST_REQUIRE( db );

      auto& condenser = app.get_plugin< hive::plugins::condenser_api::condenser_api_plugin >();
      condenser.plugin_startup(); //has to be called because condenser fills its variables then
      condenser_api = condenser.api.get();
      BOOST_REQUIRE( condenser_api );

      auto& account_history = app.get_plugin< hive::plugins::account_history::account_history_api_plugin > ();
      account_history.plugin_startup();
      account_history_api = account_history.api.get();
      BOOST_REQUIRE( account_history_api );
    } );

    init_account_pub_key = init_account_priv_key.get_public_key();

    open_database( _data_dir );

    generate_block();
    db->set_hardfork( HIVE_NUM_HARDFORKS );
    generate_block();

    validate_database();
  }

  hive::plugins::condenser_api::condenser_api* condenser_api = nullptr;
  hive::plugins::account_history::account_history_api* account_history_api = nullptr;
};

BOOST_FIXTURE_TEST_SUITE( condenser_api_ah_tests, condenser_api_ah_fixture );

BOOST_AUTO_TEST_CASE( test_stub )
{ try {

  BOOST_TEST_MESSAGE( "test stub" );

  // Put test scenario here.

  validate_database();

} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_SUITE_END()
#endif
