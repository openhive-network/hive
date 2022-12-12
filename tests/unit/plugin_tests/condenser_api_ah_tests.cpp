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
using namespace hive::plugins::condenser_api;

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

// account history API -> where it's used in condenser API implementation
//  get_ops_in_block -> get_ops_in_block
//  get_transaction -> ditto get_transaction
//  get_account_history -> ditto get_account_history
//  enum_virtual_ops -> not used

BOOST_AUTO_TEST_CASE( single_transaction_test )
{ try {

  ACTORS((alice)(bob));

  fund( "alice", 500000000 );
  vest( "alice", 200000000 );

  generate_blocks(30);

  transfer_operation op;
  op.from = "alice";
  op.to = "bob";
  op.amount = asset(1000,HIVE_SYMBOL);
  signed_transaction tx;
  tx.operations.push_back( op );

  tx.ref_block_num = 0;
  tx.ref_block_prefix = 0;
  tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );

  push_transaction( tx, alice_private_key );
  auto transaction_block_number = db->head_block_num();

  // Let's make sure that current head block is irreversible.
  BOOST_REQUIRE( transaction_block_number == db->get_last_irreversible_block_num() );
  
  // Check condenser variant, which accepts 2 args and calls ah's variant with default value of include_reversieble = false.
  auto block_ops = condenser_api->get_ops_in_block({transaction_block_number /*block_num*/, false /*only_virtual*/});
  BOOST_REQUIRE( block_ops.size() == 1 ); // <- one irreversible operation (virtal/producer_reward_operation)
  // Check account history variant using include_reversible = true.
  auto ah_block_ops = account_history_api->get_ops_in_block({transaction_block_number, false, true /*include_reversible*/});
  BOOST_REQUIRE( ah_block_ops.ops.size() == 3 ); // <- additional two reversible operations.
  // Problem #1 - The variants return different number of operations, though the block is supposedly irreversible.

  generate_blocks(60);
  // Later blocks should be irreversible now.
  BOOST_REQUIRE( transaction_block_number < db->get_last_irreversible_block_num() );

  // Let's check the block again, sixty blocks later.
  block_ops = condenser_api->get_ops_in_block({transaction_block_number /*block_num*/, false /*only_virtual*/});
  BOOST_REQUIRE( block_ops.size() == 1 );  // <- no change here
  ah_block_ops = account_history_api->get_ops_in_block({transaction_block_number, false, true /*include_reversible*/});
  BOOST_REQUIRE( ah_block_ops.ops.size() == 2 ); // <- one operation has been reversed
  // Problem #2 - An operation disappeared from irreversible block.

} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_SUITE_END()
#endif
