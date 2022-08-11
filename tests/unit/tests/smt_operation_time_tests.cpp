
#if defined IS_TEST_NET && defined HIVE_ENABLE_SMT

#include <boost/test/unit_test.hpp>

#include <hive/chain/hive_fwd.hpp>

#include <hive/protocol/exceptions.hpp>
#include <hive/protocol/hardfork.hpp>

#include <hive/chain/block_summary_object.hpp>
#include <hive/chain/database.hpp>
#include <hive/chain/hive_objects.hpp>

#include <hive/chain/util/reward.hpp>

#include <hive/plugins/debug_node/debug_node_plugin.hpp>

#include <fc/crypto/digest.hpp>

#include "../db_fixture/database_fixture.hpp"

#include <cmath>

using namespace hive;
using namespace hive::chain;
using namespace hive::protocol;

BOOST_FIXTURE_TEST_SUITE( smt_operation_time_tests, smt_database_fixture )

BOOST_AUTO_TEST_CASE( smt_liquidity_rewards )
{
  using std::abs;

  try
  {
    db->liquidity_rewards_enabled = false;

    ACTORS( (alice)(bob)(sam)(dave)(smtcreator) )

    //Create SMT and give some SMT to creators.
    signed_transaction tx;
    asset_symbol_type any_smt_symbol = create_smt( "smtcreator", smtcreator_private_key, 3 );

    generate_block();
    vest( HIVE_INIT_MINER_NAME, "alice", ASSET( "10.000 TESTS" ) );
    vest( HIVE_INIT_MINER_NAME, "bob", ASSET( "10.000 TESTS" ) );
    vest( HIVE_INIT_MINER_NAME, "sam", ASSET( "10.000 TESTS" ) );
    vest( HIVE_INIT_MINER_NAME, "dave", ASSET( "10.000 TESTS" ) );

    tx.operations.clear();

    BOOST_TEST_MESSAGE( "Rewarding Bob with TESTS" );

    auto exchange_rate = price( ASSET( "1.250 TESTS" ), asset( 1000, any_smt_symbol ) );

    const account_object& alice_account = db->get_account( "alice" );
    FUND( "alice", asset( 25522, any_smt_symbol ) );
    asset alice_smt = db->get_balance( alice_account, any_smt_symbol );

    FUND( "alice", alice_smt.amount );
    FUND( "bob", alice_smt.amount );
    FUND( "sam", alice_smt.amount );
    FUND( "dave", alice_smt.amount );

    int64_t alice_smt_volume = 0;
    int64_t alice_hive_volume = 0;
    time_point_sec alice_reward_last_update = fc::time_point_sec::min();
    int64_t bob_smt_volume = 0;
    int64_t bob_hive_volume = 0;
    time_point_sec bob_reward_last_update = fc::time_point_sec::min();
    int64_t sam_smt_volume = 0;
    int64_t sam_hive_volume = 0;
    time_point_sec sam_reward_last_update = fc::time_point_sec::min();
    int64_t dave_smt_volume = 0;
    int64_t dave_hive_volume = 0;
    time_point_sec dave_reward_last_update = fc::time_point_sec::min();

    BOOST_TEST_MESSAGE( "Creating Limit Order for HIVE that will stay on the books for 30 minutes exactly." );

    limit_order_create_operation op;
    op.owner = "alice";
    op.amount_to_sell = asset( alice_smt.amount.value / 20, any_smt_symbol ) ;
    op.min_to_receive = op.amount_to_sell * exchange_rate;
    op.orderid = 1;
    op.expiration = db->head_block_time() + fc::seconds( HIVE_MAX_LIMIT_ORDER_EXPIRATION );

    tx.operations.clear();
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    tx.operations.push_back( op );
    push_transaction( tx, alice_private_key );

    BOOST_TEST_MESSAGE( "Waiting 10 minutes" );

    generate_blocks( db->head_block_time() + HIVE_MIN_LIQUIDITY_REWARD_PERIOD_SEC_HF10, true );

    BOOST_TEST_MESSAGE( "Creating Limit Order for SMT that will be filled immediately." );

    op.owner = "bob";
    op.min_to_receive = op.amount_to_sell;
    op.amount_to_sell = op.min_to_receive * exchange_rate;
    op.fill_or_kill = false;
    op.orderid = 2;

    tx.operations.clear();
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    tx.operations.push_back( op );
    push_transaction( tx, bob_private_key );

    alice_hive_volume += ( asset( alice_smt.amount / 20, any_smt_symbol ) * exchange_rate ).amount.value;
    alice_reward_last_update = db->head_block_time();
    bob_hive_volume -= ( asset( alice_smt.amount / 20, any_smt_symbol ) * exchange_rate ).amount.value;
    bob_reward_last_update = db->head_block_time();

    auto ops = get_last_operations( 1 );
    const auto& liquidity_idx = db->get_index< liquidity_reward_balance_index >().indices().get< by_owner >();
    const auto& limit_order_idx = db->get_index< limit_order_index >().indices().get< by_account >();

    auto reward = liquidity_idx.find( get_account_id( "alice" ) );
    BOOST_REQUIRE( reward == liquidity_idx.end() );
    /*BOOST_REQUIRE( reward->owner == get_account_id( "alice" ) );
    BOOST_REQUIRE( reward->get_hbd_volume() == alice_smt_volume );
    BOOST_REQUIRE( reward->get_hive_volume() == alice_hive_volume );
    BOOST_CHECK( reward->last_update == alice_reward_last_update );*/

    reward = liquidity_idx.find( get_account_id( "bob" ) );
    BOOST_REQUIRE( reward == liquidity_idx.end() );
    /*BOOST_REQUIRE( reward->owner == get_account_id( "bob" ) );
    BOOST_REQUIRE( reward->get_hbd_volume() == bob_smt_volume );
    BOOST_REQUIRE( reward->get_hive_volume() == bob_hive_volume );
    BOOST_CHECK( reward->last_update == bob_reward_last_update );*/

    auto fill_order_op = ops[0].get< fill_order_operation >();

    BOOST_REQUIRE( fill_order_op.open_owner == "alice" );
    BOOST_REQUIRE( fill_order_op.open_orderid == 1 );
    BOOST_REQUIRE( fill_order_op.open_pays.amount.value == asset( alice_smt.amount.value / 20, any_smt_symbol ).amount.value );
    BOOST_REQUIRE( fill_order_op.current_owner == "bob" );
    BOOST_REQUIRE( fill_order_op.current_orderid == 2 );
    BOOST_REQUIRE( fill_order_op.current_pays.amount.value == ( asset( alice_smt.amount.value / 20, any_smt_symbol ) * exchange_rate ).amount.value );

    BOOST_CHECK( limit_order_idx.find( boost::make_tuple( "alice", 1 ) ) == limit_order_idx.end() );
    BOOST_CHECK( limit_order_idx.find( boost::make_tuple( "bob", 2 ) ) == limit_order_idx.end() );

    BOOST_TEST_MESSAGE( "Creating Limit Order for SMT that will stay on the books for 60 minutes." );

    op.owner = "sam";
    op.amount_to_sell = asset( ( alice_smt.amount.value / 20 ), HIVE_SYMBOL );
    op.min_to_receive = asset( ( alice_smt.amount.value / 20 ), any_smt_symbol );
    op.orderid = 3;

    tx.operations.clear();
    tx.operations.push_back( op );
    push_transaction( tx, sam_private_key );

    BOOST_TEST_MESSAGE( "Waiting 10 minutes" );

    generate_blocks( db->head_block_time() + HIVE_MIN_LIQUIDITY_REWARD_PERIOD_SEC_HF10, true );

    BOOST_TEST_MESSAGE( "Creating Limit Order for SMT that will stay on the books for 30 minutes." );

    op.owner = "bob";
    op.orderid = 4;
    op.amount_to_sell = asset( ( alice_smt.amount.value / 10 ) * 3 - alice_smt.amount.value / 20, HIVE_SYMBOL );
    op.min_to_receive = asset( ( alice_smt.amount.value / 10 ) * 3 - alice_smt.amount.value / 20, any_smt_symbol );

    tx.operations.clear();
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    tx.operations.push_back( op );
    push_transaction( tx, bob_private_key );

    BOOST_TEST_MESSAGE( "Waiting 30 minutes" );

    generate_blocks( db->head_block_time() + HIVE_MIN_LIQUIDITY_REWARD_PERIOD_SEC_HF10, true );

    BOOST_TEST_MESSAGE( "Filling both limit orders." );

    op.owner = "alice";
    op.orderid = 5;
    op.amount_to_sell = asset( ( alice_smt.amount.value / 10 ) * 3, any_smt_symbol );
    op.min_to_receive = asset( ( alice_smt.amount.value / 10 ) * 3, HIVE_SYMBOL );

    tx.operations.clear();
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    tx.operations.push_back( op );
    push_transaction( tx, alice_private_key );

    alice_smt_volume -= ( alice_smt.amount.value / 10 ) * 3;
    alice_reward_last_update = db->head_block_time();
    sam_smt_volume += alice_smt.amount.value / 20;
    sam_reward_last_update = db->head_block_time();
    bob_smt_volume += ( alice_smt.amount.value / 10 ) * 3 - ( alice_smt.amount.value / 20 );
    bob_reward_last_update = db->head_block_time();
    ops = get_last_operations( 2 );

    fill_order_op = ops[0].get< fill_order_operation >();
    BOOST_REQUIRE( fill_order_op.open_owner == "bob" );
    BOOST_REQUIRE( fill_order_op.open_orderid == 4 );
    BOOST_REQUIRE( fill_order_op.open_pays.amount.value == asset( ( alice_smt.amount.value / 10 ) * 3 - alice_smt.amount.value / 20, HIVE_SYMBOL ).amount.value );
    BOOST_REQUIRE( fill_order_op.current_owner == "alice" );
    BOOST_REQUIRE( fill_order_op.current_orderid == 5 );
    BOOST_REQUIRE( fill_order_op.current_pays.amount.value == asset( ( alice_smt.amount.value / 10 ) * 3 - alice_smt.amount.value / 20, any_smt_symbol ).amount.value );

    fill_order_op = ops[1].get< fill_order_operation >();
    BOOST_REQUIRE( fill_order_op.open_owner == "sam" );
    BOOST_REQUIRE( fill_order_op.open_orderid == 3 );
    BOOST_REQUIRE( fill_order_op.open_pays.amount.value == asset( alice_smt.amount.value / 20, HIVE_SYMBOL ).amount.value );
    BOOST_REQUIRE( fill_order_op.current_owner == "alice" );
    BOOST_REQUIRE( fill_order_op.current_orderid == 5 );
    BOOST_REQUIRE( fill_order_op.current_pays.amount.value == asset( alice_smt.amount.value / 20, any_smt_symbol ).amount.value );

    reward = liquidity_idx.find( get_account_id( "alice" ) );
    BOOST_REQUIRE( reward == liquidity_idx.end() );
    /*BOOST_REQUIRE( reward->owner == get_account_id( "alice" ) );
    BOOST_REQUIRE( reward->get_hbd_volume() == alice_smt_volume );
    BOOST_REQUIRE( reward->get_hive_volume() == alice_hive_volume );
    BOOST_CHECK( reward->last_update == alice_reward_last_update );*/

    reward = liquidity_idx.find( get_account_id( "bob" ) );
    BOOST_REQUIRE( reward == liquidity_idx.end() );
    /*BOOST_REQUIRE( reward->owner == get_account_id( "bob" ) );
    BOOST_REQUIRE( reward->get_hbd_volume() == bob_smt_volume );
    BOOST_REQUIRE( reward->get_hive_volume() == bob_hive_volume );
    BOOST_CHECK( reward->last_update == bob_reward_last_update );*/

    reward = liquidity_idx.find( get_account_id( "sam" ) );
    BOOST_REQUIRE( reward == liquidity_idx.end() );
    /*BOOST_REQUIRE( reward->owner == get_account_id( "sam" ) );
    BOOST_REQUIRE( reward->get_hbd_volume() == sam_smt_volume );
    BOOST_REQUIRE( reward->get_hive_volume() == sam_hive_volume );
    BOOST_CHECK( reward->last_update == sam_reward_last_update );*/

    BOOST_TEST_MESSAGE( "Testing a partial fill before minimum time and full fill after minimum time" );

    op.orderid = 6;
    op.amount_to_sell = asset( alice_smt.amount.value / 20 * 2, any_smt_symbol );
    op.min_to_receive = asset( alice_smt.amount.value / 20 * 2, HIVE_SYMBOL );

    tx.operations.clear();
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    tx.operations.push_back( op );
    push_transaction( tx, alice_private_key );

    generate_blocks( db->head_block_time() + fc::seconds( HIVE_MIN_LIQUIDITY_REWARD_PERIOD_SEC_HF10.to_seconds() / 2 ), true );

    op.owner = "bob";
    op.orderid = 7;
    op.amount_to_sell = asset( alice_smt.amount.value / 20, HIVE_SYMBOL );
    op.min_to_receive = asset( alice_smt.amount.value / 20, any_smt_symbol );

    tx.operations.clear();
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    tx.operations.push_back( op );
    push_transaction( tx, bob_private_key );

    generate_blocks( db->head_block_time() + fc::seconds( HIVE_MIN_LIQUIDITY_REWARD_PERIOD_SEC_HF10.to_seconds() / 2 ), true );

    ops = get_last_operations( 4 );
    fill_order_op = ops[3].get< fill_order_operation >();

    BOOST_REQUIRE( fill_order_op.open_owner == "alice" );
    BOOST_REQUIRE( fill_order_op.open_orderid == 6 );
    BOOST_REQUIRE( fill_order_op.open_pays.amount.value == asset( alice_smt.amount.value / 20, any_smt_symbol ).amount.value );
    BOOST_REQUIRE( fill_order_op.current_owner == "bob" );
    BOOST_REQUIRE( fill_order_op.current_orderid == 7 );
    BOOST_REQUIRE( fill_order_op.current_pays.amount.value == asset( alice_smt.amount.value / 20, HIVE_SYMBOL ).amount.value );

    reward = liquidity_idx.find( get_account_id( "alice" ) );
    BOOST_REQUIRE( reward == liquidity_idx.end() );
    /*BOOST_REQUIRE( reward->owner == get_account_id( "alice" ) );
    BOOST_REQUIRE( reward->get_hbd_volume() == alice_smt_volume );
    BOOST_REQUIRE( reward->get_hive_volume() == alice_hive_volume );
    BOOST_CHECK( reward->last_update == alice_reward_last_update );*/

    reward = liquidity_idx.find( get_account_id( "bob" ) );
    BOOST_REQUIRE( reward == liquidity_idx.end() );
    /*BOOST_REQUIRE( reward->owner == get_account_id( "bob" ) );
    BOOST_REQUIRE( reward->get_hbd_volume() == bob_smt_volume );
    BOOST_REQUIRE( reward->get_hive_volume() == bob_hive_volume );
    BOOST_CHECK( reward->last_update == bob_reward_last_update );*/

    reward = liquidity_idx.find( get_account_id( "sam" ) );
    BOOST_REQUIRE( reward == liquidity_idx.end() );
    /*BOOST_REQUIRE( reward->owner == get_account_id( "sam" ) );
    BOOST_REQUIRE( reward->get_hbd_volume() == sam_smt_volume );
    BOOST_REQUIRE( reward->get_hive_volume() == sam_hive_volume );
    BOOST_CHECK( reward->last_update == sam_reward_last_update );*/

    generate_blocks( db->head_block_time() + HIVE_MIN_LIQUIDITY_REWARD_PERIOD_SEC_HF10, true );

    op.owner = "sam";
    op.orderid = 8;

    tx.operations.clear();
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    tx.operations.push_back( op );
    push_transaction( tx, sam_private_key );

    alice_hive_volume += alice_smt.amount.value / 20;
    alice_reward_last_update = db->head_block_time();
    sam_hive_volume -= alice_smt.amount.value / 20;
    sam_reward_last_update = db->head_block_time();

    ops = get_last_operations( 1 );
    fill_order_op = ops[0].get< fill_order_operation >();

    BOOST_REQUIRE( fill_order_op.open_owner == "alice" );
    BOOST_REQUIRE( fill_order_op.open_orderid == 6 );
    BOOST_REQUIRE( fill_order_op.open_pays.amount.value == asset( alice_smt.amount.value / 20, any_smt_symbol ).amount.value );
    BOOST_REQUIRE( fill_order_op.current_owner == "sam" );
    BOOST_REQUIRE( fill_order_op.current_orderid == 8 );
    BOOST_REQUIRE( fill_order_op.current_pays.amount.value == asset( alice_smt.amount.value / 20, HIVE_SYMBOL ).amount.value );

    reward = liquidity_idx.find( get_account_id( "alice" ) );
    BOOST_REQUIRE( reward == liquidity_idx.end() );
    /*BOOST_REQUIRE( reward->owner == get_account_id( "alice" ) );
    BOOST_REQUIRE( reward->get_hbd_volume() == alice_smt_volume );
    BOOST_REQUIRE( reward->get_hive_volume() == alice_hive_volume );
    BOOST_CHECK( reward->last_update == alice_reward_last_update );*/

    reward = liquidity_idx.find( get_account_id( "bob" ) );
    BOOST_REQUIRE( reward == liquidity_idx.end() );
    /*BOOST_REQUIRE( reward->owner == get_account_id( "bob" ) );
    BOOST_REQUIRE( reward->get_hbd_volume() == bob_smt_volume );
    BOOST_REQUIRE( reward->get_hive_volume() == bob_hive_volume );
    BOOST_CHECK( reward->last_update == bob_reward_last_update );*/

    reward = liquidity_idx.find( get_account_id( "sam" ) );
    BOOST_REQUIRE( reward == liquidity_idx.end() );
    /*BOOST_REQUIRE( reward->owner == get_account_id( "sam" ) );
    BOOST_REQUIRE( reward->get_hbd_volume() == sam_smt_volume );
    BOOST_REQUIRE( reward->get_hive_volume() == sam_hive_volume );
    BOOST_CHECK( reward->last_update == sam_reward_last_update );*/

    BOOST_TEST_MESSAGE( "Trading to give Alice and Bob positive volumes to receive rewards" );

    transfer_operation transfer;
    transfer.to = "dave";
    transfer.from = "alice";
    transfer.amount = asset( alice_smt.amount / 2, any_smt_symbol );

    tx.operations.clear();
    tx.operations.push_back( transfer );
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    push_transaction( tx, alice_private_key );

    op.owner = "alice";
    op.amount_to_sell = asset( 8 * ( alice_smt.amount.value / 20 ), HIVE_SYMBOL );
    op.min_to_receive = asset( op.amount_to_sell.amount, any_smt_symbol );
    op.orderid = 9;
    tx.operations.clear();
    tx.operations.push_back( op );
    push_transaction( tx, alice_private_key );

    generate_blocks( db->head_block_time() + HIVE_MIN_LIQUIDITY_REWARD_PERIOD_SEC_HF10, true );

    op.owner = "dave";
    op.amount_to_sell = asset( 7 * ( alice_smt.amount.value / 20 ), any_smt_symbol );;
    op.min_to_receive = asset( op.amount_to_sell.amount, HIVE_SYMBOL );
    op.orderid = 10;
    tx.operations.clear();
    tx.operations.push_back( op );
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    push_transaction( tx, dave_private_key );

    alice_smt_volume += op.amount_to_sell.amount.value;
    alice_reward_last_update = db->head_block_time();
    dave_smt_volume -= op.amount_to_sell.amount.value;
    dave_reward_last_update = db->head_block_time();

    ops = get_last_operations( 1 );
    fill_order_op = ops[0].get< fill_order_operation >();

    BOOST_REQUIRE( fill_order_op.open_owner == "alice" );
    BOOST_REQUIRE( fill_order_op.open_orderid == 9 );
    BOOST_REQUIRE( fill_order_op.open_pays.amount.value == 7 * ( alice_smt.amount.value / 20 ) );
    BOOST_REQUIRE( fill_order_op.current_owner == "dave" );
    BOOST_REQUIRE( fill_order_op.current_orderid == 10 );
    BOOST_REQUIRE( fill_order_op.current_pays.amount.value == 7 * ( alice_smt.amount.value / 20 ) );

    reward = liquidity_idx.find( get_account_id( "alice" ) );
    BOOST_REQUIRE( reward == liquidity_idx.end() );
    /*BOOST_REQUIRE( reward->owner == get_account_id( "alice" ) );
    BOOST_REQUIRE( reward->get_hbd_volume() == alice_smt_volume );
    BOOST_REQUIRE( reward->get_hive_volume() == alice_hive_volume );
    BOOST_CHECK( reward->last_update == alice_reward_last_update );*/

    reward = liquidity_idx.find( get_account_id( "bob" ) );
    BOOST_REQUIRE( reward == liquidity_idx.end() );
    /*BOOST_REQUIRE( reward->owner == get_account_id( "bob" ) );
    BOOST_REQUIRE( reward->get_hbd_volume() == bob_smt_volume );
    BOOST_REQUIRE( reward->get_hive_volume() == bob_hive_volume );
    BOOST_CHECK( reward->last_update == bob_reward_last_update );*/

    reward = liquidity_idx.find( get_account_id( "sam" ) );
    BOOST_REQUIRE( reward == liquidity_idx.end() );
    /*BOOST_REQUIRE( reward->owner == get_account_id( "sam" ) );
    BOOST_REQUIRE( reward->get_hbd_volume() == sam_smt_volume );
    BOOST_REQUIRE( reward->get_hive_volume() == sam_hive_volume );
    BOOST_CHECK( reward->last_update == sam_reward_last_update );*/

    reward = liquidity_idx.find( get_account_id( "dave" ) );
    BOOST_REQUIRE( reward == liquidity_idx.end() );
    /*BOOST_REQUIRE( reward->owner == get_account_id( "dave" ) );
    BOOST_REQUIRE( reward->get_hbd_volume() == dave_smt_volume );
    BOOST_REQUIRE( reward->get_hive_volume() == dave_hive_volume );
    BOOST_CHECK( reward->last_update == dave_reward_last_update );*/

    op.owner = "bob";
    op.amount_to_sell.amount = alice_smt.amount / 20;
    op.min_to_receive.amount = op.amount_to_sell.amount;
    op.orderid = 11;
    tx.operations.clear();
    tx.operations.push_back( op );
    push_transaction( tx, bob_private_key );

    alice_smt_volume += op.amount_to_sell.amount.value;
    alice_reward_last_update = db->head_block_time();
    bob_smt_volume -= op.amount_to_sell.amount.value;
    bob_reward_last_update = db->head_block_time();

    ops = get_last_operations( 1 );
    fill_order_op = ops[0].get< fill_order_operation >();

    BOOST_REQUIRE( fill_order_op.open_owner == "alice" );
    BOOST_REQUIRE( fill_order_op.open_orderid == 9 );
    BOOST_REQUIRE( fill_order_op.open_pays.amount.value == alice_smt.amount.value / 20 );
    BOOST_REQUIRE( fill_order_op.current_owner == "bob" );
    BOOST_REQUIRE( fill_order_op.current_orderid == 11 );
    BOOST_REQUIRE( fill_order_op.current_pays.amount.value == alice_smt.amount.value / 20 );

    reward = liquidity_idx.find( get_account_id( "alice" ) );
    BOOST_REQUIRE( reward == liquidity_idx.end() );
    /*BOOST_REQUIRE( reward->owner == get_account_id( "alice" ) );
    BOOST_REQUIRE( reward->get_hbd_volume() == alice_smt_volume );
    BOOST_REQUIRE( reward->get_hive_volume() == alice_hive_volume );
    BOOST_CHECK( reward->last_update == alice_reward_last_update );*/

    reward = liquidity_idx.find( get_account_id( "bob" ) );
    BOOST_REQUIRE( reward == liquidity_idx.end() );
    /*BOOST_REQUIRE( reward->owner == get_account_id( "bob" ) );
    BOOST_REQUIRE( reward->get_hbd_volume() == bob_smt_volume );
    BOOST_REQUIRE( reward->get_hive_volume() == bob_hive_volume );
    BOOST_CHECK( reward->last_update == bob_reward_last_update );*/

    reward = liquidity_idx.find( get_account_id( "sam" ) );
    BOOST_REQUIRE( reward == liquidity_idx.end() );
    /*BOOST_REQUIRE( reward->owner == get_account_id( "sam" ) );
    BOOST_REQUIRE( reward->get_hbd_volume() == sam_smt_volume );
    BOOST_REQUIRE( reward->get_hive_volume() == sam_hive_volume );
    BOOST_CHECK( reward->last_update == sam_reward_last_update );*/

    reward = liquidity_idx.find( get_account_id( "dave" ) );
    BOOST_REQUIRE( reward == liquidity_idx.end() );
    /*BOOST_REQUIRE( reward->owner == get_account_id( "dave" ) );
    BOOST_REQUIRE( reward->get_hbd_volume() == dave_smt_volume );
    BOOST_REQUIRE( reward->get_hive_volume() == dave_hive_volume );
    BOOST_CHECK( reward->last_update == dave_reward_last_update );*/

    transfer.to = "bob";
    transfer.from = "alice";
    transfer.amount = asset( alice_smt.amount / 5, any_smt_symbol );
    tx.operations.clear();
    tx.operations.push_back( transfer );
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    push_transaction( tx, alice_private_key );

    op.owner = "bob";
    op.orderid = 12;
    op.amount_to_sell = asset( 3 * ( alice_smt.amount / 40 ), any_smt_symbol );
    op.min_to_receive = asset( op.amount_to_sell.amount, HIVE_SYMBOL );
    tx.operations.clear();
    tx.operations.push_back( op );
    push_transaction( tx, bob_private_key );

    generate_blocks( db->head_block_time() + HIVE_MIN_LIQUIDITY_REWARD_PERIOD_SEC_HF10, true );

    op.owner = "dave";
    op.orderid = 13;
    op.amount_to_sell = op.min_to_receive;
    op.min_to_receive.symbol = any_smt_symbol;
    tx.operations.clear();
    tx.operations.push_back( op );
    push_transaction( tx, dave_private_key );

    bob_hive_volume += op.amount_to_sell.amount.value;
    bob_reward_last_update = db->head_block_time();
    dave_hive_volume -= op.amount_to_sell.amount.value;
    dave_reward_last_update = db->head_block_time();

    ops = get_last_operations( 1 );
    fill_order_op = ops[0].get< fill_order_operation >();

    BOOST_REQUIRE( fill_order_op.open_owner == "bob" );
    BOOST_REQUIRE( fill_order_op.open_orderid == 12 );
    BOOST_REQUIRE( fill_order_op.open_pays.amount.value == 3 * ( alice_smt.amount.value / 40 ) );
    BOOST_REQUIRE( fill_order_op.current_owner == "dave" );
    BOOST_REQUIRE( fill_order_op.current_orderid == 13 );
    BOOST_REQUIRE( fill_order_op.current_pays.amount.value == 3 * ( alice_smt.amount.value / 40 ) );

    reward = liquidity_idx.find( get_account_id( "alice" ) );
    BOOST_REQUIRE( reward == liquidity_idx.end() );
    /*BOOST_REQUIRE( reward->owner == get_account_id( "alice" ) );
    BOOST_REQUIRE( reward->get_hbd_volume() == alice_smt_volume );
    BOOST_REQUIRE( reward->get_hive_volume() == alice_hive_volume );
    BOOST_CHECK( reward->last_update == alice_reward_last_update );*/

    reward = liquidity_idx.find( get_account_id( "bob" ) );
    BOOST_REQUIRE( reward == liquidity_idx.end() );
    /*BOOST_REQUIRE( reward->owner == get_account_id( "bob" ) );
    BOOST_REQUIRE( reward->get_hbd_volume() == bob_smt_volume );
    BOOST_REQUIRE( reward->get_hive_volume() == bob_hive_volume );
    BOOST_CHECK( reward->last_update == bob_reward_last_update );*/

    reward = liquidity_idx.find( get_account_id( "sam" ) );
    BOOST_REQUIRE( reward == liquidity_idx.end() );
    /*BOOST_REQUIRE( reward->owner == get_account_id( "sam" ) );
    BOOST_REQUIRE( reward->get_hbd_volume() == sam_smt_volume );
    BOOST_REQUIRE( reward->get_hive_volume() == sam_hive_volume );
    BOOST_CHECK( reward->last_update == sam_reward_last_update );*/

    reward = liquidity_idx.find( get_account_id( "dave" ) );
    BOOST_REQUIRE( reward == liquidity_idx.end() );
    /*BOOST_REQUIRE( reward->owner == get_account_id( "dave" ) );
    BOOST_REQUIRE( reward->get_hbd_volume() == dave_smt_volume );
    BOOST_REQUIRE( reward->get_hive_volume() == dave_hive_volume );
    BOOST_CHECK( reward->last_update == dave_reward_last_update );*/

    auto alice_balance = get_balance( "alice" );
    auto bob_balance = get_balance( "bob" );
    auto sam_balance = get_balance( "sam" );
    auto dave_balance = get_balance( "dave" );

    BOOST_TEST_MESSAGE( "Generating Blocks to trigger liquidity rewards" );

    db->liquidity_rewards_enabled = true;
    generate_blocks( HIVE_LIQUIDITY_REWARD_BLOCKS - ( db->head_block_num() % HIVE_LIQUIDITY_REWARD_BLOCKS ) - 1 );

    BOOST_REQUIRE( db->head_block_num() % HIVE_LIQUIDITY_REWARD_BLOCKS == HIVE_LIQUIDITY_REWARD_BLOCKS - 1 );
    BOOST_REQUIRE( get_balance( "alice" ).amount.value == alice_balance.amount.value );
    BOOST_REQUIRE( get_balance( "bob" ).amount.value == bob_balance.amount.value );
    BOOST_REQUIRE( get_balance( "sam" ).amount.value == sam_balance.amount.value );
    BOOST_REQUIRE( get_balance( "dave" ).amount.value == dave_balance.amount.value );

    generate_block();

    //alice_balance += HIVE_MIN_LIQUIDITY_REWARD;

    BOOST_REQUIRE( get_balance( "alice" ).amount.value == alice_balance.amount.value );
    BOOST_REQUIRE( get_balance( "bob" ).amount.value == bob_balance.amount.value );
    BOOST_REQUIRE( get_balance( "sam" ).amount.value == sam_balance.amount.value );
    BOOST_REQUIRE( get_balance( "dave" ).amount.value == dave_balance.amount.value );

    ops = get_last_operations( 1 );

    HIVE_REQUIRE_THROW( ops[0].get< liquidity_reward_operation>(), fc::exception );
    //BOOST_REQUIRE( ops[0].get< liquidity_reward_operation>().payout.amount.value == HIVE_MIN_LIQUIDITY_REWARD.amount.value );

    generate_blocks( HIVE_LIQUIDITY_REWARD_BLOCKS );

    //bob_balance += HIVE_MIN_LIQUIDITY_REWARD;

    BOOST_REQUIRE( get_balance( "alice" ).amount.value == alice_balance.amount.value );
    BOOST_REQUIRE( get_balance( "bob" ).amount.value == bob_balance.amount.value );
    BOOST_REQUIRE( get_balance( "sam" ).amount.value == sam_balance.amount.value );
    BOOST_REQUIRE( get_balance( "dave" ).amount.value == dave_balance.amount.value );

    ops = get_last_operations( 1 );

    HIVE_REQUIRE_THROW( ops[0].get< liquidity_reward_operation>(), fc::exception );
    //BOOST_REQUIRE( ops[0].get< liquidity_reward_operation>().payout.amount.value == HIVE_MIN_LIQUIDITY_REWARD.amount.value );

    alice_hive_volume = 0;
    alice_smt_volume = 0;
    bob_hive_volume = 0;
    bob_smt_volume = 0;

    BOOST_TEST_MESSAGE( "Testing liquidity timeout" );

    generate_blocks( sam_reward_last_update + HIVE_LIQUIDITY_TIMEOUT_SEC - fc::seconds( HIVE_BLOCK_INTERVAL / 2 ) - HIVE_MIN_LIQUIDITY_REWARD_PERIOD_SEC, true );

    op.owner = "sam";
    op.orderid = 14;
    op.amount_to_sell = ASSET( "1.000 TESTS" );
    op.min_to_receive = ASSET( "1.000 TBD" );
    tx.operations.clear();
    tx.operations.push_back( op );
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    push_transaction( tx, sam_private_key );

    generate_blocks( db->head_block_time() + ( HIVE_BLOCK_INTERVAL / 2 ) + HIVE_LIQUIDITY_TIMEOUT_SEC, true );

    reward = liquidity_idx.find( get_account_id( "sam" ) );
    /*BOOST_REQUIRE( reward == liquidity_idx.end() );
    BOOST_REQUIRE( reward->owner == get_account_id( "sam" ) );
    BOOST_REQUIRE( reward->get_hbd_volume() == sam_smt_volume );
    BOOST_REQUIRE( reward->get_hive_volume() == sam_hive_volume );
    BOOST_CHECK( reward->last_update == sam_reward_last_update );*/

    generate_block();

    op.owner = "alice";
    op.orderid = 15;
    op.amount_to_sell.symbol = any_smt_symbol;
    op.min_to_receive.symbol = HIVE_SYMBOL;
    tx.operations.clear();
    tx.operations.push_back( op );
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    push_transaction( tx, alice_private_key );

    sam_smt_volume = ASSET( "1.000 TBD" ).amount.value;
    sam_hive_volume = 0;
    sam_reward_last_update = db->head_block_time();

    reward = liquidity_idx.find( get_account_id( "sam" ) );
    /*BOOST_REQUIRE( reward == liquidity_idx.end() );
    BOOST_REQUIRE( reward->owner == get_account_id( "sam" ) );
    BOOST_REQUIRE( reward->get_hbd_volume() == sam_smt_volume );
    BOOST_REQUIRE( reward->get_hive_volume() == sam_hive_volume );
    BOOST_CHECK( reward->last_update == sam_reward_last_update );*/

    validate_database();
  }
  FC_LOG_AND_RETHROW();
}

BOOST_AUTO_TEST_SUITE_END()
#endif
