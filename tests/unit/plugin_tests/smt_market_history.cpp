#if defined IS_TEST_NET && defined HIVE_ENABLE_SMT
#include <boost/test/unit_test.hpp>

#include <hive/chain/account_object.hpp>
#include <hive/chain/comment_object.hpp>
#include <hive/protocol/hive_operations.hpp>

#include <hive/plugins/market_history/market_history_plugin.hpp>

#include "../db_fixture/database_fixture.hpp"

using namespace hive::chain;
using namespace hive::protocol;

BOOST_FIXTURE_TEST_SUITE( smt_market_history, smt_database_fixture_for_plugin )

BOOST_AUTO_TEST_CASE( smt_mh_test )
{
  using namespace hive::plugins::market_history;

  try
  {
    auto _data_dir = common_init( [&]( appbase::application& app, int argc, char** argv )
    {
      app.register_plugin< market_history_plugin >();
      db_plugin = &app.register_plugin< hive::plugins::debug_node::debug_node_plugin >();

      db_plugin->logging = false;
      app.initialize<
        hive::plugins::market_history::market_history_plugin,
        hive::plugins::debug_node::debug_node_plugin
      >( argc, argv );

      db = &app.get_plugin< hive::plugins::chain::chain_plugin >().db();
      BOOST_REQUIRE( db );
    } );

    init_account_pub_key = init_account_priv_key.get_public_key();

    open_database( _data_dir );

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

    ACTORS( (alice)(bob)(sam)(smtcreator) );

    signed_transaction tx;
    asset_symbol_type any_smt_symbol = create_smt( "smtcreator", smtcreator_private_key, 3);

    fund( "alice", ASSET( "1000.000 TESTS" ) );
    fund( "bob", ASSET( "1000.000 TESTS" ) );
    fund( "sam", ASSET( "1000.000 TESTS" ) );
    fund( "alice", asset( 1000000, any_smt_symbol ) );

    tx.operations.clear();
    tx.signatures.clear();

    const auto& bucket_idx = db->get_index< bucket_index >().indices().get< by_bucket >();
    const auto& order_hist_idx = db->get_index< order_history_index >().indices().get< by_id >();

    BOOST_REQUIRE( bucket_idx.begin() == bucket_idx.end() );
    BOOST_REQUIRE( order_hist_idx.begin() == order_hist_idx.end() );
    validate_database();

    auto fill_order_a_time = db->head_block_time();
    auto time_a = fc::time_point_sec( ( fill_order_a_time.sec_since_epoch() / 15 ) * 15 );

    limit_order_create_operation op;
    op.owner = "alice";
    op.amount_to_sell = asset( 1000, any_smt_symbol );
    op.min_to_receive = ASSET( "2.000 TESTS" );
    op.expiration = db->head_block_time() + fc::seconds( HIVE_MAX_LIMIT_ORDER_EXPIRATION );
    tx.operations.push_back( op );
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    sign( tx, alice_private_key );
    push_transaction( tx, 0 );

    tx.operations.clear();
    tx.signatures.clear();

    op.owner = "bob";
    op.amount_to_sell = ASSET( "1.500 TESTS" );
    op.min_to_receive = asset( 750, any_smt_symbol );
    tx.operations.push_back( op );
    sign( tx, bob_private_key );
    push_transaction( tx, 0 );

    generate_blocks( db->head_block_time() + ( 60 * 90 ) );

    auto fill_order_b_time = db->head_block_time();

    tx.operations.clear();
    tx.signatures.clear();

    op.owner = "sam";
    op.amount_to_sell = ASSET( "1.000 TESTS" );
    op.min_to_receive = asset( 500, any_smt_symbol );
    tx.operations.push_back( op );
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    sign( tx, sam_private_key );
    push_transaction( tx, 0 );

    generate_blocks( db->head_block_time() + 60 );

    auto fill_order_c_time = db->head_block_time();

    tx.operations.clear();
    tx.signatures.clear();

    op.owner = "alice";
    op.amount_to_sell = asset( 500, any_smt_symbol );
    op.min_to_receive = ASSET( "0.900 TESTS" );
    tx.operations.push_back( op );
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    sign( tx, alice_private_key );
    push_transaction( tx, 0 );

    tx.operations.clear();
    tx.signatures.clear();

    op.owner = "bob";
    op.amount_to_sell = ASSET( "0.450 TESTS" );
    op.min_to_receive = asset( 250, any_smt_symbol );
    tx.operations.push_back( op );
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    sign( tx, bob_private_key );
    push_transaction( tx, 0 );
    validate_database();

    auto bucket = bucket_idx.begin();

    BOOST_REQUIRE( bucket->seconds == 15 );
    BOOST_REQUIRE( bucket->open == time_a );
    BOOST_REQUIRE( bucket->hive.high == ASSET( "1.500 TESTS " ).amount );
    BOOST_REQUIRE( bucket->non_hive.high == asset( 750, any_smt_symbol ).amount );
    BOOST_REQUIRE( bucket->hive.low == ASSET( "1.500 TESTS" ).amount );
    BOOST_REQUIRE( bucket->non_hive.low == asset( 750, any_smt_symbol ).amount );
    BOOST_REQUIRE( bucket->hive.open == ASSET( "1.500 TESTS" ).amount );
    BOOST_REQUIRE( bucket->non_hive.open == asset( 750, any_smt_symbol ).amount );
    BOOST_REQUIRE( bucket->hive.close == ASSET( "1.500 TESTS").amount );
    BOOST_REQUIRE( bucket->non_hive.close == asset( 750, any_smt_symbol ).amount );
    BOOST_REQUIRE( bucket->hive.volume == ASSET( "1.500 TESTS" ).amount );
    BOOST_REQUIRE( bucket->non_hive.volume == asset( 750, any_smt_symbol ).amount );
    bucket++;

    BOOST_REQUIRE( bucket->seconds == 15 );
    BOOST_REQUIRE( bucket->open == time_a + ( 60 * 90 ) );
    BOOST_REQUIRE( bucket->hive.high == ASSET( "0.500 TESTS " ).amount );
    BOOST_REQUIRE( bucket->non_hive.high == asset( 250, any_smt_symbol ).amount );
    BOOST_REQUIRE( bucket->hive.low == ASSET( "0.500 TESTS" ).amount );
    BOOST_REQUIRE( bucket->non_hive.low == asset( 250, any_smt_symbol ).amount );
    BOOST_REQUIRE( bucket->hive.open == ASSET( "0.500 TESTS" ).amount );
    BOOST_REQUIRE( bucket->non_hive.open == asset( 250, any_smt_symbol ).amount );
    BOOST_REQUIRE( bucket->hive.close == ASSET( "0.500 TESTS").amount );
    BOOST_REQUIRE( bucket->non_hive.close == asset( 250, any_smt_symbol ).amount );
    BOOST_REQUIRE( bucket->hive.volume == ASSET( "0.500 TESTS" ).amount );
    BOOST_REQUIRE( bucket->non_hive.volume == asset( 250, any_smt_symbol ).amount );
    bucket++;

    BOOST_REQUIRE( bucket->seconds == 15 );
    BOOST_REQUIRE( bucket->open == time_a + ( 60 * 90 ) + 60 );
    BOOST_REQUIRE( bucket->hive.high == ASSET( "0.450 TESTS " ).amount );
    BOOST_REQUIRE( bucket->non_hive.high == asset( 250, any_smt_symbol ).amount );
    BOOST_REQUIRE( bucket->hive.low == ASSET( "0.500 TESTS" ).amount );
    BOOST_REQUIRE( bucket->non_hive.low == asset( 250, any_smt_symbol ).amount );
    BOOST_REQUIRE( bucket->hive.open == ASSET( "0.500 TESTS" ).amount );
    BOOST_REQUIRE( bucket->non_hive.open == asset( 250, any_smt_symbol ).amount );
    BOOST_REQUIRE( bucket->hive.close == ASSET( "0.450 TESTS").amount );
    BOOST_REQUIRE( bucket->non_hive.close == asset( 250, any_smt_symbol ).amount );
    BOOST_REQUIRE( bucket->hive.volume == ASSET( "0.950 TESTS" ).amount );
    BOOST_REQUIRE( bucket->non_hive.volume == asset( 500, any_smt_symbol ).amount );
    bucket++;

    BOOST_REQUIRE( bucket->seconds == 60 );
    BOOST_REQUIRE( bucket->open == time_a );
    BOOST_REQUIRE( bucket->hive.high == ASSET( "1.500 TESTS " ).amount );
    BOOST_REQUIRE( bucket->non_hive.high == asset( 750, any_smt_symbol ).amount );
    BOOST_REQUIRE( bucket->hive.low == ASSET( "1.500 TESTS" ).amount );
    BOOST_REQUIRE( bucket->non_hive.low == asset( 750, any_smt_symbol ).amount );
    BOOST_REQUIRE( bucket->hive.open == ASSET( "1.500 TESTS" ).amount );
    BOOST_REQUIRE( bucket->non_hive.open == asset( 750, any_smt_symbol ).amount );
    BOOST_REQUIRE( bucket->hive.close == ASSET( "1.500 TESTS").amount );
    BOOST_REQUIRE( bucket->non_hive.close == asset( 750, any_smt_symbol ).amount );
    BOOST_REQUIRE( bucket->hive.volume == ASSET( "1.500 TESTS" ).amount );
    BOOST_REQUIRE( bucket->non_hive.volume == asset( 750, any_smt_symbol ).amount );
    bucket++;

    BOOST_REQUIRE( bucket->seconds == 60 );
    BOOST_REQUIRE( bucket->open == time_a + ( 60 * 90 ) );
    BOOST_REQUIRE( bucket->hive.high == ASSET( "0.500 TESTS " ).amount );
    BOOST_REQUIRE( bucket->non_hive.high == asset( 250, any_smt_symbol ).amount );
    BOOST_REQUIRE( bucket->hive.low == ASSET( "0.500 TESTS" ).amount );
    BOOST_REQUIRE( bucket->non_hive.low == asset( 250, any_smt_symbol ).amount );
    BOOST_REQUIRE( bucket->hive.open == ASSET( "0.500 TESTS" ).amount );
    BOOST_REQUIRE( bucket->non_hive.open == asset( 250, any_smt_symbol ).amount );
    BOOST_REQUIRE( bucket->hive.close == ASSET( "0.500 TESTS").amount );
    BOOST_REQUIRE( bucket->non_hive.close == asset( 250, any_smt_symbol ).amount );
    BOOST_REQUIRE( bucket->hive.volume == ASSET( "0.500 TESTS" ).amount );
    BOOST_REQUIRE( bucket->non_hive.volume == asset( 250, any_smt_symbol ).amount );
    bucket++;

    BOOST_REQUIRE( bucket->seconds == 60 );
    BOOST_REQUIRE( bucket->open == time_a + ( 60 * 90 ) + 60 );
    BOOST_REQUIRE( bucket->hive.high == ASSET( "0.450 TESTS " ).amount );
    BOOST_REQUIRE( bucket->non_hive.high == asset( 250, any_smt_symbol ).amount );
    BOOST_REQUIRE( bucket->hive.low == ASSET( "0.500 TESTS" ).amount );
    BOOST_REQUIRE( bucket->non_hive.low == asset( 250, any_smt_symbol ).amount );
    BOOST_REQUIRE( bucket->hive.open == ASSET( "0.500 TESTS" ).amount );
    BOOST_REQUIRE( bucket->non_hive.open == asset( 250, any_smt_symbol ).amount );
    BOOST_REQUIRE( bucket->hive.close == ASSET( "0.450 TESTS").amount );
    BOOST_REQUIRE( bucket->non_hive.close == asset( 250, any_smt_symbol ).amount );
    BOOST_REQUIRE( bucket->hive.volume == ASSET( "0.950 TESTS" ).amount );
    BOOST_REQUIRE( bucket->non_hive.volume == asset( 500, any_smt_symbol ).amount );
    bucket++;

    BOOST_REQUIRE( bucket->seconds == 300 );
    BOOST_REQUIRE( bucket->open == time_a );
    BOOST_REQUIRE( bucket->hive.high == ASSET( "1.500 TESTS " ).amount );
    BOOST_REQUIRE( bucket->non_hive.high == asset( 750, any_smt_symbol ).amount );
    BOOST_REQUIRE( bucket->hive.low == ASSET( "1.500 TESTS" ).amount );
    BOOST_REQUIRE( bucket->non_hive.low == asset( 750, any_smt_symbol ).amount );
    BOOST_REQUIRE( bucket->hive.open == ASSET( "1.500 TESTS" ).amount );
    BOOST_REQUIRE( bucket->non_hive.open == asset( 750, any_smt_symbol ).amount );
    BOOST_REQUIRE( bucket->hive.close == ASSET( "1.500 TESTS").amount );
    BOOST_REQUIRE( bucket->non_hive.close == asset( 750, any_smt_symbol ).amount );
    BOOST_REQUIRE( bucket->hive.volume == ASSET( "1.500 TESTS" ).amount );
    BOOST_REQUIRE( bucket->non_hive.volume == asset( 750, any_smt_symbol ).amount );
    bucket++;

    BOOST_REQUIRE( bucket->seconds == 300 );
    BOOST_REQUIRE( bucket->open == time_a + ( 60 * 90 ) );
    BOOST_REQUIRE( bucket->hive.high == ASSET( "0.450 TESTS " ).amount );
    BOOST_REQUIRE( bucket->non_hive.high == asset( 250, any_smt_symbol ).amount );
    BOOST_REQUIRE( bucket->hive.low == ASSET( "0.500 TESTS" ).amount );
    BOOST_REQUIRE( bucket->non_hive.low == asset( 250, any_smt_symbol ).amount );
    BOOST_REQUIRE( bucket->hive.open == ASSET( "0.500 TESTS" ).amount );
    BOOST_REQUIRE( bucket->non_hive.open == asset( 250, any_smt_symbol ).amount );
    BOOST_REQUIRE( bucket->hive.close == ASSET( "0.450 TESTS").amount );
    BOOST_REQUIRE( bucket->non_hive.close == asset( 250, any_smt_symbol ).amount );
    BOOST_REQUIRE( bucket->hive.volume == ASSET( "1.450 TESTS" ).amount );
    BOOST_REQUIRE( bucket->non_hive.volume == asset( 750, any_smt_symbol ).amount );
    bucket++;

    BOOST_REQUIRE( bucket->seconds == 3600 );
    BOOST_REQUIRE( bucket->open == time_a );
    BOOST_REQUIRE( bucket->hive.high == ASSET( "1.500 TESTS " ).amount );
    BOOST_REQUIRE( bucket->non_hive.high == asset( 750, any_smt_symbol ).amount );
    BOOST_REQUIRE( bucket->hive.low == ASSET( "1.500 TESTS" ).amount );
    BOOST_REQUIRE( bucket->non_hive.low == asset( 750, any_smt_symbol ).amount );
    BOOST_REQUIRE( bucket->hive.open == ASSET( "1.500 TESTS" ).amount );
    BOOST_REQUIRE( bucket->non_hive.open == asset( 750, any_smt_symbol ).amount );
    BOOST_REQUIRE( bucket->hive.close == ASSET( "1.500 TESTS").amount );
    BOOST_REQUIRE( bucket->non_hive.close == asset( 750, any_smt_symbol ).amount );
    BOOST_REQUIRE( bucket->hive.volume == ASSET( "1.500 TESTS" ).amount );
    BOOST_REQUIRE( bucket->non_hive.volume == asset( 750, any_smt_symbol ).amount );
    bucket++;

    BOOST_REQUIRE( bucket->seconds == 3600 );
    BOOST_REQUIRE( bucket->open == time_a + ( 60 * 60 ) );
    BOOST_REQUIRE( bucket->hive.high == ASSET( "0.450 TESTS " ).amount );
    BOOST_REQUIRE( bucket->non_hive.high == asset( 250, any_smt_symbol ).amount );
    BOOST_REQUIRE( bucket->hive.low == ASSET( "0.500 TESTS" ).amount );
    BOOST_REQUIRE( bucket->non_hive.low == asset( 250, any_smt_symbol ).amount );
    BOOST_REQUIRE( bucket->hive.open == ASSET( "0.500 TESTS" ).amount );
    BOOST_REQUIRE( bucket->non_hive.open == asset( 250, any_smt_symbol ).amount );
    BOOST_REQUIRE( bucket->hive.close == ASSET( "0.450 TESTS").amount );
    BOOST_REQUIRE( bucket->non_hive.close == asset( 250, any_smt_symbol ).amount );
    BOOST_REQUIRE( bucket->hive.volume == ASSET( "1.450 TESTS" ).amount );
    BOOST_REQUIRE( bucket->non_hive.volume == asset( 750, any_smt_symbol ).amount );
    bucket++;

    BOOST_REQUIRE( bucket->seconds == 86400 );
    BOOST_REQUIRE( bucket->open == HIVE_GENESIS_TIME );
    BOOST_REQUIRE( bucket->hive.high == ASSET( "0.450 TESTS " ).amount );
    BOOST_REQUIRE( bucket->non_hive.high == asset( 250, any_smt_symbol ).amount );
    BOOST_REQUIRE( bucket->hive.low == ASSET( "1.500 TESTS" ).amount );
    BOOST_REQUIRE( bucket->non_hive.low == asset( 750, any_smt_symbol ).amount );
    BOOST_REQUIRE( bucket->hive.open == ASSET( "1.500 TESTS" ).amount );
    BOOST_REQUIRE( bucket->non_hive.open == asset( 750, any_smt_symbol ).amount );
    BOOST_REQUIRE( bucket->hive.close == ASSET( "0.450 TESTS").amount );
    BOOST_REQUIRE( bucket->non_hive.close == asset( 250, any_smt_symbol ).amount );
    BOOST_REQUIRE( bucket->hive.volume == ASSET( "2.950 TESTS" ).amount );
    BOOST_REQUIRE( bucket->non_hive.volume == asset( 1500, any_smt_symbol ).amount );
    bucket++;

    BOOST_REQUIRE( bucket == bucket_idx.end() );

    auto order = order_hist_idx.begin();

    BOOST_REQUIRE( order->time == fill_order_a_time );
    BOOST_REQUIRE( order->op.current_owner == "bob" );
    BOOST_REQUIRE( order->op.current_orderid == 0 );
    BOOST_REQUIRE( order->op.current_pays == ASSET( "1.500 TESTS" ) );
    BOOST_REQUIRE( order->op.open_owner == "alice" );
    BOOST_REQUIRE( order->op.open_orderid == 0 );
    BOOST_REQUIRE( order->op.open_pays == asset( 750, any_smt_symbol ) );
    order++;

    BOOST_REQUIRE( order->time == fill_order_b_time );
    BOOST_REQUIRE( order->op.current_owner == "sam" );
    BOOST_REQUIRE( order->op.current_orderid == 0 );
    BOOST_REQUIRE( order->op.current_pays == ASSET( "0.500 TESTS" ) );
    BOOST_REQUIRE( order->op.open_owner == "alice" );
    BOOST_REQUIRE( order->op.open_orderid == 0 );
    BOOST_REQUIRE( order->op.open_pays == asset( 250, any_smt_symbol ) );
    order++;

    BOOST_REQUIRE( order->time == fill_order_c_time );
    BOOST_REQUIRE( order->op.current_owner == "alice" );
    BOOST_REQUIRE( order->op.current_orderid == 0 );
    BOOST_REQUIRE( order->op.current_pays == asset( 250, any_smt_symbol ) );
    BOOST_REQUIRE( order->op.open_owner == "sam" );
    BOOST_REQUIRE( order->op.open_orderid == 0 );
    BOOST_REQUIRE( order->op.open_pays == ASSET( "0.500 TESTS" ) );
    order++;

    BOOST_REQUIRE( order->time == fill_order_c_time );
    BOOST_REQUIRE( order->op.current_owner == "bob" );
    BOOST_REQUIRE( order->op.current_orderid == 0 );
    BOOST_REQUIRE( order->op.current_pays == ASSET( "0.450 TESTS" ) );
    BOOST_REQUIRE( order->op.open_owner == "alice" );
    BOOST_REQUIRE( order->op.open_orderid == 0 );
    BOOST_REQUIRE( order->op.open_pays == asset( 250, any_smt_symbol ) );
    order++;

    BOOST_REQUIRE( order == order_hist_idx.end() );

    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_SUITE_END()
#endif
