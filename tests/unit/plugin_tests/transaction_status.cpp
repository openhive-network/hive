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

BOOST_FIXTURE_TEST_SUITE( transaction_status, hived_fixture );

BOOST_AUTO_TEST_CASE( transaction_status_test )
{
  using namespace hive::plugins::transaction_status;

  try
  {
    hive::plugins::transaction_status_api::transaction_status_api_plugin* tx_status_api = nullptr;
    hive::plugins::transaction_status::transaction_status_plugin* tx_status = nullptr;

    // Set genesis time to 1300 blocks ago, and plugin's nominal block depth to 30 so that
    // - actual block depth is 1230 (see transaction_status_plugin::plugin_initialize)
    // - blockchain calculated head num is 1280 (see transaction_status_impl::estimate_starting_timestamp)
    // - plugin's estimated starting block is 50 (ditto)
    configuration::hardfork_schedule_t immediate_hf28_schedule = { hardfork_schedule_item_t{ 28, 1 } };
    configuration_data.set_hardfork_schedule(
      fc::time_point::now() - fc::seconds( TRANSACTION_STATUS_TEST_GENESIS_BLOCK_OFFSET * HIVE_BLOCK_INTERVAL ),
      immediate_hf28_schedule );
    configuration_data.set_initial_asset_supply( INITIAL_TEST_SUPPLY, HBD_INITIAL_TEST_SUPPLY );
    configuration_data.allow_not_enough_rc = false;

    postponed_init(
      { 
        config_line_t( { "plugin",
          { HIVE_TRANSACTION_STATUS_PLUGIN_NAME,
            HIVE_TRANSACTION_STATUS_API_PLUGIN_NAME } }
        ),
        config_line_t( { "transaction-status-block-depth",
          { TRANSACTION_STATUS_TEST_BLOCK_DEPTH_STR } }
        ),
        config_line_t( { "shared-file-size",
          { std::to_string( 1024 * 1024 * shared_file_size_in_mb_64 ) } }
        )
      },
      &tx_status_api,
      &tx_status
    );

    init_account_pub_key = init_account_priv_key.get_public_key();

    BOOST_REQUIRE( db->get_index< transaction_status_index >().indices().get< by_id >().empty() );
    BOOST_REQUIRE( db->get_index< transaction_status_index >().indices().get< by_trx_id >().empty() );
    BOOST_REQUIRE( db->get_index< transaction_status_index >().indices().get< by_block_num >().empty() );
    BOOST_REQUIRE( db->get_index< transaction_status_block_index >().indices().get< by_id >().empty() );
    BOOST_REQUIRE( db->get_index< transaction_status_block_index >().indices().get< by_block_num >().empty() );

    generate_block();
    db->set_hardfork( HIVE_NUM_HARDFORKS );
    generate_block();
    db->_log_hardforks = true;

    vest( HIVE_INIT_MINER_NAME, ASSET( "10.000 TESTS" ) );

    // Fill up the rest of the required miners
    for( int i = HIVE_NUM_INIT_MINERS; i < HIVE_MAX_WITNESSES; i++ )
    {
      account_create( HIVE_INIT_MINER_NAME + fc::to_string( i ), init_account_pub_key );
      fund( HIVE_INIT_MINER_NAME + fc::to_string( i ), HIVE_MIN_PRODUCER_REWARD );
      witness_create( HIVE_INIT_MINER_NAME + fc::to_string( i ), init_account_priv_key, "foo.bar", init_account_pub_key, HIVE_MIN_PRODUCER_REWARD.amount );
    }

    validate_database();

    ACTORS( (alice)(bob) );
    generate_block();

    issue_funds( "alice", ASSET( "1000.000 TESTS" ) );
    issue_funds( "alice", ASSET( "1000.000 TBD" ) );
    issue_funds( "bob", ASSET( "1000.000 TESTS" ) );

    generate_block();

    validate_database();

    BOOST_TEST_MESSAGE(" -- transaction status tracking test" );

    signed_transaction _tx0;
    transfer_operation op0;
    auto _tx0_expiration = db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION;

    op0.from = "alice";
    op0.to = "bob";
    op0.amount = ASSET( "5.000 TESTS" );

    // Create transaction 0
    _tx0.operations.push_back( op0 );
    _tx0.set_expiration( _tx0_expiration );
    auto tx0 = push_transaction( _tx0, alice_private_key, 0, hive::protocol::pack_type::legacy );

    // Tracking should not be enabled until we have reached 
    // TRANSACTION_STATUS_TEST_GENESIS_BLOCK_OFFSET - TRANSACTION_STATUS_TEST_BLOCK_DEPTH blocks
    BOOST_REQUIRE( db->get_index< transaction_status_index >().indices().get< by_id >().empty() );
    BOOST_REQUIRE( db->get_index< transaction_status_index >().indices().get< by_trx_id >().empty() );
    BOOST_REQUIRE( db->get_index< transaction_status_index >().indices().get< by_block_num >().empty() );
    BOOST_REQUIRE( db->get_index< transaction_status_block_index >().indices().get< by_id >().empty() );
    BOOST_REQUIRE( db->get_index< transaction_status_block_index >().indices().get< by_block_num >().empty() );

    // Transaction 0 should not be tracked
    auto tso = db->find< transaction_status_object, by_trx_id >( tx0->get_transaction_id() );
    BOOST_REQUIRE( tso == nullptr );

    auto api_return = tx_status_api->api->find_transaction( { .transaction_id = tx0->get_transaction_id() } );
    BOOST_REQUIRE( api_return.status == unknown );
    BOOST_REQUIRE( api_return.block_num.valid() == false );
    BOOST_REQUIRE( api_return.rc_cost.valid() == false );

    api_return = tx_status_api->api->find_transaction( { .transaction_id = tx0->get_transaction_id(), _tx0_expiration  } );
    BOOST_REQUIRE( api_return.status == unknown );
    BOOST_REQUIRE( api_return.block_num.valid() == false );
    BOOST_REQUIRE( api_return.rc_cost.valid() == false );

    generate_blocks( TRANSACTION_STATUS_TEST_GENESIS_BLOCK_OFFSET - TRANSACTION_STATUS_TEST_BLOCK_DEPTH - db->head_block_num() );

    // Tracking began. Tracked blocks exist in the mem pool
    BOOST_REQUIRE( not db->get_index< transaction_status_block_index >().indices().get< by_id >().empty() );
    BOOST_REQUIRE( not db->get_index< transaction_status_block_index >().indices().get< by_block_num >().empty() );

    signed_transaction _tx1;
    transfer_operation op1;
    auto _tx1_expiration = db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION;

    op1.from = "alice";
    op1.to = "bob";
    op1.amount = ASSET( "5.000 TESTS" );

    // Create transaction 1
    _tx1.operations.push_back( op1 );
    _tx1.set_expiration( _tx1_expiration );
    auto tx1 = push_transaction( _tx1, alice_private_key, 0, hive::protocol::pack_type::legacy );

    // Transaction 1 exists in the mem pool
    tso = db->find< transaction_status_object, by_trx_id >( tx1->get_transaction_id() );
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
    auto tx_block_num = db->head_block_num() + 1;
    auto tsbo = db->find< transaction_status_block_object, by_block_num >( tx_block_num );
    BOOST_REQUIRE( tsbo == nullptr );

    generate_block();

    // After block generation transaction's block number is updated in object ...
    tso = db->find< transaction_status_object, by_trx_id >( tx1->get_transaction_id() );
    BOOST_REQUIRE( tso != nullptr );
    BOOST_REQUIRE( tso->block_num == tx_block_num );
    // ... and the block exists in the mem pool
    tsbo = db->find< transaction_status_block_object, by_block_num >( tso->block_num );
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
    auto _tx2_expiration = db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION;

    op2.from = "alice";
    op2.to = "bob";
    op2.amount = ASSET( "5.000 TESTS" );

    _tx2.operations.push_back( op2 );
    _tx2.set_expiration( _tx2_expiration );
    auto tx2 = push_transaction( _tx2, alice_private_key, 0, hive::protocol::pack_type::legacy );

    // Create transaction 3
    signed_transaction _tx3;
    transfer_operation op3;
    auto _tx3_expiration = db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION;

    op3.from = "bob";
    op3.to = "alice";
    op3.amount = ASSET( "5.000 TESTS" );

    _tx3.operations.push_back( op3 );
    _tx3.set_expiration( _tx3_expiration );
    auto tx3 = push_transaction( _tx3, bob_private_key, 0, hive::protocol::pack_type::legacy );

    // Transaction 1 exists in a block
    tso = db->find< transaction_status_object, by_trx_id >( tx1->get_transaction_id() );
    BOOST_REQUIRE( tso != nullptr );
    BOOST_REQUIRE( tso->block_num == db->head_block_num() );
    BOOST_REQUIRE_GT( tso->rc_cost, 0 );

    api_return = tx_status_api->api->find_transaction( { .transaction_id = tx1->get_transaction_id() } );
    BOOST_REQUIRE( api_return.status == within_reversible_block );
    BOOST_REQUIRE( api_return.block_num.valid() );
    BOOST_REQUIRE_EQUAL( *api_return.block_num, db->head_block_num() );
    BOOST_REQUIRE( api_return.rc_cost.valid() );
    BOOST_REQUIRE_EQUAL( *api_return.rc_cost, tso->rc_cost );

    api_return = tx_status_api->api->find_transaction( { .transaction_id = tx1->get_transaction_id(), _tx1_expiration } );
    BOOST_REQUIRE( api_return.status == within_reversible_block );
    BOOST_REQUIRE( api_return.block_num.valid() );
    BOOST_REQUIRE_EQUAL( *api_return.block_num, db->head_block_num() );
    BOOST_REQUIRE( api_return.rc_cost.valid() );
    BOOST_REQUIRE_EQUAL( *api_return.rc_cost, tso->rc_cost );

    // Transaction 1 block exists in the mem pool
    tsbo = db->find< transaction_status_block_object, by_block_num >( tso->block_num );
    BOOST_REQUIRE( tsbo != nullptr );
    BOOST_REQUIRE( tsbo->block_num == tso->block_num );
    BOOST_REQUIRE( tsbo->timestamp > fc::time_point_sec() );

    // Transaction 2 exists in a mem pool
    tso = db->find< transaction_status_object, by_trx_id >( tx2->get_transaction_id() );
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
    tso = db->find< transaction_status_object, by_trx_id >( tx3->get_transaction_id() );
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

    generate_block(); 

    generate_blocks( TRANSACTION_STATUS_TEST_BLOCK_DEPTH -1 );

    // Transaction 1 exists in a block
    tso = db->find< transaction_status_object, by_trx_id >( tx1->get_transaction_id() );
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
    tsbo = db->find< transaction_status_block_object, by_block_num >( tso->block_num );
    BOOST_REQUIRE( tsbo != nullptr );
    BOOST_REQUIRE( tsbo->block_num == tso->block_num );
    BOOST_REQUIRE( tsbo->timestamp > fc::time_point_sec() );

    // Transaction 2 exists in a block
    tso = db->find< transaction_status_object, by_trx_id >( tx2->get_transaction_id() );
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
    tsbo = db->find< transaction_status_block_object, by_block_num >( tso->block_num );
    BOOST_REQUIRE( tsbo != nullptr );
    BOOST_REQUIRE( tsbo->block_num == tso->block_num );
    BOOST_REQUIRE( tsbo->timestamp > fc::time_point_sec() );
    auto tx2_block_num = tso->block_num;

    // Transaction 3 exists in a block
    tso = db->find< transaction_status_object, by_trx_id >( tx3->get_transaction_id() );
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
    tsbo = db->find< transaction_status_block_object, by_block_num >( tso->block_num );
    BOOST_REQUIRE( tsbo != nullptr );
    BOOST_REQUIRE( tsbo->block_num == tso->block_num );
    BOOST_REQUIRE( tsbo->timestamp > fc::time_point_sec() );
    auto tx3_block_num = tso->block_num;

    generate_blocks( HIVE_MAX_TIME_UNTIL_EXPIRATION / HIVE_BLOCK_INTERVAL );

    // Transaction 1 is no longer tracked
    tso = db->find< transaction_status_object, by_trx_id >( tx1->get_transaction_id() );
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
    tsbo = db->find< transaction_status_block_object, by_block_num >( tx1_block_num );
    BOOST_REQUIRE( tsbo == nullptr );

    // Transaction 2 exists in a block
    tso = db->find< transaction_status_object, by_trx_id >( tx2->get_transaction_id() );
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
    tsbo = db->find< transaction_status_block_object, by_block_num >( tso->block_num );
    BOOST_REQUIRE( tsbo != nullptr );
    BOOST_REQUIRE( tsbo->block_num == tso->block_num );
    BOOST_REQUIRE( tsbo->timestamp > fc::time_point_sec() );

    // Transaction 3 exists in a block
    tso = db->find< transaction_status_object, by_trx_id >( tx3->get_transaction_id() );
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
    tsbo = db->find< transaction_status_block_object, by_block_num >( tso->block_num );
    BOOST_REQUIRE( tsbo != nullptr );
    BOOST_REQUIRE( tsbo->block_num == tso->block_num );
    BOOST_REQUIRE( tsbo->timestamp > fc::time_point_sec() );

    generate_block();

    // Transaction 2 is no longer tracked
    tso = db->find< transaction_status_object, by_trx_id >( tx2->get_transaction_id() );
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
    tsbo = db->find< transaction_status_block_object, by_block_num >( tx2_block_num );
    BOOST_REQUIRE( tsbo == nullptr );

    // Transaction 3 is no longer tracked
    tso = db->find< transaction_status_object, by_trx_id >( tx3->get_transaction_id() );
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
    tsbo = db->find< transaction_status_block_object, by_block_num >( tx3_block_num );
    BOOST_REQUIRE( tsbo == nullptr );

    // At this point our index should be empty
    BOOST_REQUIRE( db->get_index< transaction_status_index >().indices().get< by_id >().empty() );
    BOOST_REQUIRE( db->get_index< transaction_status_index >().indices().get< by_trx_id >().empty() );
    BOOST_REQUIRE( db->get_index< transaction_status_index >().indices().get< by_block_num >().empty() );
    // Each transaction block is already removed from our index (see above). 
    // Also no older block should exist in our index.
    auto lower_bound = std::max< uint32_t >( { tx1_block_num, tx2_block_num, tx3_block_num} );
    const auto& bidx = db->get_index< transaction_status_block_index >().indices().get< by_block_num >();
    auto bitr = bidx.begin();
    BOOST_REQUIRE( bitr != bidx.end() );
    BOOST_REQUIRE( bitr->block_num > lower_bound );

    generate_block();

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
    auto lib_time = get_block_reader().fetch_block_by_number( 
      db->get_last_irreversible_block_num() )->get_block_header().timestamp;
    api_return = tx_status_api->api->find_transaction( { .transaction_id = transaction_id_type(), .expiration = lib_time } );
    BOOST_REQUIRE( api_return.status == expired_irreversible );
    BOOST_REQUIRE( api_return.block_num.valid() == false );
    BOOST_REQUIRE( api_return.rc_cost.valid() == false );

    // One second after our last irreversible block
    auto after_lib_time = lib_time + fc::seconds(1);
    api_return = tx_status_api->api->find_transaction( { .transaction_id = transaction_id_type(), after_lib_time } );
    BOOST_REQUIRE( api_return.status == expired_reversible );
    BOOST_REQUIRE( api_return.block_num.valid() == false );
    BOOST_REQUIRE( api_return.rc_cost.valid() == false );

    // One second before our block depth
    auto old_time = get_block_reader().fetch_block_by_number( 
      db->head_block_num() - TRANSACTION_STATUS_TEST_BLOCK_DEPTH + 1 )->get_block_header().timestamp - fc::seconds(1);
    api_return = tx_status_api->api->find_transaction( { .transaction_id = transaction_id_type(), old_time } );
    BOOST_REQUIRE( api_return.status == too_old );
    BOOST_REQUIRE( api_return.block_num.valid() == false );
    BOOST_REQUIRE( api_return.rc_cost.valid() == false );
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_SUITE_END()
#endif

