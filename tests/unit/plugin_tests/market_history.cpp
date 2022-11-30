#ifdef IS_TEST_NET
#include <boost/test/unit_test.hpp>

#include <hive/chain/account_object.hpp>
#include <hive/chain/comment_object.hpp>
#include <hive/chain/hive_objects.hpp>
#include <hive/chain/full_transaction.hpp>
#include <hive/protocol/hive_operations.hpp>

#include <hive/plugins/market_history/market_history_plugin.hpp>
#include <hive/plugins/market_history_api/market_history_api_plugin.hpp>
#include <hive/plugins/market_history_api/market_history_api.hpp>

#include "../db_fixture/database_fixture.hpp"

using namespace hive::chain;
using namespace hive::protocol;

BOOST_FIXTURE_TEST_SUITE( market_history, database_fixture )

BOOST_AUTO_TEST_CASE( mh_test )
{
  using namespace hive::plugins::market_history;

  try
  {
    market_history_api* mh_api = nullptr;
    auto _data_dir = common_init( [&]( appbase::application& app, int argc, char** argv )
    {
      app.register_plugin< market_history_plugin >();
      app.register_plugin< market_history_api_plugin >();
      db_plugin = &app.register_plugin< hive::plugins::debug_node::debug_node_plugin >();

      db_plugin->logging = false;
      app.initialize<
        hive::plugins::market_history::market_history_plugin,
        hive::plugins::market_history::market_history_api_plugin,
        hive::plugins::debug_node::debug_node_plugin
      >( argc, argv );

      db = &app.get_plugin< hive::plugins::chain::chain_plugin >().db();
      mh_api = app.get_plugin< market_history_api_plugin >().api.get();
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

    ACTORS( (alice)(bob)(sam) );
    generate_block();

    fund( "alice", ASSET( "1000.000 TESTS" ) );
    fund( "alice", ASSET( "1000.000 TBD" ) );
    fund( "bob", ASSET( "1000.000 TESTS" ) );
    fund( "bob", ASSET( "1000.000 TBD" ) );
    fund( "sam", ASSET( "1000.000 TESTS" ) );
    fund( "sam", ASSET( "1000.000 TBD" ) );

    const auto& bucket_idx = db->get_index< bucket_index >().indices().get< by_bucket >();
    const auto& order_hist_idx = db->get_index< order_history_index >().indices().get< by_id >();

    BOOST_REQUIRE( bucket_idx.begin() == bucket_idx.end() );
    BOOST_REQUIRE( order_hist_idx.begin() == order_hist_idx.end() );
    validate_database();

    signed_transaction tx;

    auto fill_order_a_time = db->head_block_time();
    auto time_a = fc::time_point_sec( ( fill_order_a_time.sec_since_epoch() / 15 ) * 15 );

    limit_order_create_operation op;
    op.owner = "alice";
    op.amount_to_sell = ASSET( "1.000 TBD" );
    op.min_to_receive = ASSET( "2.000 TESTS" );
    op.expiration = db->head_block_time() + fc::seconds( HIVE_MAX_LIMIT_ORDER_EXPIRATION );
    tx.operations.push_back( op );
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    push_transaction( tx, alice_private_key );

    tx.operations.clear();
    

    op.owner = "bob";
    op.amount_to_sell = ASSET( "1.500 TESTS" );
    op.min_to_receive = ASSET( "0.750 TBD" );
    tx.operations.push_back( op );
    push_transaction( tx, bob_private_key );

    generate_blocks( db->head_block_time() + ( 60 * 90 ) );

    auto fill_order_b_time = db->head_block_time();

    tx.operations.clear();
    

    op.owner = "sam";
    op.amount_to_sell = ASSET( "1.000 TESTS" );
    op.min_to_receive = ASSET( "0.500 TBD" );
    tx.operations.push_back( op );
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    push_transaction( tx, sam_private_key );

    generate_blocks( db->head_block_time() + 60 );

    auto fill_order_c_time = db->head_block_time();

    tx.operations.clear();
    

    op.owner = "alice";
    op.amount_to_sell = ASSET( "0.500 TBD" );
    op.min_to_receive = ASSET( "0.900 TESTS" );
    tx.operations.push_back( op );
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    push_transaction( tx, alice_private_key );

    tx.operations.clear();
    

    op.owner = "bob";
    op.amount_to_sell = ASSET( "0.450 TESTS" );
    op.min_to_receive = ASSET( "0.250 TBD" );
    tx.operations.push_back( op );
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    push_transaction( tx, bob_private_key );
    validate_database();

    auto bucket = bucket_idx.begin();
    auto check_bucket = []( const bucket_object& b, uint32_t s, fc::time_point_sec o,
      int64_t h_h, int64_t h_l, int64_t h_o, int64_t h_c, int64_t h_v,
      int64_t d_h, int64_t d_l, int64_t d_o, int64_t d_c, int64_t d_v )
    {
      BOOST_REQUIRE_EQUAL( b.seconds, s );
      BOOST_REQUIRE( b.open == o );
      BOOST_REQUIRE_EQUAL( b.hive.high.value, h_h );
      BOOST_REQUIRE_EQUAL( b.hive.low.value, h_l );
      BOOST_REQUIRE_EQUAL( b.hive.open.value, h_o );
      BOOST_REQUIRE_EQUAL( b.hive.close.value, h_c );
      BOOST_REQUIRE_EQUAL( b.hive.volume.value, h_v );
      BOOST_REQUIRE_EQUAL( b.non_hive.high.value, d_h );
      BOOST_REQUIRE_EQUAL( b.non_hive.low.value, d_l );
      BOOST_REQUIRE_EQUAL( b.non_hive.open.value, d_o );
      BOOST_REQUIRE_EQUAL( b.non_hive.close.value, d_c );
      BOOST_REQUIRE_EQUAL( b.non_hive.volume.value, d_v );
    };

    check_bucket( *bucket, 15, time_a, 1500, 1500, 1500, 1500, 1500, 750, 750, 750, 750, 750 );
    bucket++;
    check_bucket( *bucket, 15, time_a + ( 60 * 90 ), 500, 500, 500, 500, 500, 250, 250, 250, 250, 250 );
    bucket++;
    check_bucket( *bucket, 15, time_a + ( 60 * 90 ) + 60, 450, 500, 500, 450, 950, 250, 250, 250, 250, 500 );
    bucket++;
    check_bucket( *bucket, 60, time_a, 1500, 1500, 1500, 1500, 1500, 750, 750, 750, 750, 750 );
    bucket++;
    check_bucket( *bucket, 60, time_a + ( 60 * 90 ), 500, 500, 500, 500, 500, 250, 250, 250, 250, 250 );
    bucket++;
    check_bucket( *bucket, 60, time_a + ( 60 * 90 ) + 60, 450, 500, 500, 450, 950, 250, 250, 250, 250, 500 );
    bucket++;
    check_bucket( *bucket, 300, time_a, 1500, 1500, 1500, 1500, 1500, 750, 750, 750, 750, 750 );
    bucket++;
    check_bucket( *bucket, 300, time_a + ( 60 * 90 ), 450, 500, 500, 450, 1450, 250, 250, 250, 250, 750 );
    bucket++;
    check_bucket( *bucket, 3600, time_a, 1500, 1500, 1500, 1500, 1500, 750, 750, 750, 750, 750 );
    bucket++;
    check_bucket( *bucket, 3600, time_a + ( 60 * 60 ), 450, 500, 500, 450, 1450, 250, 250, 250, 250, 750 );
    bucket++;
    check_bucket( *bucket, 86400, HIVE_GENESIS_TIME, 450, 1500, 1500, 450, 2950, 250, 750, 750, 250, 1500 );
    bucket++;

    BOOST_REQUIRE( bucket == bucket_idx.end() );

    auto order = order_hist_idx.begin();

    BOOST_REQUIRE( order->time == fill_order_a_time );
    BOOST_REQUIRE( order->op.current_owner == "bob" );
    BOOST_REQUIRE( order->op.current_orderid == 0 );
    BOOST_REQUIRE( order->op.current_pays == ASSET( "1.500 TESTS" ) );
    BOOST_REQUIRE( order->op.open_owner == "alice" );
    BOOST_REQUIRE( order->op.open_orderid == 0 );
    BOOST_REQUIRE( order->op.open_pays == ASSET( "0.750 TBD" ) );
    order++;

    BOOST_REQUIRE( order->time == fill_order_b_time );
    BOOST_REQUIRE( order->op.current_owner == "sam" );
    BOOST_REQUIRE( order->op.current_orderid == 0 );
    BOOST_REQUIRE( order->op.current_pays == ASSET( "0.500 TESTS" ) );
    BOOST_REQUIRE( order->op.open_owner == "alice" );
    BOOST_REQUIRE( order->op.open_orderid == 0 );
    BOOST_REQUIRE( order->op.open_pays == ASSET( "0.250 TBD" ) );
    order++;

    BOOST_REQUIRE( order->time == fill_order_c_time );
    BOOST_REQUIRE( order->op.current_owner == "alice" );
    BOOST_REQUIRE( order->op.current_orderid == 0 );
    BOOST_REQUIRE( order->op.current_pays == ASSET( "0.250 TBD" ) );
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
    BOOST_REQUIRE( order->op.open_pays == ASSET( "0.250 TBD" ) );
    order++;

    BOOST_REQUIRE( order == order_hist_idx.end() );

    generate_blocks( HIVE_BLOCKS_PER_DAY );
    generate_block();

    // test against bugs in market_history_api

    op.owner = "alice";
    op.amount_to_sell = ASSET( "10.000 TBD" );
    op.min_to_receive = ASSET( "20.000 TESTS" );
    op.expiration = db->head_block_time() + fc::seconds( HIVE_MAX_LIMIT_ORDER_EXPIRATION );
    op.orderid = 1;
    push_transaction( op, alice_private_key );
    op.amount_to_sell = ASSET( "20.000 TBD" );
    op.min_to_receive = ASSET( "42.000 TESTS" );
    op.orderid = 2;
    push_transaction( op, alice_private_key );
    op.amount_to_sell = ASSET( "30.000 TBD" );
    op.min_to_receive = ASSET( "68.000 TESTS" );
    op.orderid = 3;
    push_transaction( op, alice_private_key );
    generate_block();

    op.owner = "bob";
    op.amount_to_sell = ASSET( "5.000 TESTS" );
    op.min_to_receive = ASSET( "2.500 TBD" );
    op.expiration = db->head_block_time() + fc::seconds( HIVE_MAX_LIMIT_ORDER_EXPIRATION );
    op.orderid = 4;
    push_transaction( op, bob_private_key );

    generate_block();
    op.owner = "sam";
    op.amount_to_sell = ASSET( "10.000 TESTS" );
    op.min_to_receive = ASSET( "5.000 TBD" );
    op.expiration = db->head_block_time() + fc::seconds( HIVE_MAX_LIMIT_ORDER_EXPIRATION );
    op.orderid = 5;
    push_transaction( op, sam_private_key );
    generate_block();

    generate_blocks( HIVE_BLOCKS_PER_DAY );
    generate_block();

    fill_order_a_time = db->head_block_time();
    time_a = fc::time_point_sec( ( fill_order_a_time.sec_since_epoch() / 15 ) * 15 );

    op.amount_to_sell = ASSET( "68.000 TESTS" );
    op.min_to_receive = ASSET( "30.000 TBD" );
    op.expiration = db->head_block_time() + fc::seconds( HIVE_MAX_LIMIT_ORDER_EXPIRATION );
    op.orderid = 6;
    push_transaction( op, sam_private_key );
    generate_block();

    generate_blocks( 30 );
    generate_block();

    op.owner = "bob";
    op.amount_to_sell = ASSET( "75.000 TESTS" );
    op.min_to_receive = ASSET( "25.000 TBD" );
    op.expiration = db->head_block_time() + fc::seconds( HIVE_MAX_LIMIT_ORDER_EXPIRATION );
    op.orderid = 7;
    push_transaction( op, bob_private_key );
    generate_block();

    op.owner = "alice";
    op.amount_to_sell = ASSET( "50.000 TBD" );
    op.min_to_receive = ASSET( "150.000 TESTS" );
    op.expiration = db->head_block_time() + fc::seconds( HIVE_MAX_LIMIT_ORDER_EXPIRATION );
    op.orderid = 8;
    push_transaction( op, alice_private_key );

    generate_blocks( 30 );
    generate_block();

    op.owner = "bob";
    op.amount_to_sell = ASSET( "75.000 TESTS" );
    op.min_to_receive = ASSET( "25.000 TBD" );
    op.expiration = db->head_block_time() + fc::seconds( HIVE_MAX_LIMIT_ORDER_EXPIRATION );
    op.orderid = 9;
    push_transaction( op, bob_private_key );
    generate_block();

    op.owner = "sam";
    op.amount_to_sell = ASSET( "20.000 TESTS" );
    op.min_to_receive = ASSET( "10.000 TBD" );
    op.expiration = db->head_block_time() + fc::seconds( HIVE_MAX_LIMIT_ORDER_EXPIRATION );
    op.orderid = 10;
    push_transaction( op, sam_private_key );
    generate_block();

    auto ticker = mh_api->get_ticker( {} );
    BOOST_REQUIRE_EQUAL( ticker.latest, 25.0 / 75.0 );
    BOOST_REQUIRE_EQUAL( ticker.percent_change, ( ( 25.0 / 75.0 ) - ( 2.5 / 5.0 ) ) / ( 2.5 / 5.0 ) * 100.0 );
    BOOST_REQUIRE_EQUAL( ticker.lowest_ask, 10.0 / 20.0 );
    BOOST_REQUIRE_EQUAL( ticker.highest_bid, 25.0 / 75.0 );
    BOOST_REQUIRE_EQUAL( ticker.hive_volume.amount.value, 68000 + 75000 + 75000 );
    BOOST_REQUIRE_EQUAL( ticker.hbd_volume.amount.value, 31764 + 30069 + 25000 );

    auto volume = mh_api->get_volume( {} );
    BOOST_REQUIRE_EQUAL( volume.hive_volume.amount.value, ticker.hive_volume.amount.value );
    BOOST_REQUIRE_EQUAL( volume.hbd_volume.amount.value, ticker.hbd_volume.amount.value );

    auto orders = mh_api->get_order_book( { 10 } );
    // just last orders from sam and bob
    BOOST_REQUIRE_EQUAL( orders.asks.size(), 1 );
    BOOST_REQUIRE_EQUAL( orders.asks.front().real_price, ticker.lowest_ask );
    BOOST_REQUIRE_EQUAL( orders.asks.front().hive.value, 20000 );
    BOOST_REQUIRE_EQUAL( orders.asks.front().hbd.value, 10000 );
    BOOST_REQUIRE_EQUAL( orders.bids.size(), 1 );
    BOOST_REQUIRE_EQUAL( orders.bids.front().real_price, ticker.highest_bid );
    BOOST_REQUIRE_EQUAL( orders.bids.front().hive.value, 47001 );
    BOOST_REQUIRE_EQUAL( orders.bids.front().hbd.value, 15667 );

    //get_trade_history/get_recent_trades should be moved to HAF/AH, so not testing them here

    auto buckets = mh_api->get_market_history( { 15, db->head_block_time() - 86400, db->head_block_time() } ).buckets;
    BOOST_REQUIRE_EQUAL( buckets.size(), 3 );
    check_bucket( buckets[0], 15, time_a, 5000, 21000, 5000, 21000, 68000, 2500, 9264, 2500, 9264, 31764 );
    check_bucket( buckets[1], 15, time_a + ( 7 * 15 ), 47001, 27999, 47001, 27999, 75000, 20736, 9333, 20736, 9333, 30069 );
    check_bucket( buckets[2], 15, time_a + ( 13 * 15 ), 75000, 75000, 75000, 75000, 75000, 25000, 25000, 25000, 25000, 25000 );
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_SUITE_END()
#endif
