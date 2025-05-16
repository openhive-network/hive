#if defined IS_TEST_NET
#include <boost/test/unit_test.hpp>
#include <hive/chain/account_object.hpp>
#include <hive/protocol/hive_operations.hpp>
#include <hive/protocol/config.hpp>
#include <hive/plugins/transaction_status/transaction_status_plugin.hpp>
#include <hive/plugins/transaction_status/transaction_status_objects.hpp>
#include <hive/plugins/transaction_status_api/transaction_status_api_plugin.hpp>
#include <hive/plugins/transaction_status_api/transaction_status_api.hpp>

#include "../db_fixture/hived_fixture.hpp"

using namespace hive::chain;
using namespace hive::protocol;

#define TRANSACTION_STATUS_TEST_BLOCK_DEPTH 30
#define TRANSACTION_STATUS_TEST_BLOCK_DEPTH_STR BOOST_PP_STRINGIZE( TRANSACTION_STATUS_TEST_BLOCK_DEPTH )
#define TRANSACTION_STATUS_TEST_GENESIS_BLOCK_OFFSET 1300

struct empty_fixture
{
};

using trx_status = hive::plugins::transaction_status::transaction_status;

BOOST_FIXTURE_TEST_SUITE( transaction_status, empty_fixture );

BOOST_AUTO_TEST_CASE( transaction_status_test )
{
  using namespace hive::plugins::transaction_status;

  try
  {
    auto _content =[]( hived_fixture& fixture, uint32_t nr_hardforks, uint32_t block_num,
                                const trx_status& status_00, const trx_status& status_01 )
    {
      hive::plugins::transaction_status_api::transaction_status_api_plugin* tx_status_api = nullptr;
      hive::plugins::transaction_status::transaction_status_plugin* tx_status = nullptr;

      // Set genesis time to 1300 blocks ago, and plugin's nominal block depth to 30 so that
      // - actual block depth is 1230 (see transaction_status_plugin::plugin_initialize)
      // - blockchain calculated head num is 1280 (see transaction_status_impl::estimate_starting_timestamp)
      // - plugin's estimated starting block is 50 (ditto)
      configuration::hardfork_schedule_t immediate_hf_schedule = { hardfork_schedule_item_t{ 28, block_num } };
      configuration_data.set_hardfork_schedule(
        fc::time_point::now() - fc::seconds( TRANSACTION_STATUS_TEST_GENESIS_BLOCK_OFFSET * HIVE_BLOCK_INTERVAL ),
        immediate_hf_schedule );
      configuration_data.set_initial_asset_supply( INITIAL_TEST_SUPPLY, HBD_INITIAL_TEST_SUPPLY );
      configuration_data.allow_not_enough_rc = false;

      fixture.postponed_init(
        { 
          hived_fixture::config_line_t( { "plugin",
            { HIVE_TRANSACTION_STATUS_PLUGIN_NAME,
              HIVE_TRANSACTION_STATUS_API_PLUGIN_NAME } }
          ),
          hived_fixture::config_line_t( { "transaction-status-block-depth",
            { TRANSACTION_STATUS_TEST_BLOCK_DEPTH_STR } }
          ),
          hived_fixture::config_line_t( { "shared-file-size",
            { std::to_string( 1024 * 1024 * fixture.shared_file_size_small ) } }
          )
        },
        &tx_status_api,
        &tx_status
      );

      fixture.init_account_pub_key = fixture.init_account_priv_key.get_public_key();

      BOOST_REQUIRE( fixture.db->get_index< transaction_status_index >().indices().get< by_id >().empty() );
      BOOST_REQUIRE( fixture.db->get_index< transaction_status_index >().indices().get< by_trx_id >().empty() );
      BOOST_REQUIRE( fixture.db->get_index< transaction_status_index >().indices().get< by_block_num >().empty() );
      BOOST_REQUIRE( fixture.db->get_index< transaction_status_block_index >().indices().get< by_id >().empty() );
      BOOST_REQUIRE( fixture.db->get_index< transaction_status_block_index >().indices().get< by_block_num >().empty() );

      fixture.generate_block();
      fixture.db->set_hardfork( nr_hardforks );
      fixture.generate_block();
      fixture.db->_log_hardforks = true;

      fixture.vest( HIVE_INIT_MINER_NAME, ASSET( "10.000 TESTS" ) );

      // Fill up the rest of the required miners
      for( int i = HIVE_NUM_INIT_MINERS; i < HIVE_MAX_WITNESSES; i++ )
      {
        fixture.account_create( HIVE_INIT_MINER_NAME + fc::to_string( i ), fixture.init_account_pub_key );
        fixture.fund( HIVE_INIT_MINER_NAME + fc::to_string( i ), HIVE_MIN_PRODUCER_REWARD );
        fixture.witness_create( HIVE_INIT_MINER_NAME + fc::to_string( i ), fixture.init_account_priv_key, "foo.bar", fixture.init_account_pub_key, HIVE_MIN_PRODUCER_REWARD.amount );
      }

      fixture.validate_database();

      ACTORS_EXT( (fixture), (alice)(bob) );
      fixture.generate_block();

      fixture.issue_funds( "alice", ASSET( "1000.000 TESTS" ) );
      fixture.issue_funds( "alice", ASSET( "1000.000 TBD" ) );
      fixture.issue_funds( "bob", ASSET( "1000.000 TESTS" ) );

      fixture.generate_block();

      fixture.validate_database();

      BOOST_TEST_MESSAGE(" -- transaction status tracking test" );

      signed_transaction _tx0;
      transfer_operation op0;
      auto _tx0_expiration = fixture.db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION;

      op0.from = "alice";
      op0.to = "bob";
      op0.amount = ASSET( "5.000 TESTS" );

      // Create transaction 0
      _tx0.operations.push_back( op0 );
      _tx0.set_expiration( _tx0_expiration );
      auto tx0 = fixture.push_transaction( _tx0, alice_private_key, 0, hive::protocol::pack_type::legacy );

      // Tracking should not be enabled until we have reached 
      // TRANSACTION_STATUS_TEST_GENESIS_BLOCK_OFFSET - TRANSACTION_STATUS_TEST_BLOCK_DEPTH blocks
      BOOST_REQUIRE( fixture.db->get_index< transaction_status_index >().indices().get< by_id >().empty() );
      BOOST_REQUIRE( fixture.db->get_index< transaction_status_index >().indices().get< by_trx_id >().empty() );
      BOOST_REQUIRE( fixture.db->get_index< transaction_status_index >().indices().get< by_block_num >().empty() );
      BOOST_REQUIRE( fixture.db->get_index< transaction_status_block_index >().indices().get< by_id >().empty() );
      BOOST_REQUIRE( fixture.db->get_index< transaction_status_block_index >().indices().get< by_block_num >().empty() );

      // Transaction 0 should not be tracked
      auto tso = fixture.db->find< transaction_status_object, by_trx_id >( tx0->get_transaction_id() );
      BOOST_REQUIRE( tso == nullptr );

      auto api_return = tx_status_api->api->find_transaction( { .transaction_id = tx0->get_transaction_id() } );
      BOOST_REQUIRE( api_return.status == unknown );
      BOOST_REQUIRE( api_return.block_num.valid() == false );
      BOOST_REQUIRE( api_return.rc_cost.valid() == false );

      api_return = tx_status_api->api->find_transaction( { .transaction_id = tx0->get_transaction_id(), _tx0_expiration  } );
      BOOST_REQUIRE( api_return.status == unknown );
      BOOST_REQUIRE( api_return.block_num.valid() == false );
      BOOST_REQUIRE( api_return.rc_cost.valid() == false );

      fixture.generate_blocks( TRANSACTION_STATUS_TEST_GENESIS_BLOCK_OFFSET - TRANSACTION_STATUS_TEST_BLOCK_DEPTH - fixture.db->head_block_num() );

      // Tracking began. Tracked blocks exist in the mem pool
      BOOST_REQUIRE( not fixture.db->get_index< transaction_status_block_index >().indices().get< by_id >().empty() );
      BOOST_REQUIRE( not fixture.db->get_index< transaction_status_block_index >().indices().get< by_block_num >().empty() );

      signed_transaction _tx1;
      transfer_operation op1;
      auto _tx1_expiration = fixture.db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION;

      op1.from = "alice";
      op1.to = "bob";
      op1.amount = ASSET( "5.000 TESTS" );

      // Create transaction 1
      _tx1.operations.push_back( op1 );
      _tx1.set_expiration( _tx1_expiration );
      auto tx1 = fixture.push_transaction( _tx1, alice_private_key, 0, hive::protocol::pack_type::legacy );

      // Transaction 1 exists in the mem pool
      tso = fixture.db->find< transaction_status_object, by_trx_id >( tx1->get_transaction_id() );
      BOOST_REQUIRE( tso != nullptr );
      BOOST_REQUIRE( tso->block_num == 0 );
      BOOST_REQUIRE_EQUAL( tso->rc_cost, -1 );

      api_return = tx_status_api->api->find_transaction( { .transaction_id = tx1->get_transaction_id() } );
      BOOST_REQUIRE( api_return.status == within_mempool );
      BOOST_REQUIRE( api_return.block_num.valid() == false );
      BOOST_REQUIRE( api_return.rc_cost.valid() == false );

      api_return = tx_status_api->api->find_transaction( { .transaction_id = tx1->get_transaction_id(), _tx1_expiration } );
      BOOST_REQUIRE( api_return.status == within_mempool );
      BOOST_REQUIRE( api_return.block_num.valid() == false );
      BOOST_REQUIRE( api_return.rc_cost.valid() == false );

      // Transaction 1 block is missing in the mem pool (block not generated yet)
      auto tx_block_num = fixture.db->head_block_num() + 1;
      auto tsbo = fixture.db->find< transaction_status_block_object, by_block_num >( tx_block_num );
      BOOST_REQUIRE( tsbo == nullptr );

      fixture.generate_block();

      // After block generation transaction's block number is updated in object ...
      tso = fixture.db->find< transaction_status_object, by_trx_id >( tx1->get_transaction_id() );
      BOOST_REQUIRE( tso != nullptr );
      BOOST_REQUIRE( tso->block_num == tx_block_num );
      // ... and the block exists in the mem pool
      tsbo = fixture.db->find< transaction_status_block_object, by_block_num >( tso->block_num );
      BOOST_REQUIRE( tsbo != nullptr );
      BOOST_REQUIRE( tsbo->block_num == tso->block_num );
      BOOST_REQUIRE( tsbo->timestamp > fc::time_point_sec() );
      auto tx1_block_num = tso->block_num;

      /*
        * Test for two transactions in the same block
        */

      // Create transaction 2
      signed_transaction _tx2;
      transfer_operation op2;
      auto _tx2_expiration = fixture.db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION;

      op2.from = "alice";
      op2.to = "bob";
      op2.amount = ASSET( "5.000 TESTS" );

      _tx2.operations.push_back( op2 );
      _tx2.set_expiration( _tx2_expiration );
      auto tx2 = fixture.push_transaction( _tx2, alice_private_key, 0, hive::protocol::pack_type::legacy );

      // Create transaction 3
      signed_transaction _tx3;
      transfer_operation op3;
      auto _tx3_expiration = fixture.db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION;

      op3.from = "bob";
      op3.to = "alice";
      op3.amount = ASSET( "5.000 TESTS" );

      _tx3.operations.push_back( op3 );
      _tx3.set_expiration( _tx3_expiration );
      auto tx3 = fixture.push_transaction( _tx3, bob_private_key, 0, hive::protocol::pack_type::legacy );

      // Transaction 1 exists in a block
      tso = fixture.db->find< transaction_status_object, by_trx_id >( tx1->get_transaction_id() );
      BOOST_REQUIRE( tso != nullptr );
      BOOST_REQUIRE( tso->block_num == fixture.db->head_block_num() );
      BOOST_REQUIRE_GT( tso->rc_cost, 0 );

      api_return = tx_status_api->api->find_transaction( { .transaction_id = tx1->get_transaction_id() } );
      BOOST_REQUIRE( api_return.status == within_reversible_block );
      BOOST_REQUIRE( api_return.block_num.valid() );
      BOOST_REQUIRE_EQUAL( *api_return.block_num, fixture.db->head_block_num() );
      BOOST_REQUIRE( api_return.rc_cost.valid() );
      BOOST_REQUIRE_EQUAL( *api_return.rc_cost, tso->rc_cost );

      api_return = tx_status_api->api->find_transaction( { .transaction_id = tx1->get_transaction_id(), _tx1_expiration } );
      BOOST_REQUIRE( api_return.status == within_reversible_block );
      BOOST_REQUIRE( api_return.block_num.valid() );
      BOOST_REQUIRE_EQUAL( *api_return.block_num, fixture.db->head_block_num() );
      BOOST_REQUIRE( api_return.rc_cost.valid() );
      BOOST_REQUIRE_EQUAL( *api_return.rc_cost, tso->rc_cost );

      // Transaction 1 block exists in the mem pool
      tsbo = fixture.db->find< transaction_status_block_object, by_block_num >( tso->block_num );
      BOOST_REQUIRE( tsbo != nullptr );
      BOOST_REQUIRE( tsbo->block_num == tso->block_num );
      BOOST_REQUIRE( tsbo->timestamp > fc::time_point_sec() );

      // Transaction 2 exists in a mem pool
      tso = fixture.db->find< transaction_status_object, by_trx_id >( tx2->get_transaction_id() );
      BOOST_REQUIRE( tso != nullptr );
      BOOST_REQUIRE( tso->block_num == 0 );
      BOOST_REQUIRE_EQUAL( tso->rc_cost, -1 );

      api_return = tx_status_api->api->find_transaction( { .transaction_id = tx2->get_transaction_id() } );
      BOOST_REQUIRE( api_return.status == within_mempool );
      BOOST_REQUIRE( api_return.block_num.valid() == false );
      BOOST_REQUIRE( api_return.rc_cost.valid() == false );

      api_return = tx_status_api->api->find_transaction( { .transaction_id = tx2->get_transaction_id(), _tx2_expiration } );
      BOOST_REQUIRE( api_return.status == within_mempool );
      BOOST_REQUIRE( api_return.block_num.valid() == false );
      BOOST_REQUIRE( api_return.rc_cost.valid() == false );

      // Transaction 3 exists in a mem pool
      tso = fixture.db->find< transaction_status_object, by_trx_id >( tx3->get_transaction_id() );
      BOOST_REQUIRE( tso != nullptr );
      BOOST_REQUIRE( tso->block_num == 0 );
      BOOST_REQUIRE_EQUAL( tso->rc_cost, -1 );

      api_return = tx_status_api->api->find_transaction( { .transaction_id = tx3->get_transaction_id() } );
      BOOST_REQUIRE( api_return.status == within_mempool );
      BOOST_REQUIRE( api_return.block_num.valid() == false );
      BOOST_REQUIRE( api_return.rc_cost.valid() == false );

      api_return = tx_status_api->api->find_transaction( { .transaction_id = tx3->get_transaction_id(), _tx3_expiration } );
      BOOST_REQUIRE( api_return.status == within_mempool );
      BOOST_REQUIRE( api_return.block_num.valid() == false );
      BOOST_REQUIRE( api_return.rc_cost.valid() == false );

      fixture.generate_block(); 

      fixture.generate_blocks( TRANSACTION_STATUS_TEST_BLOCK_DEPTH -1 );

      // Transaction 1 exists in a block
      tso = fixture.db->find< transaction_status_object, by_trx_id >( tx1->get_transaction_id() );
      BOOST_REQUIRE( tso != nullptr );
      BOOST_REQUIRE( tso->block_num > 0 );
      BOOST_REQUIRE_GT( tso->rc_cost, 0 );

      api_return = tx_status_api->api->find_transaction( { .transaction_id = tx1->get_transaction_id() } );
      BOOST_REQUIRE( api_return.status == within_irreversible_block );
      BOOST_REQUIRE( api_return.block_num.valid() );
      BOOST_REQUIRE_EQUAL( *api_return.block_num, tso->block_num );
      BOOST_REQUIRE( api_return.rc_cost.valid() );
      BOOST_REQUIRE_EQUAL( *api_return.rc_cost, tso->rc_cost );

      api_return = tx_status_api->api->find_transaction( { .transaction_id = tx1->get_transaction_id(), _tx1_expiration } );
      BOOST_REQUIRE( api_return.status == within_irreversible_block );
      BOOST_REQUIRE( api_return.block_num.valid() );
      BOOST_REQUIRE_EQUAL( *api_return.block_num, tso->block_num );
      BOOST_REQUIRE( api_return.rc_cost.valid() );
      BOOST_REQUIRE_EQUAL( *api_return.rc_cost, tso->rc_cost );

      // Transaction 1 block exists in the mem pool
      tsbo = fixture.db->find< transaction_status_block_object, by_block_num >( tso->block_num );
      BOOST_REQUIRE( tsbo != nullptr );
      BOOST_REQUIRE( tsbo->block_num == tso->block_num );
      BOOST_REQUIRE( tsbo->timestamp > fc::time_point_sec() );

      // Transaction 2 exists in a block
      tso = fixture.db->find< transaction_status_object, by_trx_id >( tx2->get_transaction_id() );
      BOOST_REQUIRE( tso != nullptr );
      BOOST_REQUIRE( tso->block_num > 0 );
      BOOST_REQUIRE_GT( tso->rc_cost, 0 );

      api_return = tx_status_api->api->find_transaction( { .transaction_id = tx2->get_transaction_id() } );
      BOOST_REQUIRE( api_return.status == within_irreversible_block );
      BOOST_REQUIRE( api_return.block_num.valid() );
      BOOST_REQUIRE_EQUAL( *api_return.block_num, tso->block_num );
      BOOST_REQUIRE( api_return.rc_cost.valid() );
      BOOST_REQUIRE_EQUAL( *api_return.rc_cost, tso->rc_cost );

      api_return = tx_status_api->api->find_transaction( { .transaction_id = tx2->get_transaction_id(), _tx2_expiration } );
      BOOST_REQUIRE( api_return.status == within_irreversible_block );
      BOOST_REQUIRE( api_return.block_num.valid() );
      BOOST_REQUIRE_EQUAL( *api_return.block_num, tso->block_num );
      BOOST_REQUIRE( api_return.rc_cost.valid() );
      BOOST_REQUIRE_EQUAL( *api_return.rc_cost, tso->rc_cost );

      // Transaction 2 block exists in the mem pool
      tsbo = fixture.db->find< transaction_status_block_object, by_block_num >( tso->block_num );
      BOOST_REQUIRE( tsbo != nullptr );
      BOOST_REQUIRE( tsbo->block_num == tso->block_num );
      BOOST_REQUIRE( tsbo->timestamp > fc::time_point_sec() );
      auto tx2_block_num = tso->block_num;

      // Transaction 3 exists in a block
      tso = fixture.db->find< transaction_status_object, by_trx_id >( tx3->get_transaction_id() );
      BOOST_REQUIRE( tso != nullptr );
      BOOST_REQUIRE( tso->block_num > 0 );
      BOOST_REQUIRE_GT( tso->rc_cost, 0 );

      api_return = tx_status_api->api->find_transaction( { .transaction_id = tx3->get_transaction_id() } );
      BOOST_REQUIRE( api_return.status == within_irreversible_block );
      BOOST_REQUIRE( api_return.block_num.valid() );
      BOOST_REQUIRE_EQUAL( *api_return.block_num, tso->block_num );
      BOOST_REQUIRE( api_return.rc_cost.valid() );
      BOOST_REQUIRE_EQUAL( *api_return.rc_cost, tso->rc_cost );

      api_return = tx_status_api->api->find_transaction( { .transaction_id = tx3->get_transaction_id(), _tx3_expiration } );
      BOOST_REQUIRE( api_return.status == within_irreversible_block );
      BOOST_REQUIRE( api_return.block_num.valid() );
      BOOST_REQUIRE_EQUAL( *api_return.block_num, tso->block_num );
      BOOST_REQUIRE( api_return.rc_cost.valid() );
      BOOST_REQUIRE_EQUAL( *api_return.rc_cost, tso->rc_cost );

      // Transaction 3 block exists in the mem pool
      tsbo = fixture.db->find< transaction_status_block_object, by_block_num >( tso->block_num );
      BOOST_REQUIRE( tsbo != nullptr );
      BOOST_REQUIRE( tsbo->block_num == tso->block_num );
      BOOST_REQUIRE( tsbo->timestamp > fc::time_point_sec() );
      auto tx3_block_num = tso->block_num;

      fixture.generate_blocks( HIVE_MAX_TIME_UNTIL_EXPIRATION / HIVE_BLOCK_INTERVAL );

      // Transaction 1 is no longer tracked
      tso = fixture.db->find< transaction_status_object, by_trx_id >( tx1->get_transaction_id() );
      BOOST_REQUIRE( tso == nullptr );

      api_return = tx_status_api->api->find_transaction( { .transaction_id = tx1->get_transaction_id() } );
      BOOST_REQUIRE( api_return.status == unknown );
      BOOST_REQUIRE( api_return.block_num.valid() == false );
      BOOST_REQUIRE( api_return.rc_cost.valid() == false );

      api_return = tx_status_api->api->find_transaction( { .transaction_id = tx1->get_transaction_id(), _tx1_expiration } );
      BOOST_REQUIRE( api_return.status == too_old );
      BOOST_REQUIRE( api_return.block_num.valid() == false );
      BOOST_REQUIRE( api_return.rc_cost.valid() == false );

      // Transaction 1 block is no longer tracked
      tsbo = fixture.db->find< transaction_status_block_object, by_block_num >( tx1_block_num );
      BOOST_REQUIRE( tsbo == nullptr );

      // Transaction 2 exists in a block
      tso = fixture.db->find< transaction_status_object, by_trx_id >( tx2->get_transaction_id() );
      BOOST_REQUIRE( tso != nullptr );
      BOOST_REQUIRE( tso->block_num > 0 );
      BOOST_REQUIRE_GT( tso->rc_cost, 0 );

      api_return = tx_status_api->api->find_transaction( { .transaction_id = tx2->get_transaction_id() } );
      BOOST_REQUIRE( api_return.status == within_irreversible_block );
      BOOST_REQUIRE( api_return.block_num.valid() );
      BOOST_REQUIRE_EQUAL( *api_return.block_num, tso->block_num );
      BOOST_REQUIRE( api_return.rc_cost.valid() );
      BOOST_REQUIRE_EQUAL( *api_return.rc_cost, tso->rc_cost );

      api_return = tx_status_api->api->find_transaction( { .transaction_id = tx2->get_transaction_id(), _tx2_expiration } );
      BOOST_REQUIRE( api_return.status == within_irreversible_block );
      BOOST_REQUIRE( api_return.block_num.valid() );
      BOOST_REQUIRE_EQUAL( *api_return.block_num, tso->block_num );
      BOOST_REQUIRE( api_return.rc_cost.valid() );
      BOOST_REQUIRE_EQUAL( *api_return.rc_cost, tso->rc_cost );

      // Transaction 2 block exists in the mem pool
      tsbo = fixture.db->find< transaction_status_block_object, by_block_num >( tso->block_num );
      BOOST_REQUIRE( tsbo != nullptr );
      BOOST_REQUIRE( tsbo->block_num == tso->block_num );
      BOOST_REQUIRE( tsbo->timestamp > fc::time_point_sec() );

      // Transaction 3 exists in a block
      tso = fixture.db->find< transaction_status_object, by_trx_id >( tx3->get_transaction_id() );
      BOOST_REQUIRE( tso != nullptr );
      BOOST_REQUIRE( tso->block_num > 0 );
      BOOST_REQUIRE_GT( tso->rc_cost, 0 );

      api_return = tx_status_api->api->find_transaction( { .transaction_id = tx3->get_transaction_id() } );
      BOOST_REQUIRE( api_return.status == within_irreversible_block );
      BOOST_REQUIRE( api_return.block_num.valid() );
      BOOST_REQUIRE_EQUAL( *api_return.block_num, tso->block_num );
      BOOST_REQUIRE( api_return.rc_cost.valid() );
      BOOST_REQUIRE_EQUAL( *api_return.rc_cost, tso->rc_cost );

      api_return = tx_status_api->api->find_transaction( { .transaction_id = tx3->get_transaction_id(), _tx3_expiration } );
      BOOST_REQUIRE( api_return.status == within_irreversible_block );
      BOOST_REQUIRE( api_return.block_num.valid() );
      BOOST_REQUIRE_EQUAL( *api_return.block_num, tso->block_num );
      BOOST_REQUIRE( api_return.rc_cost.valid() );
      BOOST_REQUIRE_EQUAL( *api_return.rc_cost, tso->rc_cost );

      // Transaction 3 block exists in the mem pool
      tsbo = fixture.db->find< transaction_status_block_object, by_block_num >( tso->block_num );
      BOOST_REQUIRE( tsbo != nullptr );
      BOOST_REQUIRE( tsbo->block_num == tso->block_num );
      BOOST_REQUIRE( tsbo->timestamp > fc::time_point_sec() );

      fixture.generate_block();

      // Transaction 2 is no longer tracked
      tso = fixture.db->find< transaction_status_object, by_trx_id >( tx2->get_transaction_id() );
      BOOST_REQUIRE( tso == nullptr );

      api_return = tx_status_api->api->find_transaction( { .transaction_id = tx2->get_transaction_id() } );
      BOOST_REQUIRE( api_return.status == unknown );
      BOOST_REQUIRE( api_return.block_num.valid() == false );
      BOOST_REQUIRE( api_return.rc_cost.valid() == false );

      api_return = tx_status_api->api->find_transaction( { .transaction_id = tx2->get_transaction_id(), _tx2_expiration } );
      BOOST_REQUIRE( api_return.status == too_old );
      BOOST_REQUIRE( api_return.block_num.valid() == false );
      BOOST_REQUIRE( api_return.rc_cost.valid() == false );

      // Transaction 2 block is no longer tracked
      tsbo = fixture.db->find< transaction_status_block_object, by_block_num >( tx2_block_num );
      BOOST_REQUIRE( tsbo == nullptr );

      // Transaction 3 is no longer tracked
      tso = fixture.db->find< transaction_status_object, by_trx_id >( tx3->get_transaction_id() );
      BOOST_REQUIRE( tso == nullptr );

      api_return = tx_status_api->api->find_transaction( { .transaction_id = tx3->get_transaction_id() } );
      BOOST_REQUIRE( api_return.status == unknown );
      BOOST_REQUIRE( api_return.block_num.valid() == false );
      BOOST_REQUIRE( api_return.rc_cost.valid() == false );

      api_return = tx_status_api->api->find_transaction( { .transaction_id = tx3->get_transaction_id(), _tx3_expiration } );
      BOOST_REQUIRE( api_return.status == too_old );
      BOOST_REQUIRE( api_return.block_num.valid() == false );
      BOOST_REQUIRE( api_return.rc_cost.valid() == false );

      // Transaction 3 block is no longer tracked
      tsbo = fixture.db->find< transaction_status_block_object, by_block_num >( tx3_block_num );
      BOOST_REQUIRE( tsbo == nullptr );

      // At this point our index should be empty
      BOOST_REQUIRE( fixture.db->get_index< transaction_status_index >().indices().get< by_id >().empty() );
      BOOST_REQUIRE( fixture.db->get_index< transaction_status_index >().indices().get< by_trx_id >().empty() );
      BOOST_REQUIRE( fixture.db->get_index< transaction_status_index >().indices().get< by_block_num >().empty() );
      // Each transaction block is already removed from our index (see above). 
      // Also no older block should exist in our index.
      auto lower_bound = std::max< uint32_t >( { tx1_block_num, tx2_block_num, tx3_block_num} );
      const auto& bidx = fixture.db->get_index< transaction_status_block_index >().indices().get< by_block_num >();
      auto bitr = bidx.begin();
      BOOST_REQUIRE( bitr != bidx.end() );
      BOOST_REQUIRE( bitr->block_num > lower_bound );

      fixture.generate_block();

      api_return = tx_status_api->api->find_transaction( { .transaction_id = tx1->get_transaction_id() } );
      BOOST_REQUIRE( api_return.status == unknown );
      BOOST_REQUIRE( api_return.block_num.valid() == false );
      BOOST_REQUIRE( api_return.rc_cost.valid() == false );

      api_return = tx_status_api->api->find_transaction( { .transaction_id = tx1->get_transaction_id(), _tx1_expiration } );
      BOOST_REQUIRE( api_return.status == too_old );
      BOOST_REQUIRE( api_return.block_num.valid() == false );
      BOOST_REQUIRE( api_return.rc_cost.valid() == false );

      api_return = tx_status_api->api->find_transaction( { .transaction_id = tx2->get_transaction_id() } );
      BOOST_REQUIRE( api_return.status == unknown );
      BOOST_REQUIRE( api_return.block_num.valid() == false );
      BOOST_REQUIRE( api_return.rc_cost.valid() == false );

      api_return = tx_status_api->api->find_transaction( { .transaction_id = tx2->get_transaction_id(), _tx2_expiration } );
      BOOST_REQUIRE( api_return.status == too_old );
      BOOST_REQUIRE( api_return.block_num.valid() == false );
      BOOST_REQUIRE( api_return.rc_cost.valid() == false );

      /**
        * Testing transactions that do not exist, but expirations are provided
        */
      BOOST_TEST_MESSAGE( " -- transaction status expiration test" );

      // The time of our last irreversible block
      auto lib_time = fixture.get_block_reader().get_block_by_number( 
        fixture.db->get_last_irreversible_block_num() )->get_block_header().timestamp;
      api_return = tx_status_api->api->find_transaction( { .transaction_id = transaction_id_type(), .expiration = lib_time } );
      BOOST_REQUIRE( api_return.status == status_00 );
      BOOST_REQUIRE( api_return.block_num.valid() == false );
      BOOST_REQUIRE( api_return.rc_cost.valid() == false );

      // One second after our last irreversible block
      auto after_lib_time = lib_time + fc::seconds(1);
      api_return = tx_status_api->api->find_transaction( { .transaction_id = transaction_id_type(), after_lib_time } );
      BOOST_REQUIRE( api_return.status == status_01 );
      BOOST_REQUIRE( api_return.block_num.valid() == false );
      BOOST_REQUIRE( api_return.rc_cost.valid() == false );

      // One second before our block depth
      auto old_time = fixture.get_block_reader().get_block_by_number( 
        fixture.db->head_block_num() - TRANSACTION_STATUS_TEST_BLOCK_DEPTH + 1 )->get_block_header().timestamp - fc::seconds(1);
      api_return = tx_status_api->api->find_transaction( { .transaction_id = transaction_id_type(), old_time } );
      BOOST_REQUIRE( api_return.status == too_old );
      BOOST_REQUIRE( api_return.block_num.valid() == false );
      BOOST_REQUIRE( api_return.rc_cost.valid() == false );
    };

    {
      hived_fixture _fixture;
      const uint32_t _nr_hardforks = 27;
      uint32_t _block_num = 1'000'000;
      trx_status _status_00 = trx_status::expired_irreversible;
      trx_status _status_01 = trx_status::expired_reversible;

      _content( _fixture, _nr_hardforks, _block_num, _status_00, _status_01 );
    }
    {
      hived_fixture _fixture;
      const uint32_t _nr_hardforks = 28;
      uint32_t _block_num = 1;
      trx_status _status_00 = trx_status::too_old;
      trx_status _status_01 = trx_status::too_old;

      _content( _fixture, _nr_hardforks, _block_num, _status_00, _status_01 );
    }
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_SUITE_END()
#endif

