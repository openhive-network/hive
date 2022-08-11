#if defined IS_TEST_NET
#include <boost/test/unit_test.hpp>
#include <hive/chain/account_object.hpp>
#include <hive/protocol/hive_operations.hpp>
#include <hive/protocol/config.hpp>
#include <hive/plugins/transaction_status/transaction_status_plugin.hpp>
#include <hive/plugins/transaction_status/transaction_status_objects.hpp>
#include <hive/plugins/transaction_status_api/transaction_status_api_plugin.hpp>
#include <hive/plugins/transaction_status_api/transaction_status_api.hpp>

#include "../db_fixture/database_fixture.hpp"

using namespace hive::chain;
using namespace hive::protocol;

#define TRANSCATION_STATUS_TRACK_AFTER_BLOCK 1300
#define TRANSCATION_STATUS_TRACK_AFTER_BLOCK_STR BOOST_PP_STRINGIZE( TRANSCATION_STATUS_TRACK_AFTER_BLOCK )
#define TRANSACTION_STATUS_TEST_BLOCK_DEPTH 30
#define TRANSACTION_STATUS_TEST_BLOCK_DEPTH_STR BOOST_PP_STRINGIZE( TRANSACTION_STATUS_TEST_BLOCK_DEPTH )

BOOST_FIXTURE_TEST_SUITE( transaction_status, database_fixture );

BOOST_AUTO_TEST_CASE( transaction_status_test )
{
  using namespace hive::plugins::transaction_status;

  try
  {
    hive::plugins::transaction_status_api::transaction_status_api_plugin* tx_status_api = nullptr;
    hive::plugins::transaction_status::transaction_status_plugin* tx_status = nullptr;

    auto _data_dir = common_init( [&]( appbase::application& app, int argc, char** argv )
    {
      app.register_plugin< transaction_status_plugin >();
      app.register_plugin< hive::plugins::transaction_status_api::transaction_status_api_plugin >();
      db_plugin = &app.register_plugin< hive::plugins::debug_node::debug_node_plugin >();

      // We create an argc/argv so that the transaction_status plugin can be initialized with a reasonable block depth
      int test_argc = 5;
      const char* test_argv[] = {
        boost::unit_test::framework::master_test_suite().argv[ 0 ],
        "--transaction-status-block-depth",
        TRANSACTION_STATUS_TEST_BLOCK_DEPTH_STR,
        "--transaction-status-track-after-block",
        TRANSCATION_STATUS_TRACK_AFTER_BLOCK_STR
      };

      db_plugin->logging = false;
      app.initialize<
        hive::plugins::transaction_status_api::transaction_status_api_plugin,
        hive::plugins::debug_node::debug_node_plugin >( test_argc, ( char** ) test_argv );

      db = &app.get_plugin< hive::plugins::chain::chain_plugin >().db();
      BOOST_REQUIRE( db );

      tx_status_api = &app.get_plugin< hive::plugins::transaction_status_api::transaction_status_api_plugin >();
      BOOST_REQUIRE( tx_status_api );

      tx_status = &app.get_plugin< hive::plugins::transaction_status::transaction_status_plugin >();
      BOOST_REQUIRE( tx_status );
    } );

    init_account_pub_key = init_account_priv_key.get_public_key();

    open_database( _data_dir );

    BOOST_REQUIRE( db->get_index< transaction_status_index >().indices().get< by_id >().empty() );
    BOOST_REQUIRE( db->get_index< transaction_status_index >().indices().get< by_trx_id >().empty() );
    BOOST_REQUIRE( db->get_index< transaction_status_index >().indices().get< by_block_num >().empty() );

    BOOST_REQUIRE( tx_status->state_is_valid() );

    generate_block();
    db->set_hardfork( HIVE_NUM_HARDFORKS );
    generate_block();

    vest( "initminer", 10000 );

    // Fill up the rest of the required miners
    for( int i = HIVE_NUM_INIT_MINERS; i < HIVE_MAX_WITNESSES; i++ )
    {
      account_create( HIVE_INIT_MINER_NAME + fc::to_string( i ), init_account_pub_key );
      fund( HIVE_INIT_MINER_NAME + fc::to_string( i ), HIVE_MIN_PRODUCER_REWARD.amount.value );
      witness_create( HIVE_INIT_MINER_NAME + fc::to_string( i ), init_account_priv_key, "foo.bar", init_account_pub_key, HIVE_MIN_PRODUCER_REWARD.amount );
    }

    validate_database();

    ACTORS( (alice)(bob) );
    generate_block();

    fund( "alice", ASSET( "1000.000 TESTS" ) );
    fund( "alice", ASSET( "1000.000 TBD" ) );
    fund( "bob", ASSET( "1000.000 TESTS" ) );

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

    // Tracking should not be enabled until we have reached TRANSCATION_STATUS_TRACK_AFTER_BLOCK - ( HIVE_MAX_TIME_UNTIL_EXPIRATION / HIVE_BLOCK_INTERVAL ) blocks
    BOOST_REQUIRE( db->get_index< transaction_status_index >().indices().get< by_id >().empty() );
    BOOST_REQUIRE( db->get_index< transaction_status_index >().indices().get< by_trx_id >().empty() );
    BOOST_REQUIRE( db->get_index< transaction_status_index >().indices().get< by_block_num >().empty() );

    // Transaction 0 should not be tracked
    auto tso = db->find< transaction_status_object, by_trx_id >( tx0->get_transaction_id() );
    BOOST_REQUIRE( tso == nullptr );

    auto api_return = tx_status_api->api->find_transaction( { .transaction_id = tx0->get_transaction_id() } );
    BOOST_REQUIRE( api_return.status == unknown );
    BOOST_REQUIRE( api_return.block_num.valid() == false );

    api_return = tx_status_api->api->find_transaction( { .transaction_id = tx0->get_transaction_id(), _tx0_expiration  } );
    BOOST_REQUIRE( api_return.status == unknown );
    BOOST_REQUIRE( api_return.block_num.valid() == false );

    generate_blocks( TRANSCATION_STATUS_TRACK_AFTER_BLOCK - db->head_block_num() );

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

    api_return = tx_status_api->api->find_transaction( { .transaction_id = tx1->get_transaction_id() } );
    BOOST_REQUIRE( api_return.status == within_mempool );
    BOOST_REQUIRE( api_return.block_num.valid() == false );

    api_return = tx_status_api->api->find_transaction( { .transaction_id = tx1->get_transaction_id(), _tx1_expiration } );
    BOOST_REQUIRE( api_return.status == within_mempool );
    BOOST_REQUIRE( api_return.block_num.valid() == false );

    generate_block();

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

    api_return = tx_status_api->api->find_transaction( { .transaction_id = tx1->get_transaction_id() } );
    BOOST_REQUIRE( api_return.status == within_reversible_block );
    BOOST_REQUIRE( api_return.block_num.valid() );
    BOOST_REQUIRE( api_return.block_num == db->head_block_num() );

    api_return = tx_status_api->api->find_transaction( { .transaction_id = tx1->get_transaction_id(), _tx1_expiration } );
    BOOST_REQUIRE( api_return.status == within_reversible_block );
    BOOST_REQUIRE( api_return.block_num.valid() );
    BOOST_REQUIRE( api_return.block_num == db->head_block_num() );

    // Transaction 2 exists in a mem pool
    tso = db->find< transaction_status_object, by_trx_id >( tx2->get_transaction_id() );
    BOOST_REQUIRE( tso != nullptr );
    BOOST_REQUIRE( tso->block_num == 0 );

    api_return = tx_status_api->api->find_transaction( { .transaction_id = tx2->get_transaction_id() } );
    BOOST_REQUIRE( api_return.status == within_mempool );
    BOOST_REQUIRE( api_return.block_num.valid() == false );

    api_return = tx_status_api->api->find_transaction( { .transaction_id = tx2->get_transaction_id(), _tx2_expiration } );
    BOOST_REQUIRE( api_return.status == within_mempool );
    BOOST_REQUIRE( api_return.block_num.valid() == false );

    // Transaction 3 exists in a mem pool
    tso = db->find< transaction_status_object, by_trx_id >( tx3->get_transaction_id() );
    BOOST_REQUIRE( tso != nullptr );
    BOOST_REQUIRE( tso->block_num == 0 );

    api_return = tx_status_api->api->find_transaction( { .transaction_id = tx3->get_transaction_id() } );
    BOOST_REQUIRE( api_return.status == within_mempool );
    BOOST_REQUIRE( api_return.block_num.valid() == false );

    api_return = tx_status_api->api->find_transaction( { .transaction_id = tx3->get_transaction_id(), _tx3_expiration } );
    BOOST_REQUIRE( api_return.status == within_mempool );
    BOOST_REQUIRE( api_return.block_num.valid() == false );

    BOOST_REQUIRE( tx_status->state_is_valid() );

    generate_blocks( TRANSACTION_STATUS_TEST_BLOCK_DEPTH );

    // Transaction 1 exists in a block
    tso = db->find< transaction_status_object, by_trx_id >( tx1->get_transaction_id() );
    BOOST_REQUIRE( tso != nullptr );
    BOOST_REQUIRE( tso->block_num > 0 );

    api_return = tx_status_api->api->find_transaction( { .transaction_id = tx1->get_transaction_id() } );
    BOOST_REQUIRE( api_return.status == within_irreversible_block );
    BOOST_REQUIRE( *api_return.block_num > 0 );

    api_return = tx_status_api->api->find_transaction( { .transaction_id = tx1->get_transaction_id(), _tx1_expiration } );
    BOOST_REQUIRE( api_return.status == within_irreversible_block );
    BOOST_REQUIRE( *api_return.block_num > 0 );

    // Transaction 2 exists in a block
    tso = db->find< transaction_status_object, by_trx_id >( tx2->get_transaction_id() );
    BOOST_REQUIRE( tso != nullptr );
    BOOST_REQUIRE( tso->block_num > 0 );

    api_return = tx_status_api->api->find_transaction( { .transaction_id = tx2->get_transaction_id() } );
    BOOST_REQUIRE( api_return.status == within_irreversible_block );
    BOOST_REQUIRE( api_return.block_num.valid() );
    BOOST_REQUIRE( *api_return.block_num > 0 );

    api_return = tx_status_api->api->find_transaction( { .transaction_id = tx2->get_transaction_id(), _tx2_expiration } );
    BOOST_REQUIRE( api_return.status == within_irreversible_block );
    BOOST_REQUIRE( api_return.block_num.valid() );
    BOOST_REQUIRE( *api_return.block_num > 0 );

    // Transaction 3 exists in a block
    tso = db->find< transaction_status_object, by_trx_id >( tx3->get_transaction_id() );
    BOOST_REQUIRE( tso != nullptr );
    BOOST_REQUIRE( tso->block_num > 0 );

    api_return = tx_status_api->api->find_transaction( { .transaction_id = tx3->get_transaction_id() } );
    BOOST_REQUIRE( api_return.status == within_irreversible_block );
    BOOST_REQUIRE( api_return.block_num.valid() );
    BOOST_REQUIRE( *api_return.block_num > 0 );

    api_return = tx_status_api->api->find_transaction( { .transaction_id = tx3->get_transaction_id(), _tx3_expiration } );
    BOOST_REQUIRE( api_return.status == within_irreversible_block );
    BOOST_REQUIRE( api_return.block_num.valid() );
    BOOST_REQUIRE( *api_return.block_num > 0 );

    BOOST_REQUIRE( tx_status->state_is_valid() );

    generate_blocks( HIVE_MAX_TIME_UNTIL_EXPIRATION / HIVE_BLOCK_INTERVAL );

    // Transaction 1 is no longer tracked
    tso = db->find< transaction_status_object, by_trx_id >( tx1->get_transaction_id() );
    BOOST_REQUIRE( tso == nullptr );

    api_return = tx_status_api->api->find_transaction( { .transaction_id = tx1->get_transaction_id() } );
    BOOST_REQUIRE( api_return.status == unknown );
    BOOST_REQUIRE( api_return.block_num.valid() == false );

    api_return = tx_status_api->api->find_transaction( { .transaction_id = tx1->get_transaction_id(), _tx1_expiration } );
    BOOST_REQUIRE( api_return.status == too_old );
    BOOST_REQUIRE( api_return.block_num.valid() == false );

    // Transaction 2 exists in a block
    tso = db->find< transaction_status_object, by_trx_id >( tx2->get_transaction_id() );
    BOOST_REQUIRE( tso != nullptr );
    BOOST_REQUIRE( tso->block_num > 0 );

    api_return = tx_status_api->api->find_transaction( { .transaction_id = tx2->get_transaction_id() } );
    BOOST_REQUIRE( api_return.status == within_irreversible_block );
    BOOST_REQUIRE( api_return.block_num.valid() );
    BOOST_REQUIRE( *api_return.block_num > 0 );

    api_return = tx_status_api->api->find_transaction( { .transaction_id = tx2->get_transaction_id(), _tx2_expiration } );
    BOOST_REQUIRE( api_return.status == within_irreversible_block );
    BOOST_REQUIRE( api_return.block_num.valid() );
    BOOST_REQUIRE( *api_return.block_num > 0 );

    // Transaction 3 exists in a block
    tso = db->find< transaction_status_object, by_trx_id >( tx3->get_transaction_id() );
    BOOST_REQUIRE( tso != nullptr );
    BOOST_REQUIRE( tso->block_num > 0 );

    api_return = tx_status_api->api->find_transaction( { .transaction_id = tx3->get_transaction_id() } );
    BOOST_REQUIRE( api_return.status == within_irreversible_block );
    BOOST_REQUIRE( api_return.block_num.valid() );
    BOOST_REQUIRE( *api_return.block_num > 0 );

    api_return = tx_status_api->api->find_transaction( { .transaction_id = tx3->get_transaction_id(), _tx3_expiration } );
    BOOST_REQUIRE( api_return.status == within_irreversible_block );
    BOOST_REQUIRE( api_return.block_num.valid() );
    BOOST_REQUIRE( *api_return.block_num > 0 );

    BOOST_REQUIRE( tx_status->state_is_valid() );

    generate_block();

    // Transaction 2 is no longer tracked
    tso = db->find< transaction_status_object, by_trx_id >( tx2->get_transaction_id() );
    BOOST_REQUIRE( tso == nullptr );

    api_return = tx_status_api->api->find_transaction( { .transaction_id = tx2->get_transaction_id() } );
    BOOST_REQUIRE( api_return.status == unknown );
    BOOST_REQUIRE( api_return.block_num.valid() == false );

    api_return = tx_status_api->api->find_transaction( { .transaction_id = tx2->get_transaction_id(), _tx2_expiration } );
    BOOST_REQUIRE( api_return.status == too_old );
    BOOST_REQUIRE( api_return.block_num.valid() == false );

    // Transaction 3 is no longer tracked
    tso = db->find< transaction_status_object, by_trx_id >( tx3->get_transaction_id() );
    BOOST_REQUIRE( tso == nullptr );

    api_return = tx_status_api->api->find_transaction( { .transaction_id = tx3->get_transaction_id() } );
    BOOST_REQUIRE( api_return.status == unknown );
    BOOST_REQUIRE( api_return.block_num.valid() == false );

    api_return = tx_status_api->api->find_transaction( { .transaction_id = tx3->get_transaction_id(), _tx3_expiration } );
    BOOST_REQUIRE( api_return.status == too_old );
    BOOST_REQUIRE( api_return.block_num.valid() == false );

    // At this point our index should be empty
    BOOST_REQUIRE( db->get_index< transaction_status_index >().indices().get< by_id >().empty() );
    BOOST_REQUIRE( db->get_index< transaction_status_index >().indices().get< by_trx_id >().empty() );
    BOOST_REQUIRE( db->get_index< transaction_status_index >().indices().get< by_block_num >().empty() );

    BOOST_REQUIRE( tx_status->state_is_valid() );

    generate_block();

    api_return = tx_status_api->api->find_transaction( { .transaction_id = tx1->get_transaction_id() } );
    BOOST_REQUIRE( api_return.status == unknown );
    BOOST_REQUIRE( api_return.block_num.valid() == false );

    api_return = tx_status_api->api->find_transaction( { .transaction_id = tx1->get_transaction_id(), _tx1_expiration } );
    BOOST_REQUIRE( api_return.status == too_old );
    BOOST_REQUIRE( api_return.block_num.valid() == false );

    api_return = tx_status_api->api->find_transaction( { .transaction_id = tx2->get_transaction_id() } );
    BOOST_REQUIRE( api_return.status == unknown );
    BOOST_REQUIRE( api_return.block_num.valid() == false );

    api_return = tx_status_api->api->find_transaction( { .transaction_id = tx2->get_transaction_id(), _tx2_expiration } );
    BOOST_REQUIRE( api_return.status == too_old );
    BOOST_REQUIRE( api_return.block_num.valid() == false );

    /**
      * Testing transactions that do not exist, but expirations are provided
      */
    BOOST_TEST_MESSAGE( " -- transaction status expiration test" );

    // The time of our last irreversible block
    auto lib_time = db->fetch_block_by_number( db->get_last_irreversible_block_num() )->get_block_header().timestamp;
    api_return = tx_status_api->api->find_transaction( { .transaction_id = transaction_id_type(), .expiration = lib_time } );
    BOOST_REQUIRE( api_return.status == expired_irreversible );
    BOOST_REQUIRE( api_return.block_num.valid() == false );

    // One second after our last irreversible block
    auto after_lib_time = lib_time + fc::seconds(1);
    api_return = tx_status_api->api->find_transaction( { .transaction_id = transaction_id_type(), after_lib_time } );
    BOOST_REQUIRE( api_return.status == expired_reversible );
    BOOST_REQUIRE( api_return.block_num.valid() == false );

    // One second before our block depth
    auto old_time = db->fetch_block_by_number( db->head_block_num() - TRANSACTION_STATUS_TEST_BLOCK_DEPTH + 1 )->get_block_header().timestamp - fc::seconds(1);
    api_return = tx_status_api->api->find_transaction( { .transaction_id = transaction_id_type(), old_time } );
    BOOST_REQUIRE( api_return.status == too_old );
    BOOST_REQUIRE( api_return.block_num.valid() == false );

    BOOST_REQUIRE( tx_status->state_is_valid() );

    /**
      * Testing transaction status plugin state
      */
    BOOST_TEST_MESSAGE( " -- transaction status state test" );

    // Create transaction 4
    signed_transaction _tx4;
    transfer_operation op4;
    auto _tx4_expiration = db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION;

    op4.from = "alice";
    op4.to = "bob";
    op4.amount = ASSET( "5.000 TESTS" );

    _tx4.operations.push_back( op4 );
    _tx4.set_expiration( _tx4_expiration );
    auto tx4 = push_transaction( _tx4, alice_private_key, 0, hive::protocol::pack_type::legacy );

    generate_block();

    BOOST_REQUIRE( tx_status->state_is_valid() );

    const auto& tx_status_obj = db->get< transaction_status_object, by_trx_id >( tx4->get_transaction_id() );
    db->remove( tx_status_obj );

    // Upper bound of transaction status state should cause state to be invalid
    BOOST_REQUIRE( tx_status->state_is_valid() == false );

    tx_status->rebuild_state();

    BOOST_REQUIRE( tx_status->state_is_valid() );

    // Create transaction 5
    signed_transaction _tx5;
    transfer_operation op5;
    auto _tx5_expiration = db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION;

    op5.from = "alice";
    op5.to = "bob";
    op5.amount = ASSET( "5.000 TESTS" );

    _tx5.operations.push_back( op5 );
    _tx5.set_expiration( _tx5_expiration );
    auto tx5 = push_transaction( _tx5, alice_private_key, 0, hive::protocol::pack_type::legacy );

    generate_blocks( TRANSACTION_STATUS_TEST_BLOCK_DEPTH + ( HIVE_MAX_TIME_UNTIL_EXPIRATION / HIVE_BLOCK_INTERVAL ) - 1 );

    const auto& tx_status_obj2 = db->get< transaction_status_object, by_trx_id >( tx5->get_transaction_id() );
    db->remove( tx_status_obj2 );

    // Lower bound of transaction status state should cause state to be invalid
    BOOST_REQUIRE( tx_status->state_is_valid() == false );

    tx_status->rebuild_state();

    BOOST_REQUIRE( tx_status->state_is_valid() );
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_SUITE_END()
#endif

