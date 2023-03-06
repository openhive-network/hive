#if defined IS_TEST_NET
#include <boost/test/unit_test.hpp>

#include <fc/io/json.hpp>

#include <hive/plugins/account_history_api/account_history_api_plugin.hpp>
#include <hive/plugins/account_history_api/account_history_api.hpp>
#include <hive/plugins/database_api/database_api_plugin.hpp>
#include <hive/plugins/condenser_api/condenser_api_plugin.hpp>
#include <hive/plugins/condenser_api/condenser_api.hpp>

#include "../db_fixture/database_fixture.hpp"

using namespace hive::chain;
using namespace hive::plugins;
using namespace hive::protocol;

struct condenser_api_fixture : database_fixture
{
  condenser_api_fixture()
  {
    auto _data_dir = common_init( [&]( appbase::application& app, int argc, char** argv )
    {
      // Set cashout values to absolute safe minimum to speed up the scenarios and their tests.
      configuration_data.set_cashout_related_values( 0, 2, 4, 12, 1 );

      ah_plugin = &app.register_plugin< ah_plugin_type >();
      ah_plugin->set_destroy_database_on_startup();
      ah_plugin->set_destroy_database_on_shutdown();
      app.register_plugin< hive::plugins::account_history::account_history_api_plugin >();
      app.register_plugin< hive::plugins::database_api::database_api_plugin >();
      app.register_plugin< hive::plugins::condenser_api::condenser_api_plugin >();
      db_plugin = &app.register_plugin< hive::plugins::debug_node::debug_node_plugin >();

      int test_argc = 1;
      const char* test_argv[] = { boost::unit_test::framework::master_test_suite().argv[ 0 ] };

      db_plugin->logging = false;
      app.initialize<
        ah_plugin_type,
        hive::plugins::account_history::account_history_api_plugin,
        hive::plugins::database_api::database_api_plugin,
        hive::plugins::condenser_api::condenser_api_plugin,
        hive::plugins::debug_node::debug_node_plugin >( test_argc, ( char** ) test_argv );

      db = &app.get_plugin< hive::plugins::chain::chain_plugin >().db();
      BOOST_REQUIRE( db );

      ah_plugin->plugin_startup();

      auto& account_history = app.get_plugin< hive::plugins::account_history::account_history_api_plugin > ();
      account_history.plugin_startup();
      account_history_api = account_history.api.get();
      BOOST_REQUIRE( account_history_api );

      auto& condenser = app.get_plugin< hive::plugins::condenser_api::condenser_api_plugin >();
      condenser.plugin_startup(); //has to be called because condenser fills its variables then
      condenser_api = condenser.api.get();
      BOOST_REQUIRE( condenser_api );
    } );

    init_account_pub_key = init_account_priv_key.get_public_key();

    open_database( _data_dir );

    generate_block();
    validate_database();
  }

  virtual ~condenser_api_fixture()
  {
    try {
      configuration_data.reset_cashout_values();

      // If we're unwinding due to an exception, don't do any more checks.
      // This way, boost test's last checkpoint tells us approximately where the error was.
      if( !std::uncaught_exceptions() )
      {
        BOOST_CHECK( db->get_node_properties().skip_flags == database::skip_nothing );
      }

      if( ah_plugin )
        ah_plugin->plugin_shutdown();
      if( data_dir )
        db->wipe( data_dir->path(), data_dir->path(), true );
      return;
    } FC_CAPTURE_AND_LOG( () )
      }

  hive::plugins::condenser_api::condenser_api* condenser_api = nullptr;
  hive::plugins::account_history::account_history_api* account_history_api = nullptr;

  // Below are testing scenarios that trigger pushing different operations in the block(s).
  // Each scenario is intended to be used by several tests (e.g. tests on get_ops_in_block/get_transaction/get_account_history)

  typedef std::function < void( uint32_t generate_no_further_than ) > check_point_tester_t;

  void verify_and_advance_to_block( uint32_t block_num )
  {
    BOOST_REQUIRE( db->head_block_num() <= block_num );
    while( db->head_block_num() < block_num )
      generate_block();
  }

  /** 
   * Tests the operations that happen only on hardfork 1 - vesting_shares_split_operation & system_warning_operation (block 21)
   * Also tests: hardfork_operation & producer_reward_operation
  */
  void hf1_scenario( check_point_tester_t check_point_tester )
  {
    generate_block();
    
    // Set first hardfork to test virtual operations that happen only there.
    db->set_hardfork( HIVE_HARDFORK_0_1 );
    generate_block();

    check_point_tester( std::numeric_limits<uint32_t>::max() ); // <- no limit to max number of block generated inside.
  }

  /** 
   * Tests pow_operation that needs hardfork lower than 13.
   * Also tests: pow_reward_operation, account_created_operation, comment_operation (see database_fixture::create_with_pow) & producer_reward_operation.
  */
  void hf12_scenario( check_point_tester_t check_point_tester )
  {
    db->set_hardfork( HIVE_HARDFORK_0_12 );
    generate_block();

    PREP_ACTOR( carol0ah )
    create_with_pow( "carol0ah", carol0ah_public_key, carol0ah_private_key );

    check_point_tester( std::numeric_limits<uint32_t>::max() ); // <- no limit to max number of block generated inside.
  }

  /**
   * Tests operations that need hardfork lower than 20:
   *  pow2_operation (< hf17), ineffective_delete_comment_operation (< hf19) & account_create_with_delegation_operation (< hf20)
   * Also tests: 
   *  account_created_operation, pow_reward_operation, transfer_operation, comment_operation, comment_options_operation,
   *  vote_operation, effective_comment_vote_operation, delete_comment_operation & producer_reward_operation
   */
  void hf13_scenario( check_point_tester_t check_point_1_tester, check_point_tester_t check_point_2_tester )
  {
  db->set_hardfork( HIVE_HARDFORK_0_13 );
  vest( HIVE_INIT_MINER_NAME, HIVE_INIT_MINER_NAME, ASSET( "1000.000 TESTS" ) );
  generate_block();
  
  PREP_ACTOR( dan0ah )
  create_with_pow2( "dan0ah", dan0ah_public_key, dan0ah_private_key );

  PREP_ACTOR( edgar0ah )
  create_with_delegation( HIVE_INIT_MINER_NAME, "edgar0ah", edgar0ah_public_key, edgar0ah_post_key, ASSET( "100000000.000000 VESTS" ), init_account_priv_key );

  post_comment("edgar0ah", "permlink1", "Title 1", "Body 1", "parentpermlink1", edgar0ah_private_key);
  set_comment_options( "edgar0ah", "permlink1", ASSET( "50.010 TBD" ), HIVE_100_PERCENT, true, true, edgar0ah_private_key );

  check_point_1_tester( 24 ); // generate up to block 24th inside
  verify_and_advance_to_block( 24 );

  vote("edgar0ah", "permlink1", "dan0ah", HIVE_1_PERCENT * 100, dan0ah_private_key);
  delete_comment( "edgar0ah", "permlink1", edgar0ah_private_key );

  check_point_2_tester( std::numeric_limits<uint32_t>::max() ); // <- no limit to max number of block generated inside.
  }

  /**
   * Operations tested here:
   *  curation_reward_operation, author_reward_operation, comment_reward_operation, comment_payout_update_operation,
   *  claim_reward_balance_operation, producer_reward_operation
   */  
  void comment_and_reward_scenario( check_point_tester_t check_point_1_tester, check_point_tester_t check_point_2_tester )
  {
    db->set_hardfork( HIVE_HARDFORK_1_27 );
    generate_block();
    
    ACTORS( (dan0ah)(edgar0ah) );

    post_comment("edgar0ah", "permlink1", "Title 1", "Body 1", "parentpermlink1", edgar0ah_private_key);
    vote("edgar0ah", "permlink1", "dan0ah", HIVE_1_PERCENT * 100, dan0ah_private_key);

    check_point_1_tester( 27 ); // generate up to block 27th inside
    verify_and_advance_to_block( 27 );

    claim_reward_balance( "edgar0ah", ASSET( "0.000 TESTS" ), ASSET( "0.575 TBD" ), ASSET( "80.000000 VESTS" ), edgar0ah_private_key );

    check_point_2_tester( std::numeric_limits<uint32_t>::max() ); // <- no limit to max number of block generated inside.
  }

  /**
   * Operations tested here:
   *  convert_operation, collateralized_convert_operation, collateralized_convert_immediate_conversion_operation,
   *  limit_order_create_operation, limit_order_create2_operation, limit_order_cancel_operation, limit_order_cancelled_operation,
   *  producer_reward_operation,
   *  fill_convert_request_operation, fill_collateralized_convert_request_operation
   */  
  void convert_and_limit_order_scenario( check_point_tester_t check_point_tester )
  {
    db->set_hardfork( HIVE_HARDFORK_1_27 );
    generate_block();

    ACTORS( (edgar3ah)(carol3ah) );
    generate_block();

    fund( "edgar3ah", ASSET( "300.000 TBD" ) );
    fund( "carol3ah", ASSET( "300.000 TESTS" ) );
    generate_block();

    convert_hbd_to_hive( "edgar3ah", 0, ASSET( "11.201 TBD" ), edgar3ah_private_key );
    collateralized_convert_hive_to_hbd( "carol3ah", 0, ASSET( "22.102 TESTS" ), carol3ah_private_key );
    limit_order_create( "carol3ah", ASSET( "11.400 TESTS" ), ASSET( "11.650 TBD" ), false, fc::seconds( HIVE_MAX_LIMIT_ORDER_EXPIRATION ), 1, carol3ah_private_key );
    limit_order2_create( "carol3ah", ASSET( "22.075 TESTS" ), price( ASSET( "0.010 TESTS" ), ASSET( "0.010 TBD" ) ), false, fc::seconds( HIVE_MAX_LIMIT_ORDER_EXPIRATION ), 2, carol3ah_private_key );
    limit_order_cancel( "carol3ah", 1, carol3ah_private_key );

    check_point_tester( std::numeric_limits<uint32_t>::max() ); // <- no limit to max number of block generated inside.
  }

  /**
   * Operations tested here:
   *  transfer_to_vesting_operation, transfer_to_vesting_completed_operation, set_withdraw_vesting_route_operation,
   *  delegate_vesting_shares_operation, withdraw_vesting_operation, producer_reward_operation,
   *  fill_vesting_withdraw_operation & return_vesting_delegation_operation
   */
  void vesting_scenario( check_point_tester_t check_point_1_tester, check_point_tester_t check_point_2_tester )
  {
    // Set hardfork below HF20, to keep delegation return period short
    // (see HIVE_DELEGATION_RETURN_PERIOD_HF0 / HIVE_DELEGATION_RETURN_PERIOD_HF20 definitions)
    db->set_hardfork( HIVE_HARDFORK_0_19 );
    generate_block();

    ACTORS( (alice4ah)(ben4ah)(carol4ah) );
    generate_block();
    fund( "alice4ah", ASSET( "2.900 TESTS" ) );
    generate_block();

    vest( "alice4ah", "alice4ah", asset(2000, HIVE_SYMBOL), alice4ah_private_key );
    set_withdraw_vesting_route( "alice4ah", "ben4ah", HIVE_1_PERCENT * 50, true, alice4ah_private_key);
    delegate_vest( "alice4ah", "carol4ah", asset(3, VESTS_SYMBOL), alice4ah_private_key );
    withdraw_vesting( "alice4ah", asset( 123, VESTS_SYMBOL ), alice4ah_private_key );
    
    check_point_1_tester( 26 ); // generate up to block 26th inside
    verify_and_advance_to_block( 26 );

    delegate_vest( "alice4ah", "carol4ah", asset(2, VESTS_SYMBOL), alice4ah_private_key );

    check_point_2_tester( std::numeric_limits<uint32_t>::max() ); // <- no limit to max number of block generated inside.
  }
};

BOOST_FIXTURE_TEST_SUITE( condenser_api_tests, condenser_api_fixture );

BOOST_AUTO_TEST_CASE( get_witness_schedule_test )
{ try {
  // Start code block moved here from condenser_api_fixture's constructor,
  // to allow other tests using condenser_api_fixture with earlier hardforks.
  {
    db->set_hardfork( HIVE_NUM_HARDFORKS );
    generate_block();

    ACTORS((top1)(top2)(top3)(top4)(top5)(top6)(top7)(top8)(top9)(top10)
           (top11)(top12)(top13)(top14)(top15)(top16)(top17)(top18)(top19)(top20))
    ACTORS((backup1)(backup2)(backup3)(backup4)(backup5)(backup6)(backup7)(backup8)(backup9)(backup10))

    const auto create_witness = [&]( const std::string& witness_name )
    {
      const private_key_type account_key = generate_private_key( witness_name );
      const private_key_type witness_key = generate_private_key( witness_name + "_witness" );
      witness_create( witness_name, account_key, witness_name + ".com", witness_key.get_public_key(), 1000 );
    };
    for( int i = 1; i <= 20; ++i )
      create_witness( "top" + std::to_string(i) );
    for( int i = 1; i <= 10; ++i )
      create_witness( "backup" + std::to_string(i) );

    ACTORS((whale)(voter1)(voter2)(voter3)(voter4)(voter5)(voter6)(voter7)(voter8)(voter9)(voter10))

    fund( "whale", 500000000 );
    vest( "whale", 500000000 );

    account_witness_vote_operation op;
    op.account = "whale";
    op.approve = true;
    for( int v = 1; v <= 20; ++v )
    {
      op.witness = "top" + std::to_string(v);
      push_transaction( op, whale_private_key );
    }

    for( int i = 1; i <= 10; ++i )
    {
      std::string name = "voter" + std::to_string(i);
      fund( name, 10000000 );
      vest( name, 10000000 / i );
      op.account = name;
      for( int v = 1; v <= i; ++v )
      {
        op.witness = "backup" + std::to_string(v);
        push_transaction( op, generate_private_key(name) );
      }
    }

    generate_blocks( 28800 ); // wait 1 day for delayed governance vote to activate
    generate_blocks( db->head_block_time() + fc::seconds( 3 * 21 * 3 ), false ); // produce three more schedules:
      // - first new witnesses are included in future schedule for the first time
      // - then future schedule becomes active
      // - finally we need to wait for all those witnesses to actually produce to update their HF votes
      //   (because in previous step they actually voted for HF0 as majority_version)

    validate_database();
  }
  // End code block moved here from condenser_api_fixture's constructor

  BOOST_TEST_MESSAGE( "get_active_witnesses / get_witness_schedule test" );

  auto scheduled_witnesses_1 = condenser_api->get_active_witnesses( { false } );
  auto active_schedule_1 = condenser_api->get_witness_schedule( { false } );
  ilog( "initial active schedule: ${active_schedule_1}", ( active_schedule_1 ) );
  auto all_scheduled_witnesses_1 = condenser_api->get_active_witnesses( { true } );
  auto full_schedule_1 = condenser_api->get_witness_schedule( { true } );
  ilog( "initial full schedule: ${full_schedule_1}", ( full_schedule_1 ) );

  BOOST_REQUIRE( active_schedule_1.future_changes.valid() == false );
  BOOST_REQUIRE_EQUAL( active_schedule_1.current_shuffled_witnesses.size(), HIVE_MAX_WITNESSES );
  BOOST_REQUIRE_EQUAL( scheduled_witnesses_1.size(), HIVE_MAX_WITNESSES );
  for( int i = 0; i < HIVE_MAX_WITNESSES; ++i )
    BOOST_REQUIRE( active_schedule_1.current_shuffled_witnesses[i] == scheduled_witnesses_1[i] );
  BOOST_REQUIRE_GT( active_schedule_1.next_shuffle_block_num, db->head_block_num() );
  BOOST_REQUIRE_EQUAL( active_schedule_1.next_shuffle_block_num, full_schedule_1.next_shuffle_block_num );
  BOOST_REQUIRE_EQUAL( full_schedule_1.current_shuffled_witnesses.size(), HIVE_MAX_WITNESSES );
  BOOST_REQUIRE( full_schedule_1.future_shuffled_witnesses.valid() == true );
  BOOST_REQUIRE_EQUAL( full_schedule_1.future_shuffled_witnesses->size(), HIVE_MAX_WITNESSES );
  BOOST_REQUIRE_EQUAL( all_scheduled_witnesses_1.size(), 2 * HIVE_MAX_WITNESSES + 1 );
  for( int i = 0; i < HIVE_MAX_WITNESSES; ++i )
  {
    BOOST_REQUIRE( full_schedule_1.current_shuffled_witnesses[i] == all_scheduled_witnesses_1[i] );
    BOOST_REQUIRE( full_schedule_1.future_shuffled_witnesses.value()[i] == all_scheduled_witnesses_1[ HIVE_MAX_WITNESSES + 1 + i ] );
  }
  BOOST_REQUIRE( all_scheduled_witnesses_1[ HIVE_MAX_WITNESSES ] == "" );
  BOOST_REQUIRE( full_schedule_1.future_changes.valid() == true );
  BOOST_REQUIRE( full_schedule_1.future_changes->majority_version.valid() == true );
  BOOST_REQUIRE( full_schedule_1.future_changes->majority_version.value() > active_schedule_1.majority_version );

  generate_blocks( db->head_block_time() + fc::seconds( 3 * 21 ), false ); // one full schedule

  auto active_schedule_2 = condenser_api->get_witness_schedule( { false } );
  ilog( " active schedule: ${active_schedule_2}", ( active_schedule_2 ) );
  auto full_schedule_2 = condenser_api->get_witness_schedule( { true } );
  ilog( "initial full schedule: ${full_schedule_2}", ( full_schedule_2 ) );

  BOOST_REQUIRE( active_schedule_2.future_changes.valid() == false );
  BOOST_REQUIRE( active_schedule_2.current_virtual_time > active_schedule_1.current_virtual_time );
  BOOST_REQUIRE_EQUAL( active_schedule_2.next_shuffle_block_num, active_schedule_1.next_shuffle_block_num + HIVE_MAX_WITNESSES );
  BOOST_REQUIRE_EQUAL( active_schedule_2.num_scheduled_witnesses, active_schedule_1.num_scheduled_witnesses );
  BOOST_REQUIRE_EQUAL( active_schedule_2.elected_weight, active_schedule_1.elected_weight );
  BOOST_REQUIRE_EQUAL( active_schedule_2.timeshare_weight, active_schedule_1.timeshare_weight );
  BOOST_REQUIRE_EQUAL( active_schedule_2.miner_weight, active_schedule_1.miner_weight );
  BOOST_REQUIRE_EQUAL( active_schedule_2.witness_pay_normalization_factor, active_schedule_1.witness_pay_normalization_factor );
  BOOST_REQUIRE( active_schedule_2.median_props.account_creation_fee.to_asset() == active_schedule_1.median_props.account_creation_fee.to_asset() );
  BOOST_REQUIRE_EQUAL( active_schedule_2.median_props.maximum_block_size, active_schedule_1.median_props.maximum_block_size );
  BOOST_REQUIRE_EQUAL( active_schedule_2.median_props.hbd_interest_rate, active_schedule_1.median_props.hbd_interest_rate );
  BOOST_REQUIRE_EQUAL( active_schedule_2.median_props.account_subsidy_budget, active_schedule_1.median_props.account_subsidy_budget );
  BOOST_REQUIRE_EQUAL( active_schedule_2.median_props.account_subsidy_decay, active_schedule_1.median_props.account_subsidy_decay );
  BOOST_REQUIRE( active_schedule_2.majority_version == full_schedule_1.future_changes->majority_version.value() );
  BOOST_REQUIRE_EQUAL( active_schedule_2.max_voted_witnesses, active_schedule_1.max_voted_witnesses );
  BOOST_REQUIRE_EQUAL( active_schedule_2.max_miner_witnesses, active_schedule_1.max_miner_witnesses );
  BOOST_REQUIRE_EQUAL( active_schedule_2.max_runner_witnesses, active_schedule_1.max_runner_witnesses );
  BOOST_REQUIRE_EQUAL( active_schedule_2.hardfork_required_witnesses, active_schedule_1.hardfork_required_witnesses );
  BOOST_REQUIRE( active_schedule_2.account_subsidy_rd == active_schedule_1.account_subsidy_rd );
  BOOST_REQUIRE( active_schedule_2.account_subsidy_witness_rd == active_schedule_1.account_subsidy_witness_rd );
  BOOST_REQUIRE_EQUAL( active_schedule_2.min_witness_account_subsidy_decay, active_schedule_1.min_witness_account_subsidy_decay );
  for( int i = 0; i < HIVE_MAX_WITNESSES; ++i )
    BOOST_REQUIRE( active_schedule_2.current_shuffled_witnesses[i] == full_schedule_1.future_shuffled_witnesses.value()[i] );
  BOOST_REQUIRE( full_schedule_2.future_changes.valid() == false ); // no further changes

  // since basic mechanisms were tested on naturally filled schedules, we can now test a bit more using
  // more convenient fake data forced into state with debug plugin

  db_plugin->debug_update( [=]( database& db )
  {
    db.modify( db.get_future_witness_schedule_object(), [&]( witness_schedule_object& fwso )
    {
      fwso.median_props.account_creation_fee = ASSET( "3.000 TESTS" );
    } );
  } );
  auto full_schedule = condenser_api->get_witness_schedule( { true } );

  BOOST_REQUIRE( full_schedule.future_changes.valid() == true );
  {
    const auto& changes = full_schedule.future_changes.value();
    BOOST_REQUIRE( changes.num_scheduled_witnesses.valid() == false );
    BOOST_REQUIRE( changes.elected_weight.valid() == false );
    BOOST_REQUIRE( changes.timeshare_weight.valid() == false );
    BOOST_REQUIRE( changes.miner_weight.valid() == false );
    BOOST_REQUIRE( changes.witness_pay_normalization_factor.valid() == false );
    BOOST_REQUIRE( changes.median_props.valid() == true );
    const auto& props_changes = changes.median_props.value();
    BOOST_REQUIRE( props_changes.account_creation_fee.valid() == true );
    BOOST_REQUIRE( props_changes.account_creation_fee.value() == ASSET( "3.000 TESTS" ) );
    BOOST_REQUIRE( props_changes.maximum_block_size.valid() == false );
    BOOST_REQUIRE( props_changes.hbd_interest_rate.valid() == false );
    BOOST_REQUIRE( props_changes.account_subsidy_budget.valid() == false );
    BOOST_REQUIRE( props_changes.account_subsidy_decay.valid() == false );
    BOOST_REQUIRE( changes.majority_version.valid() == false );
    BOOST_REQUIRE( changes.max_voted_witnesses.valid() == false );
    BOOST_REQUIRE( changes.max_miner_witnesses.valid() == false );
    BOOST_REQUIRE( changes.max_runner_witnesses.valid() == false );
    BOOST_REQUIRE( changes.hardfork_required_witnesses.valid() == false );
    BOOST_REQUIRE( changes.account_subsidy_rd.valid() == false );
    BOOST_REQUIRE( changes.account_subsidy_witness_rd.valid() == false );
    BOOST_REQUIRE( changes.min_witness_account_subsidy_decay.valid() == false );
  }
  generate_block();

  db_plugin->debug_update( [=]( database& db )
  {
    db.modify( db.get_future_witness_schedule_object(), [&]( witness_schedule_object& fwso )
    {
      fwso.median_props.account_creation_fee = active_schedule_2.median_props.account_creation_fee; //revert previous change
      fwso.majority_version = version( fwso.majority_version.major_v(),
        fwso.majority_version.minor_v() + 1, fwso.majority_version.rev_v() + 10 );
    } );
  } );
  full_schedule = condenser_api->get_witness_schedule( { true } );

  BOOST_REQUIRE( full_schedule.future_changes.valid() == true );
  {
    const auto& changes = full_schedule.future_changes.value();
    BOOST_REQUIRE( changes.num_scheduled_witnesses.valid() == false );
    BOOST_REQUIRE( changes.elected_weight.valid() == false );
    BOOST_REQUIRE( changes.timeshare_weight.valid() == false );
    BOOST_REQUIRE( changes.miner_weight.valid() == false );
    BOOST_REQUIRE( changes.witness_pay_normalization_factor.valid() == false );
    BOOST_REQUIRE( changes.median_props.valid() == false );
    BOOST_REQUIRE( changes.majority_version.valid() == true );
    BOOST_REQUIRE( changes.max_voted_witnesses.valid() == false );
    BOOST_REQUIRE( changes.max_miner_witnesses.valid() == false );
    BOOST_REQUIRE( changes.max_runner_witnesses.valid() == false );
    BOOST_REQUIRE( changes.hardfork_required_witnesses.valid() == false );
    BOOST_REQUIRE( changes.account_subsidy_rd.valid() == false );
    BOOST_REQUIRE( changes.account_subsidy_witness_rd.valid() == false );
    BOOST_REQUIRE( changes.min_witness_account_subsidy_decay.valid() == false );
  }
  generate_block();

  validate_database();

} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_SUITE_END() // condenser_api_tests

// account history API -> where it's used in condenser API implementation
//  get_ops_in_block -> get_ops_in_block
//  get_transaction -> ditto get_transaction
//  get_account_history -> ditto get_account_history
//  enum_virtual_ops -> not used

/// Account history pattern goes first in the pair, condenser version pattern follows.
typedef std::vector< std::pair< std::string, std::string > > expected_t;

BOOST_FIXTURE_TEST_SUITE( condenser_get_ops_in_block_tests, condenser_api_fixture );

void compare_get_ops_in_block_results(const condenser_api::get_ops_in_block_return& block_ops,
                                      const account_history::get_ops_in_block_return& ah_block_ops,
                                      uint32_t block_num,
                                      const expected_t& expected_operations )
{
  ilog("block #${num}, ${op} operations from condenser, ${ah} operations from account history",
    ("num", block_num)("op", block_ops.size())("ah", ah_block_ops.ops.size()));
  BOOST_REQUIRE_EQUAL( block_ops.size(), ah_block_ops.ops.size() );
  BOOST_REQUIRE( expected_operations.size() == ah_block_ops.ops.size() );

  auto i_condenser = block_ops.begin();
  auto i_ah = ah_block_ops.ops.begin();
  for (size_t index = 0; i_condenser != block_ops.end(); ++i_condenser, ++i_ah, ++index )
  {
    ilog("result ah is ${result}", ("result", fc::json::to_string(*i_ah)));
    ilog("result condenser is ${result}", ("result", fc::json::to_string(*i_condenser)));

    // Compare operations in their serialized form with expected patterns:
    const auto expected = expected_operations[index];
    BOOST_REQUIRE_EQUAL( expected.first, fc::json::to_string(*i_ah) );
    BOOST_REQUIRE_EQUAL( expected.second, fc::json::to_string(*i_condenser) );
  }
}

void test_get_ops_in_block( condenser_api_fixture& caf, const expected_t& expected_operations,
  const expected_t& expected_virtual_operations, uint32_t block_num )
{
  // Call condenser get_ops_in_block and verify results with result of account history variant.
  // Note that condenser variant calls ah's one with default value of include_reversible = false.
  // Two arguments, second set to false.
  auto block_ops = caf.condenser_api->get_ops_in_block({block_num, false /*only_virtual*/});
  auto ah_block_ops = caf.account_history_api->get_ops_in_block({block_num, false /*only_virtual*/, false /*include_reversible*/});
  compare_get_ops_in_block_results( block_ops, ah_block_ops, block_num, expected_operations );
  // Two arguments, second set to true.
  block_ops = caf.condenser_api->get_ops_in_block({block_num, true /*only_virtual*/});
  ah_block_ops = caf.account_history_api->get_ops_in_block({block_num, true /*only_virtual*/});
  compare_get_ops_in_block_results( block_ops, ah_block_ops, block_num, expected_virtual_operations );
  // Single argument
  block_ops = caf.condenser_api->get_ops_in_block({block_num});
  ah_block_ops = caf.account_history_api->get_ops_in_block({block_num});
  compare_get_ops_in_block_results( block_ops, ah_block_ops, block_num, expected_operations );

  // Too few arguments
  BOOST_REQUIRE_THROW( caf.condenser_api->get_ops_in_block({}), fc::assert_exception );
  // Too many arguments
  BOOST_REQUIRE_THROW( caf.condenser_api->get_ops_in_block({block_num, false /*only_virtual*/, 0 /*redundant arg*/}), fc::assert_exception );
}

BOOST_AUTO_TEST_CASE( get_ops_in_block_hf1 )
{ try {

  BOOST_TEST_MESSAGE( "testing get_ops_in_block on operations of HF1" );

  auto check_point_tester = [ this ]( uint32_t generate_no_further_than )
  {
    generate_until_irreversible_block( 2 );
    BOOST_REQUIRE( db->head_block_num() <= generate_no_further_than );

    // Let's check operation that happens only on first hardfork:
    expected_t expected_operations = { { // producer_reward_operation / goes to initminer (in vests)
    R"~({"trx_id":"0000000000000000000000000000000000000000","block":2,"trx_in_block":4294967295,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:00:06","op":{"type":"producer_reward_operation","value":{"producer":"initminer","vesting_shares":{"amount":"1000","precision":3,"nai":"@@000000021"}}},"operation_id":0})~",
    R"~({"trx_id":"0000000000000000000000000000000000000000","block":2,"trx_in_block":4294967295,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:00:06","op":["producer_reward",{"producer":"initminer","vesting_shares":"1.000 TESTS"}]})~"
    }, { // hardfork_operation / HF1
    R"~({"trx_id":"0000000000000000000000000000000000000000","block":2,"trx_in_block":4294967295,"op_in_trx":2,"virtual_op":true,"timestamp":"2016-01-01T00:00:06","op":{"type":"hardfork_operation","value":{"hardfork_id":1}},"operation_id":0})~",
    R"~({"trx_id":"0000000000000000000000000000000000000000","block":2,"trx_in_block":4294967295,"op_in_trx":2,"virtual_op":true,"timestamp":"2016-01-01T00:00:06","op":["hardfork",{"hardfork_id":1}]})~"
    }, { // vesting_shares_split_operation / splitting producer reward
    R"~({"trx_id":"0000000000000000000000000000000000000000","block":2,"trx_in_block":4294967295,"op_in_trx":3,"virtual_op":true,"timestamp":"2016-01-01T00:00:06","op":{"type":"vesting_shares_split_operation","value":{"owner":"initminer","vesting_shares_before_split":{"amount":"1000000","precision":6,"nai":"@@000000037"},"vesting_shares_after_split":{"amount":"1000000000000","precision":6,"nai":"@@000000037"}}},"operation_id":0})~",
    R"~({"trx_id":"0000000000000000000000000000000000000000","block":2,"trx_in_block":4294967295,"op_in_trx":3,"virtual_op":true,"timestamp":"2016-01-01T00:00:06","op":["vesting_shares_split",{"owner":"initminer","vesting_shares_before_split":"1.000000 VESTS","vesting_shares_after_split":"1000000.000000 VESTS"}]})~"
    } }; 
    // Note that all operations of this block are virtual, hence we can reuse the same expected container here.
    test_get_ops_in_block( *this, expected_operations, expected_operations, 2 );

    generate_until_irreversible_block( 21 );
    BOOST_REQUIRE( db->head_block_num() <= generate_no_further_than );

    // In block 21 maximum block size is being changed:
    expected_operations = { { // system_warning_operation
      R"~({"trx_id":"0000000000000000000000000000000000000000","block":21,"trx_in_block":4294967295,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:01:03","op":{"type":"system_warning_operation","value":{"message":"Changing maximum block size from 2097152 to 131072"}},"operation_id":0})~",
      R"~({"trx_id":"0000000000000000000000000000000000000000","block":21,"trx_in_block":4294967295,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:01:03","op":["system_warning",{"message":"Changing maximum block size from 2097152 to 131072"}]})~"
      }, { // producer_reward_operation
      R"~({"trx_id":"0000000000000000000000000000000000000000","block":21,"trx_in_block":4294967295,"op_in_trx":2,"virtual_op":true,"timestamp":"2016-01-01T00:01:03","op":{"type":"producer_reward_operation","value":{"producer":"initminer","vesting_shares":{"amount":"1000","precision":3,"nai":"@@000000021"}}},"operation_id":0})~",
      R"~({"trx_id":"0000000000000000000000000000000000000000","block":21,"trx_in_block":4294967295,"op_in_trx":2,"virtual_op":true,"timestamp":"2016-01-01T00:01:03","op":["producer_reward",{"producer":"initminer","vesting_shares":"1.000 TESTS"}]})~"
      } };
    // Note that all operations of this block are virtual, hence we can reuse the same expected container here.
    test_get_ops_in_block( *this, expected_operations, expected_operations, 21 );
  };

  hf1_scenario( check_point_tester );

} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_CASE( get_ops_in_block_hf12 )
{ try {

  BOOST_TEST_MESSAGE( "testing get_ops_in_block with hf12_scenario" );

  auto check_point_tester = [ this ]( uint32_t generate_no_further_than )
  {
    generate_until_irreversible_block( 3 );
    BOOST_REQUIRE( db->head_block_num() <= generate_no_further_than );
    // Check the operations spawned by pow (3rd block).
    expected_t expected_operations = { { // pow_operation / creating carol0ah account
    R"~({"trx_id":"956eed17475ccab15529691d0e43e61bd83e0167","block":3,"trx_in_block":0,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:06","op":{"type":"pow_operation","value":{"worker_account":"carol0ah","block_id":"000000029182c87b08eb70059f4bdc22352cbfdb","nonce":40,"work":{"worker":"TST5Mwq5o6BruTVbCcxkqVKL4eeRm3Jrs5fjRGHshvGUj29FPh7Yr","input":"4e4ee151ae1b5317e0fa9835b308163b5fce6ba4b836ecd7dac90acbae5d477a","signature":"208e93e1810b5716c9725fb7e487c271d1eb7bd5674cc5bfb79c01a52b581b269777195d5e3d59750cdf3c10353d77373bfcf6393541cfa1411aa196097cdf90ed","work":"000bae328972f541f9fb4b8a07c52fe15005117434c597c7b37e320397036586"},"props":{"account_creation_fee":{"amount":"0","precision":3,"nai":"@@000000021"},"maximum_block_size":131072,"hbd_interest_rate":1000}}},"operation_id":0})~",
    R"~({"trx_id":"956eed17475ccab15529691d0e43e61bd83e0167","block":3,"trx_in_block":0,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:06","op":["pow",{"worker_account":"carol0ah","block_id":"000000029182c87b08eb70059f4bdc22352cbfdb","nonce":40,"work":{"worker":"TST5Mwq5o6BruTVbCcxkqVKL4eeRm3Jrs5fjRGHshvGUj29FPh7Yr","input":"4e4ee151ae1b5317e0fa9835b308163b5fce6ba4b836ecd7dac90acbae5d477a","signature":"208e93e1810b5716c9725fb7e487c271d1eb7bd5674cc5bfb79c01a52b581b269777195d5e3d59750cdf3c10353d77373bfcf6393541cfa1411aa196097cdf90ed","work":"000bae328972f541f9fb4b8a07c52fe15005117434c597c7b37e320397036586"},"props":{"account_creation_fee":"0.000 TESTS","maximum_block_size":131072,"hbd_interest_rate":1000}}]})~"
    }, { // account_created_operation
    R"~({"trx_id":"956eed17475ccab15529691d0e43e61bd83e0167","block":3,"trx_in_block":0,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:00:06","op":{"type":"account_created_operation","value":{"new_account_name":"carol0ah","creator":"carol0ah","initial_vesting_shares":{"amount":"0","precision":6,"nai":"@@000000037"},"initial_delegation":{"amount":"0","precision":6,"nai":"@@000000037"}}},"operation_id":0})~",
    R"~({"trx_id":"956eed17475ccab15529691d0e43e61bd83e0167","block":3,"trx_in_block":0,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:00:06","op":["account_created",{"new_account_name":"carol0ah","creator":"carol0ah","initial_vesting_shares":"0.000000 VESTS","initial_delegation":"0.000000 VESTS"}]})~"
    }, { // pow_reward_operation / direct result of pow_operation
    R"~({"trx_id":"956eed17475ccab15529691d0e43e61bd83e0167","block":3,"trx_in_block":0,"op_in_trx":2,"virtual_op":true,"timestamp":"2016-01-01T00:00:06","op":{"type":"pow_reward_operation","value":{"worker":"initminer","reward":{"amount":"21000","precision":3,"nai":"@@000000021"}}},"operation_id":0})~",
    R"~({"trx_id":"956eed17475ccab15529691d0e43e61bd83e0167","block":3,"trx_in_block":0,"op_in_trx":2,"virtual_op":true,"timestamp":"2016-01-01T00:00:06","op":["pow_reward",{"worker":"initminer","reward":"21.000 TESTS"}]})~"
    }, { // comment_operation / see database_fixture::create_with_pow
    R"~({"trx_id":"956eed17475ccab15529691d0e43e61bd83e0167","block":3,"trx_in_block":0,"op_in_trx":3,"virtual_op":false,"timestamp":"2016-01-01T00:00:06","op":{"type":"comment_operation","value":{"parent_author":"","parent_permlink":"test","author":"initminer","permlink":"test","title":"","body":"Hello world!","json_metadata":""}},"operation_id":0})~",
    R"~({"trx_id":"956eed17475ccab15529691d0e43e61bd83e0167","block":3,"trx_in_block":0,"op_in_trx":3,"virtual_op":false,"timestamp":"2016-01-01T00:00:06","op":["comment",{"parent_author":"","parent_permlink":"test","author":"initminer","permlink":"test","title":"","body":"Hello world!","json_metadata":""}]})~"
    }, { // producer_reward_operation
    R"~({"trx_id":"0000000000000000000000000000000000000000","block":3,"trx_in_block":4294967295,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:00:09","op":{"type":"producer_reward_operation","value":{"producer":"initminer","vesting_shares":{"amount":"1000","precision":3,"nai":"@@000000021"}}},"operation_id":0})~",
    R"~({"trx_id":"0000000000000000000000000000000000000000","block":3,"trx_in_block":4294967295,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:00:09","op":["producer_reward",{"producer":"initminer","vesting_shares":"1.000 TESTS"}]})~"
    } };
    expected_t expected_virtual_operations = { expected_operations[1], expected_operations[2], expected_operations[4] };
    test_get_ops_in_block( *this, expected_operations, expected_virtual_operations, 3 );
  };

  hf12_scenario( check_point_tester );

} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_CASE( get_ops_in_block_hf13 )
{ try {

  BOOST_TEST_MESSAGE( "testing get_ops_in_block with hf13_scenario" );

  auto check_point_1_tester = [ this ]( uint32_t generate_no_further_than )
  {
    generate_until_irreversible_block( 3 );
    BOOST_REQUIRE( db->head_block_num() <= generate_no_further_than );

    expected_t expected_operations = { { // pow2_operation / first obsolete operation tested here.
    R"~({"trx_id":"c649b3841e8fa1e6d5f1a6874348d82fb56c5e73","block":3,"trx_in_block":0,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:06","op":{"type":"pow2_operation","value":{"work":{"type":"pow2","value":{"input":{"worker_account":"dan0ah","prev_block":"00000002d94d15f9cc478a673e0122183f10f09b","nonce":9},"pow_summary":4132180148}},"new_owner_key":"TST7YJmUoKbPQkrMrZbrgPxDMYJA3uD3utaN3WYRwaFGKYbQ9ftKV","props":{"account_creation_fee":{"amount":"0","precision":3,"nai":"@@000000021"},"maximum_block_size":131072,"hbd_interest_rate":1000}}},"operation_id":0})~",
    R"~({"trx_id":"c649b3841e8fa1e6d5f1a6874348d82fb56c5e73","block":3,"trx_in_block":0,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:06","op":["pow2",{"work":["pow2",{"input":{"worker_account":"dan0ah","prev_block":"00000002d94d15f9cc478a673e0122183f10f09b","nonce":9},"pow_summary":4132180148}],"new_owner_key":"TST7YJmUoKbPQkrMrZbrgPxDMYJA3uD3utaN3WYRwaFGKYbQ9ftKV","props":{"account_creation_fee":"0.000 TESTS","maximum_block_size":131072,"hbd_interest_rate":1000}}]})~"
    }, { // account_created_operation
    R"~({"trx_id":"c649b3841e8fa1e6d5f1a6874348d82fb56c5e73","block":3,"trx_in_block":0,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:00:06","op":{"type":"account_created_operation","value":{"new_account_name":"dan0ah","creator":"dan0ah","initial_vesting_shares":{"amount":"0","precision":6,"nai":"@@000000037"},"initial_delegation":{"amount":"0","precision":6,"nai":"@@000000037"}}},"operation_id":0})~",
    R"~({"trx_id":"c649b3841e8fa1e6d5f1a6874348d82fb56c5e73","block":3,"trx_in_block":0,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:00:06","op":["account_created",{"new_account_name":"dan0ah","creator":"dan0ah","initial_vesting_shares":"0.000000 VESTS","initial_delegation":"0.000000 VESTS"}]})~"
    }, { // pow_reward_operation
    R"~({"trx_id":"c649b3841e8fa1e6d5f1a6874348d82fb56c5e73","block":3,"trx_in_block":0,"op_in_trx":2,"virtual_op":true,"timestamp":"2016-01-01T00:00:06","op":{"type":"pow_reward_operation","value":{"worker":"initminer","reward":{"amount":"1000000000000","precision":6,"nai":"@@000000037"}}},"operation_id":0})~",
    R"~({"trx_id":"c649b3841e8fa1e6d5f1a6874348d82fb56c5e73","block":3,"trx_in_block":0,"op_in_trx":2,"virtual_op":true,"timestamp":"2016-01-01T00:00:06","op":["pow_reward",{"worker":"initminer","reward":"1000000.000000 VESTS"}]})~"
    }, { // transfer_operation
    R"~({"trx_id":"c649b3841e8fa1e6d5f1a6874348d82fb56c5e73","block":3,"trx_in_block":0,"op_in_trx":3,"virtual_op":false,"timestamp":"2016-01-01T00:00:06","op":{"type":"transfer_operation","value":{"from":"initminer","to":"dan0ah","amount":{"amount":"1","precision":3,"nai":"@@000000021"},"memo":"test"}},"operation_id":0})~",
    R"~({"trx_id":"c649b3841e8fa1e6d5f1a6874348d82fb56c5e73","block":3,"trx_in_block":0,"op_in_trx":3,"virtual_op":false,"timestamp":"2016-01-01T00:00:06","op":["transfer",{"from":"initminer","to":"dan0ah","amount":"0.001 TESTS","memo":"test"}]})~"
    }, { // account_create_with_delegation_operation / second obsolete operation tested here.
    R"~({"trx_id":"14509bc4811afed8b9d5a277ca17223d3e9f8c87","block":3,"trx_in_block":1,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:06","op":{"type":"account_create_with_delegation_operation","value":{"fee":{"amount":"0","precision":3,"nai":"@@000000021"},"delegation":{"amount":"100000000000000","precision":6,"nai":"@@000000037"},"creator":"initminer","new_account_name":"edgar0ah","owner":{"weight_threshold":1,"account_auths":[],"key_auths":[["TST8R8maxJxeBMR3JYmap1n3Pypm886oEUjLYdsetzcnPDFpiq3pZ",1]]},"active":{"weight_threshold":1,"account_auths":[],"key_auths":[["TST8R8maxJxeBMR3JYmap1n3Pypm886oEUjLYdsetzcnPDFpiq3pZ",1]]},"posting":{"weight_threshold":1,"account_auths":[],"key_auths":[["TST8ZCsvwKqttXivgPyJ1MYS4q1r3fBZJh3g1SaBxVbfsqNcmnvD3",1]]},"memo_key":"TST8ZCsvwKqttXivgPyJ1MYS4q1r3fBZJh3g1SaBxVbfsqNcmnvD3","json_metadata":"","extensions":[]}},"operation_id":0})~",
    R"~({"trx_id":"14509bc4811afed8b9d5a277ca17223d3e9f8c87","block":3,"trx_in_block":1,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:06","op":["account_create_with_delegation",{"fee":"0.000 TESTS","delegation":"100000000.000000 VESTS","creator":"initminer","new_account_name":"edgar0ah","owner":{"weight_threshold":1,"account_auths":[],"key_auths":[["TST8R8maxJxeBMR3JYmap1n3Pypm886oEUjLYdsetzcnPDFpiq3pZ",1]]},"active":{"weight_threshold":1,"account_auths":[],"key_auths":[["TST8R8maxJxeBMR3JYmap1n3Pypm886oEUjLYdsetzcnPDFpiq3pZ",1]]},"posting":{"weight_threshold":1,"account_auths":[],"key_auths":[["TST8ZCsvwKqttXivgPyJ1MYS4q1r3fBZJh3g1SaBxVbfsqNcmnvD3",1]]},"memo_key":"TST8ZCsvwKqttXivgPyJ1MYS4q1r3fBZJh3g1SaBxVbfsqNcmnvD3","json_metadata":"","extensions":[]}]})~"
    }, { // account_created_operation
    R"~({"trx_id":"14509bc4811afed8b9d5a277ca17223d3e9f8c87","block":3,"trx_in_block":1,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:00:06","op":{"type":"account_created_operation","value":{"new_account_name":"edgar0ah","creator":"initminer","initial_vesting_shares":{"amount":"0","precision":6,"nai":"@@000000037"},"initial_delegation":{"amount":"100000000000000","precision":6,"nai":"@@000000037"}}},"operation_id":0})~",
    R"~({"trx_id":"14509bc4811afed8b9d5a277ca17223d3e9f8c87","block":3,"trx_in_block":1,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:00:06","op":["account_created",{"new_account_name":"edgar0ah","creator":"initminer","initial_vesting_shares":"0.000000 VESTS","initial_delegation":"100000000.000000 VESTS"}]})~"
    }, { // comment_operation
    R"~({"trx_id":"63807c110bc33695772793f61972ac9d29d7689a","block":3,"trx_in_block":2,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:06","op":{"type":"comment_operation","value":{"parent_author":"","parent_permlink":"parentpermlink1","author":"edgar0ah","permlink":"permlink1","title":"Title 1","body":"Body 1","json_metadata":""}},"operation_id":0})~",
    R"~({"trx_id":"63807c110bc33695772793f61972ac9d29d7689a","block":3,"trx_in_block":2,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:06","op":["comment",{"parent_author":"","parent_permlink":"parentpermlink1","author":"edgar0ah","permlink":"permlink1","title":"Title 1","body":"Body 1","json_metadata":""}]})~"
    }, { // comment_options_operation"
    R"~({"trx_id":"a669ed9de84b36a095741fe67b55e9a4a039f5ee","block":3,"trx_in_block":3,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:06","op":{"type":"comment_options_operation","value":{"author":"edgar0ah","permlink":"permlink1","max_accepted_payout":{"amount":"50010","precision":3,"nai":"@@000000013"},"percent_hbd":10000,"allow_votes":true,"allow_curation_rewards":true,"extensions":[]}},"operation_id":0})~",
    R"~({"trx_id":"a669ed9de84b36a095741fe67b55e9a4a039f5ee","block":3,"trx_in_block":3,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:06","op":["comment_options",{"author":"edgar0ah","permlink":"permlink1","max_accepted_payout":"50.010 TBD","percent_hbd":10000,"allow_votes":true,"allow_curation_rewards":true,"extensions":[]}]})~"
    }, { // producer_reward_operation
    R"~({"trx_id":"0000000000000000000000000000000000000000","block":3,"trx_in_block":4294967295,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:00:09","op":{"type":"producer_reward_operation","value":{"producer":"initminer","vesting_shares":{"amount":"1000","precision":3,"nai":"@@000000021"}}},"operation_id":0})~",
    R"~({"trx_id":"0000000000000000000000000000000000000000","block":3,"trx_in_block":4294967295,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:00:09","op":["producer_reward",{"producer":"initminer","vesting_shares":"1.000 TESTS"}]})~"
    } };
    expected_t expected_virtual_operations = { expected_operations[1], expected_operations[2], expected_operations[5], expected_operations[8] };
    test_get_ops_in_block( *this, expected_operations, expected_virtual_operations, 3 );
  };

  auto check_point_2_tester = [ this ]( uint32_t generate_no_further_than )
  {
    generate_until_irreversible_block( 25 );
    BOOST_REQUIRE( db->head_block_num() <= generate_no_further_than );

    expected_t expected_operations = { { // vote_operation
    R"~({"trx_id":"a9fcfc9ce8dabd6e47e7f2e0ce0b24ab03aa1611","block":25,"trx_in_block":0,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:01:12","op":{"type":"vote_operation","value":{"voter":"dan0ah","author":"edgar0ah","permlink":"permlink1","weight":10000}},"operation_id":0})~",
    R"~({"trx_id":"a9fcfc9ce8dabd6e47e7f2e0ce0b24ab03aa1611","block":25,"trx_in_block":0,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:01:12","op":["vote",{"voter":"dan0ah","author":"edgar0ah","permlink":"permlink1","weight":10000}]})~"
    }, { // effective_comment_vote_operation
    R"~({"trx_id":"a9fcfc9ce8dabd6e47e7f2e0ce0b24ab03aa1611","block":25,"trx_in_block":0,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:01:12","op":{"type":"effective_comment_vote_operation","value":{"voter":"dan0ah","author":"edgar0ah","permlink":"permlink1","weight":0,"rshares":"5100000000","total_vote_weight":0,"pending_payout":{"amount":"48000","precision":3,"nai":"@@000000013"}}},"operation_id":0})~",
    R"~({"trx_id":"a9fcfc9ce8dabd6e47e7f2e0ce0b24ab03aa1611","block":25,"trx_in_block":0,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:01:12","op":["effective_comment_vote",{"voter":"dan0ah","author":"edgar0ah","permlink":"permlink1","weight":0,"rshares":"5100000000","total_vote_weight":0,"pending_payout":"48.000 TBD"}]})~"
    }, { // delete_comment_operation
    R"~({"trx_id":"8e3e87e3e1adc6a946973834e4c8b79ee4750585","block":25,"trx_in_block":1,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:01:12","op":{"type":"delete_comment_operation","value":{"author":"edgar0ah","permlink":"permlink1"}},"operation_id":0})~",
    R"~({"trx_id":"8e3e87e3e1adc6a946973834e4c8b79ee4750585","block":25,"trx_in_block":1,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:01:12","op":["delete_comment",{"author":"edgar0ah","permlink":"permlink1"}]})~"
    }, { // ineffective_delete_comment_operation / third and last obsolete operation tested here.
    R"~({"trx_id":"8e3e87e3e1adc6a946973834e4c8b79ee4750585","block":25,"trx_in_block":1,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:01:12","op":{"type":"ineffective_delete_comment_operation","value":{"author":"edgar0ah","permlink":"permlink1"}},"operation_id":0})~",
    R"~({"trx_id":"8e3e87e3e1adc6a946973834e4c8b79ee4750585","block":25,"trx_in_block":1,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:01:12","op":["ineffective_delete_comment",{"author":"edgar0ah","permlink":"permlink1"}]})~"
    }, { // producer_reward_operation
    R"~({"trx_id":"0000000000000000000000000000000000000000","block":25,"trx_in_block":4294967295,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:01:15","op":{"type":"producer_reward_operation","value":{"producer":"dan0ah","vesting_shares":{"amount":"1000","precision":3,"nai":"@@000000021"}}},"operation_id":0})~",
    R"~({"trx_id":"0000000000000000000000000000000000000000","block":25,"trx_in_block":4294967295,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:01:15","op":["producer_reward",{"producer":"dan0ah","vesting_shares":"1.000 TESTS"}]})~"
    } };
    expected_t expected_virtual_operations = { expected_operations[1], expected_operations[3], expected_operations[4] };
    test_get_ops_in_block( *this, expected_operations, expected_virtual_operations, 25 );
  };

  hf13_scenario( check_point_1_tester, check_point_2_tester );

} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_CASE( get_ops_in_block_comment_and_reward )
{ try {

  BOOST_TEST_MESSAGE( "testing get_ops_in_block with comment_and_reward_scenario" );

  // Check virtual operations resulting from scenario's 1st set of actions some blocks later:
  auto check_point_1_tester = [ this ]( uint32_t generate_no_further_than )
  {
    generate_until_irreversible_block( 6 );
    BOOST_REQUIRE( db->head_block_num() <= generate_no_further_than );

    expected_t expected_operations = { { // producer_reward_operation
      R"~({"trx_id":"0000000000000000000000000000000000000000","block":6,"trx_in_block":4294967295,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:00:18","op":{"type":"producer_reward_operation","value":{"producer":"initminer","vesting_shares":{"amount":"8525584098","precision":6,"nai":"@@000000037"}}},"operation_id":0})~",
      R"~({"trx_id":"0000000000000000000000000000000000000000","block":6,"trx_in_block":4294967295,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:00:18","op":["producer_reward",{"producer":"initminer","vesting_shares":"8525.584098 VESTS"}]})~"
    }, { // curation_reward_operation
      R"~({"trx_id":"0000000000000000000000000000000000000000","block":6,"trx_in_block":4294967295,"op_in_trx":2,"virtual_op":true,"timestamp":"2016-01-01T00:00:18","op":{"type":"curation_reward_operation","value":{"curator":"dan0ah","reward":{"amount":"1089380190306","precision":6,"nai":"@@000000037"},"comment_author":"edgar0ah","comment_permlink":"permlink1","payout_must_be_claimed":true}},"operation_id":0})~",
      R"~({"trx_id":"0000000000000000000000000000000000000000","block":6,"trx_in_block":4294967295,"op_in_trx":2,"virtual_op":true,"timestamp":"2016-01-01T00:00:18","op":["curation_reward",{"curator":"dan0ah","reward":"1089380.190306 VESTS","comment_author":"edgar0ah","comment_permlink":"permlink1","payout_must_be_claimed":true}]})~"
    }, { // author_reward_operation
      R"~({"trx_id":"0000000000000000000000000000000000000000","block":6,"trx_in_block":4294967295,"op_in_trx":3,"virtual_op":true,"timestamp":"2016-01-01T00:00:18","op":{"type":"author_reward_operation","value":{"author":"edgar0ah","permlink":"permlink1","hbd_payout":{"amount":"575","precision":3,"nai":"@@000000013"},"hive_payout":{"amount":"0","precision":3,"nai":"@@000000021"},"vesting_payout":{"amount":"544690095153","precision":6,"nai":"@@000000037"},"curators_vesting_payout":{"amount":"1089380190306","precision":6,"nai":"@@000000037"},"payout_must_be_claimed":true}},"operation_id":0})~",
      R"~({"trx_id":"0000000000000000000000000000000000000000","block":6,"trx_in_block":4294967295,"op_in_trx":3,"virtual_op":true,"timestamp":"2016-01-01T00:00:18","op":["author_reward",{"author":"edgar0ah","permlink":"permlink1","hbd_payout":"0.575 TBD","hive_payout":"0.000 TESTS","vesting_payout":"544690.095153 VESTS","curators_vesting_payout":"1089380.190306 VESTS","payout_must_be_claimed":true}]})~"
    }, { // comment_reward_operation
      R"~({"trx_id":"0000000000000000000000000000000000000000","block":6,"trx_in_block":4294967295,"op_in_trx":4,"virtual_op":true,"timestamp":"2016-01-01T00:00:18","op":{"type":"comment_reward_operation","value":{"author":"edgar0ah","permlink":"permlink1","payout":{"amount":"2300","precision":3,"nai":"@@000000013"},"author_rewards":1150,"total_payout_value":{"amount":"1150","precision":3,"nai":"@@000000013"},"curator_payout_value":{"amount":"1150","precision":3,"nai":"@@000000013"},"beneficiary_payout_value":{"amount":"0","precision":3,"nai":"@@000000013"}}},"operation_id":0})~",
      R"~({"trx_id":"0000000000000000000000000000000000000000","block":6,"trx_in_block":4294967295,"op_in_trx":4,"virtual_op":true,"timestamp":"2016-01-01T00:00:18","op":["comment_reward",{"author":"edgar0ah","permlink":"permlink1","payout":"2.300 TBD","author_rewards":1150,"total_payout_value":"1.150 TBD","curator_payout_value":"1.150 TBD","beneficiary_payout_value":"0.000 TBD"}]})~"
    }, { // comment_payout_update_operation
      R"~({"trx_id":"0000000000000000000000000000000000000000","block":6,"trx_in_block":4294967295,"op_in_trx":5,"virtual_op":true,"timestamp":"2016-01-01T00:00:18","op":{"type":"comment_payout_update_operation","value":{"author":"edgar0ah","permlink":"permlink1"}},"operation_id":0})~",
      R"~({"trx_id":"0000000000000000000000000000000000000000","block":6,"trx_in_block":4294967295,"op_in_trx":5,"virtual_op":true,"timestamp":"2016-01-01T00:00:18","op":["comment_payout_update",{"author":"edgar0ah","permlink":"permlink1"}]})~"
    } };
    // Note that all operations of this block are virtual, hence we can reuse the same expected container here.
    test_get_ops_in_block( *this, expected_operations, expected_operations, 6 );
  };

  // Check operations resulting from 2nd set of actions:
  auto check_point_2_tester = [ this ]( uint32_t generate_no_further_than )
  { 
    generate_until_irreversible_block( 28 );
    BOOST_REQUIRE( db->head_block_num() <= generate_no_further_than );

    expected_t expected_operations = { { // claim_reward_balance_operation
      R"~({"trx_id":"69adf1e346c41e81cab0cc3cb3249ed03a9767c4","block":28,"trx_in_block":0,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:01:21","op":{"type":"claim_reward_balance_operation","value":{"account":"edgar0ah","reward_hive":{"amount":"0","precision":3,"nai":"@@000000021"},"reward_hbd":{"amount":"575","precision":3,"nai":"@@000000013"},"reward_vests":{"amount":"80000000","precision":6,"nai":"@@000000037"}}},"operation_id":0})~",
      R"~({"trx_id":"69adf1e346c41e81cab0cc3cb3249ed03a9767c4","block":28,"trx_in_block":0,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:01:21","op":["claim_reward_balance",{"account":"edgar0ah","reward_hive":"0.000 TESTS","reward_hbd":"0.575 TBD","reward_vests":"80.000000 VESTS"}]})~"
    }, { // producer_reward_operation
      R"~({"trx_id":"0000000000000000000000000000000000000000","block":28,"trx_in_block":4294967295,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:01:24","op":{"type":"producer_reward_operation","value":{"producer":"initminer","vesting_shares":{"amount":"7076556368","precision":6,"nai":"@@000000037"}}},"operation_id":0})~",
      R"~({"trx_id":"0000000000000000000000000000000000000000","block":28,"trx_in_block":4294967295,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:01:24","op":["producer_reward",{"producer":"initminer","vesting_shares":"7076.556368 VESTS"}]})~"
    } };
    expected_t expected_virtual_operations = { expected_operations[1] };
    test_get_ops_in_block( *this, expected_operations, expected_virtual_operations, 28 );
  };

  comment_and_reward_scenario( check_point_1_tester, check_point_2_tester );

} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_CASE( get_ops_in_block_convert_and_limit_order )
{ try {

  BOOST_TEST_MESSAGE( "testing get_ops_in_block with convert_and_limit_order_scenario" );

  auto check_point_tester = [ this ]( uint32_t generate_no_further_than )
  {
    generate_until_irreversible_block( 5 );
    BOOST_REQUIRE( db->head_block_num() <= generate_no_further_than );

    expected_t expected_operations = { { // convert_operation
      R"~({"trx_id":"6a79f548e62e1013e6fcbc7442f215cd7879f431","block":5,"trx_in_block":0,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:12","op":{"type":"convert_operation","value":{"owner":"edgar3ah","requestid":0,"amount":{"amount":"11201","precision":3,"nai":"@@000000013"}}},"operation_id":0})~",
      R"~({"trx_id":"6a79f548e62e1013e6fcbc7442f215cd7879f431","block":5,"trx_in_block":0,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:12","op":["convert",{"owner":"edgar3ah","requestid":0,"amount":"11.201 TBD"}]})~"
      }, { // collateralized_convert_operation
      R"~({"trx_id":"9f3d234471e6b053be812e180f8f63a4811f462d","block":5,"trx_in_block":1,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:12","op":{"type":"collateralized_convert_operation","value":{"owner":"carol3ah","requestid":0,"amount":{"amount":"22102","precision":3,"nai":"@@000000021"}}},"operation_id":0})~",
      R"~({"trx_id":"9f3d234471e6b053be812e180f8f63a4811f462d","block":5,"trx_in_block":1,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:12","op":["collateralized_convert",{"owner":"carol3ah","requestid":0,"amount":"22.102 TESTS"}]})~"
      }, { // collateralized_convert_immediate_conversion_operation
      R"~({"trx_id":"9f3d234471e6b053be812e180f8f63a4811f462d","block":5,"trx_in_block":1,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:00:12","op":{"type":"collateralized_convert_immediate_conversion_operation","value":{"owner":"carol3ah","requestid":0,"hbd_out":{"amount":"10524","precision":3,"nai":"@@000000013"}}},"operation_id":0})~",
      R"~({"trx_id":"9f3d234471e6b053be812e180f8f63a4811f462d","block":5,"trx_in_block":1,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:00:12","op":["collateralized_convert_immediate_conversion",{"owner":"carol3ah","requestid":0,"hbd_out":"10.524 TBD"}]})~"
      }, { // limit_order_create_operation
      R"~({"trx_id":"ad19c50dc64931096ca6bd82574f03330c37c7d9","block":5,"trx_in_block":2,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:12","op":{"type":"limit_order_create_operation","value":{"owner":"carol3ah","orderid":1,"amount_to_sell":{"amount":"11400","precision":3,"nai":"@@000000021"},"min_to_receive":{"amount":"11650","precision":3,"nai":"@@000000013"},"fill_or_kill":false,"expiration":"2016-01-29T00:00:12"}},"operation_id":0})~",
      R"~({"trx_id":"ad19c50dc64931096ca6bd82574f03330c37c7d9","block":5,"trx_in_block":2,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:12","op":["limit_order_create",{"owner":"carol3ah","orderid":1,"amount_to_sell":"11.400 TESTS","min_to_receive":"11.650 TBD","fill_or_kill":false,"expiration":"2016-01-29T00:00:12"}]})~"
      }, { // limit_order_create2_operation
      R"~({"trx_id":"5381fe94856170a97821cf6c1a3518d279111d05","block":5,"trx_in_block":3,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:12","op":{"type":"limit_order_create2_operation","value":{"owner":"carol3ah","orderid":2,"amount_to_sell":{"amount":"22075","precision":3,"nai":"@@000000021"},"exchange_rate":{"base":{"amount":"10","precision":3,"nai":"@@000000021"},"quote":{"amount":"10","precision":3,"nai":"@@000000013"}},"fill_or_kill":false,"expiration":"2016-01-29T00:00:12"}},"operation_id":0})~",
      R"~({"trx_id":"5381fe94856170a97821cf6c1a3518d279111d05","block":5,"trx_in_block":3,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:12","op":["limit_order_create2",{"owner":"carol3ah","orderid":2,"amount_to_sell":"22.075 TESTS","exchange_rate":{"base":"0.010 TESTS","quote":"0.010 TBD"},"fill_or_kill":false,"expiration":"2016-01-29T00:00:12"}]})~"
      }, { // limit_order_cancel_operation
      R"~({"trx_id":"8c57b6cc735c077c0f0e41974e3f74dafab89319","block":5,"trx_in_block":4,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:12","op":{"type":"limit_order_cancel_operation","value":{"owner":"carol3ah","orderid":1}},"operation_id":0})~",
      R"~({"trx_id":"8c57b6cc735c077c0f0e41974e3f74dafab89319","block":5,"trx_in_block":4,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:12","op":["limit_order_cancel",{"owner":"carol3ah","orderid":1}]})~"
      }, { // limit_order_cancelled_operation
      R"~({"trx_id":"8c57b6cc735c077c0f0e41974e3f74dafab89319","block":5,"trx_in_block":4,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:00:12","op":{"type":"limit_order_cancelled_operation","value":{"seller":"carol3ah","orderid":1,"amount_back":{"amount":"11400","precision":3,"nai":"@@000000021"}}},"operation_id":0})~",
      R"~({"trx_id":"8c57b6cc735c077c0f0e41974e3f74dafab89319","block":5,"trx_in_block":4,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:00:12","op":["limit_order_cancelled",{"seller":"carol3ah","orderid":1,"amount_back":"11.400 TESTS"}]})~"
      },{ // producer_reward_operation
      R"~({"trx_id":"0000000000000000000000000000000000000000","block":5,"trx_in_block":4294967295,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:00:15","op":{"type":"producer_reward_operation","value":{"producer":"initminer","vesting_shares":{"amount":"8611634248","precision":6,"nai":"@@000000037"}}},"operation_id":0})~",
      R"~({"trx_id":"0000000000000000000000000000000000000000","block":5,"trx_in_block":4294967295,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:00:15","op":["producer_reward",{"producer":"initminer","vesting_shares":"8611.634248 VESTS"}]})~"
      } };
    expected_t expected_virtual_operations = { expected_operations[2], expected_operations[6], expected_operations[7] };
    test_get_ops_in_block( *this, expected_operations, expected_virtual_operations, 5 );

    generate_until_irreversible_block( 1684 );
    BOOST_REQUIRE( db->head_block_num() <= generate_no_further_than );
    // Check virtual operations spawned by the ones obove:
    expected_operations = { { // producer_reward_operation
      R"~({"trx_id":"0000000000000000000000000000000000000000","block":1684,"trx_in_block":4294967295,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T01:24:12","op":{"type":"producer_reward_operation","value":{"producer":"initminer","vesting_shares":{"amount":"125301879876","precision":6,"nai":"@@000000037"}}},"operation_id":0})~",
      R"~({"trx_id":"0000000000000000000000000000000000000000","block":1684,"trx_in_block":4294967295,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T01:24:12","op":["producer_reward",{"producer":"initminer","vesting_shares":"125301.879876 VESTS"}]})~"
      }, { // fill_convert_request_operation
      R"~({"trx_id":"0000000000000000000000000000000000000000","block":1684,"trx_in_block":4294967295,"op_in_trx":2,"virtual_op":true,"timestamp":"2016-01-01T01:24:12","op":{"type":"fill_convert_request_operation","value":{"owner":"edgar3ah","requestid":0,"amount_in":{"amount":"11201","precision":3,"nai":"@@000000013"},"amount_out":{"amount":"11201","precision":3,"nai":"@@000000021"}}},"operation_id":0})~",
      R"~({"trx_id":"0000000000000000000000000000000000000000","block":1684,"trx_in_block":4294967295,"op_in_trx":2,"virtual_op":true,"timestamp":"2016-01-01T01:24:12","op":["fill_convert_request",{"owner":"edgar3ah","requestid":0,"amount_in":"11.201 TBD","amount_out":"11.201 TESTS"}]})~"
      }, { // fill_collateralized_convert_request_operation
      R"~({"trx_id":"0000000000000000000000000000000000000000","block":1684,"trx_in_block":4294967295,"op_in_trx":3,"virtual_op":true,"timestamp":"2016-01-01T01:24:12","op":{"type":"fill_collateralized_convert_request_operation","value":{"owner":"carol3ah","requestid":0,"amount_in":{"amount":"11050","precision":3,"nai":"@@000000021"},"amount_out":{"amount":"10524","precision":3,"nai":"@@000000013"},"excess_collateral":{"amount":"11052","precision":3,"nai":"@@000000021"}}},"operation_id":0})~",
      R"~({"trx_id":"0000000000000000000000000000000000000000","block":1684,"trx_in_block":4294967295,"op_in_trx":3,"virtual_op":true,"timestamp":"2016-01-01T01:24:12","op":["fill_collateralized_convert_request",{"owner":"carol3ah","requestid":0,"amount_in":"11.050 TESTS","amount_out":"10.524 TBD","excess_collateral":"11.052 TESTS"}]})~"
      } };
    // Note that all operations of this block are virtual, hence we can reuse the same expected container here.
    test_get_ops_in_block( *this, expected_operations, expected_operations, 1684 );
  };

  convert_and_limit_order_scenario( check_point_tester );

} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_CASE( get_ops_in_block_vesting )
{ try {

  BOOST_TEST_MESSAGE( "testing get_ops_in_block with vesting_scenario" );

  auto check_point_tester1 = [ this ]( uint32_t generate_no_further_than )
  {
    generate_until_irreversible_block( 5 );
    BOOST_REQUIRE( db->head_block_num() <= generate_no_further_than );

    // TODO Supplement the patterns here
    /*expected_t expected_operations = { { // transfer_to_vesting_operation
      }, { // transfer_to_vesting_completed_operation
      }, { // set_withdraw_vesting_route_operation
      }, { // delegate_vesting_shares_operation
      }, { // withdraw_vesting_operation
      }, { // producer_reward_operation
      } };
    expected_t expected_virtual_operations = { expected_operations[1], expected_operations[5] };
    test_get_ops_in_block( *this, expected_operations, expected_virtual_operations, 5 );*/
  };

  auto check_point_tester2 = [ this ]( uint32_t generate_no_further_than )
  {
    generate_until_irreversible_block( 31 );
    BOOST_REQUIRE( db->head_block_num() <= generate_no_further_than );

    // TODO Supplement the patterns here
    /*expected_t expected_operations = { { // producer_reward_operation
      }, { // fill_vesting_withdraw_operation
      }, { // fill_vesting_withdraw_operation
      } };
    // Note that all operations of this block are virtual, hence we can reuse the same expected container here.
    test_get_ops_in_block( *this, expected_operations, expected_operations, 28 );*/

    expected_t expected_operations = { { // return_vesting_delegation_operation
      R"~({"trx_id":"0000000000000000000000000000000000000000","block":31,"trx_in_block":4294967295,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:01:33","op":{"type":"return_vesting_delegation_operation","value":{"account":"alice4ah","vesting_shares":{"amount":"1","precision":6,"nai":"@@000000037"}}},"operation_id":0})~",
      R"~({"trx_id":"0000000000000000000000000000000000000000","block":31,"trx_in_block":4294967295,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:01:33","op":["return_vesting_delegation",{"account":"alice4ah","vesting_shares":"0.000001 VESTS"}]})~"
      }, { // producer_reward_operation
      R"~({"trx_id":"0000000000000000000000000000000000000000","block":31,"trx_in_block":4294967295,"op_in_trx":2,"virtual_op":true,"timestamp":"2016-01-01T00:01:33","op":{"type":"producer_reward_operation","value":{"producer":"initminer","vesting_shares":{"amount":"204708136662","precision":6,"nai":"@@000000037"}}},"operation_id":0})~",
      R"~({"trx_id":"0000000000000000000000000000000000000000","block":31,"trx_in_block":4294967295,"op_in_trx":2,"virtual_op":true,"timestamp":"2016-01-01T00:01:33","op":["producer_reward",{"producer":"initminer","vesting_shares":"204708.136662 VESTS"}]})~"
      } };
    // Note that all operations of this block are virtual, hence we can reuse the same expected container here.
    test_get_ops_in_block( *this, expected_operations, expected_operations, 31 );
  };

  vesting_scenario( check_point_tester1, check_point_tester2 );

} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_SUITE_END() // condenser_get_ops_in_block_tests

BOOST_FIXTURE_TEST_SUITE( condenser_get_transaction_tests, condenser_api_fixture );

// TODO: Improve by comparing get_transaction results against patterns provided as additional argument.
void test_get_transaction( const condenser_api_fixture& caf, uint32_t block_num )
{
  auto full_block_ptr = caf.db->fetch_block_by_number( block_num );
  const signed_block& block = full_block_ptr->get_block();

  for( const auto& trx : block.transactions )
  {
    const auto& trx_id = trx.id();
    BOOST_REQUIRE( trx_id != hive::protocol::transaction_id_type() );

      // Call condenser get_transaction and verify results with result of account history variant.
      const auto tx_hash = trx_id.str();
      const auto result = caf.condenser_api->get_transaction( condenser_api::get_transaction_args(1, fc::variant(tx_hash)) );
      const condenser_api::annotated_signed_transaction op_tx = result.value;
      ilog("operation transaction is ${tx}", ("tx", op_tx));
      const account_history::annotated_signed_transaction ah_op_tx = 
        caf.account_history_api->get_transaction( {tx_hash, false /*include_reversible*/} );
      ilog("operation transaction is ${tx}", ("tx", ah_op_tx));
      BOOST_REQUIRE_EQUAL( op_tx.transaction_id, ah_op_tx.transaction_id );
      BOOST_REQUIRE_EQUAL( op_tx.block_num, ah_op_tx.block_num );
      BOOST_REQUIRE_EQUAL( op_tx.transaction_num, ah_op_tx.transaction_num );

      // Do additional checks of condenser variant
      // Too few arguments
      BOOST_REQUIRE_THROW( caf.condenser_api->get_transaction( condenser_api::get_transaction_args() ), fc::assert_exception );
      // Too many arguments
      BOOST_REQUIRE_THROW( caf.condenser_api->get_transaction( condenser_api::get_transaction_args(2, fc::variant(tx_hash)) ), fc::assert_exception );
    }
}

BOOST_AUTO_TEST_CASE( get_transaction_hf1 )
{ try {

  BOOST_TEST_MESSAGE( "testing get_transaction on operations of HF1" );

  auto check_point_tester = [ this ]( uint32_t generate_no_further_than )
  {
    generate_until_irreversible_block( 2 );
    BOOST_REQUIRE( db->head_block_num() <= generate_no_further_than );

    test_get_transaction( *this, 2 ); // <- TODO: Enhance with patterns

    // In block 21 maximum block size is being changed:
    generate_until_irreversible_block( 21 );
    BOOST_REQUIRE( db->head_block_num() <= generate_no_further_than );

    test_get_transaction( *this, 21 ); // <- TODO: Enhance with patterns
  };

  hf1_scenario( check_point_tester );

} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_CASE( get_transaction_hf12 )
{ try {

  BOOST_TEST_MESSAGE( "testing get_transaction with hf12_scenario" );

  auto check_point_tester = [ this ]( uint32_t generate_no_further_than )
  {
    generate_until_irreversible_block( 3 );
    BOOST_REQUIRE( db->head_block_num() <= generate_no_further_than );

    test_get_transaction( *this, 3 ); // <- TODO: Enhance with patterns
  };

  hf12_scenario( check_point_tester );

} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_CASE( get_transaction_hf13 )
{ try {

  BOOST_TEST_MESSAGE( "testing get_transaction with hf13_scenario" );

  auto check_point_1_tester = [ this ]( uint32_t generate_no_further_than )
  {
    generate_until_irreversible_block( 3 );
    BOOST_REQUIRE( db->head_block_num() <= generate_no_further_than );

    test_get_transaction( *this, 3 ); // <- TODO: Enhance with patterns
  };

  auto check_point_2_tester = [ this ]( uint32_t generate_no_further_than )
  {
    generate_until_irreversible_block( 25 );
    BOOST_REQUIRE( db->head_block_num() <= generate_no_further_than );

    test_get_transaction( *this, 25 ); // <- TODO: Enhance with patterns
  };

  hf13_scenario( check_point_1_tester, check_point_2_tester );

} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_CASE( get_transaction_comment_and_reward )
{ try {

  BOOST_TEST_MESSAGE( "testing get_transaction with comment_and_reward_scenario" );

  // Check virtual operations resulting from scenario's 1st set of actions some blocks later:
  auto check_point_1_tester = [ this ]( uint32_t generate_no_further_than )
  {
    generate_until_irreversible_block( 6 );
    BOOST_REQUIRE( db->head_block_num() <= generate_no_further_than );

    test_get_transaction( *this, 6 ); // <- TODO: Enhance with patterns
  };

  // Check operations resulting from 2nd set of actions:
  auto check_point_2_tester = [ this ]( uint32_t generate_no_further_than )
  { 
    generate_until_irreversible_block( 28 );
    BOOST_REQUIRE( db->head_block_num() <= generate_no_further_than );

    test_get_transaction( *this, 25 ); // <- TODO: Enhance with patterns
  };

  comment_and_reward_scenario( check_point_1_tester, check_point_2_tester );

} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_CASE( get_transaction_convert_and_limit_order )
{ try {

  BOOST_TEST_MESSAGE( "testing get_transaction with convert_and_limit_order_scenario" );

  auto check_point_tester = [ this ]( uint32_t generate_no_further_than )
  {
    generate_until_irreversible_block( 5 );
    BOOST_REQUIRE( db->head_block_num() <= generate_no_further_than );

    test_get_transaction( *this, 5 ); // <- TODO: Enhance with patterns

    generate_until_irreversible_block( 1684 );
    BOOST_REQUIRE( db->head_block_num() <= generate_no_further_than );

    test_get_transaction( *this, 1684 ); // <- TODO: Enhance with patterns
  };

  convert_and_limit_order_scenario( check_point_tester );

} FC_LOG_AND_RETHROW() }

// TODO Create get_transaction_vesting test here.

BOOST_AUTO_TEST_SUITE_END() // condenser_get_transaction_tests

BOOST_FIXTURE_TEST_SUITE( condenser_get_account_history_tests, condenser_api_fixture );

void test_get_account_history( const condenser_api_fixture& caf, const std::vector< std:: string >& account_names, const std::vector< expected_t >& expected_operations )
{
  // For each requested account ...
  BOOST_REQUIRE( expected_operations.size() == account_names.size() );
  for( size_t account_index = 0; account_index < account_names.size(); ++account_index)
  {
    const auto& account_name = account_names[ account_index ];
    const auto& expected_for_account = expected_operations[ account_index ];

    auto ah1 = caf.account_history_api->get_account_history( {account_name, 100 /*start*/, 100 /*limit*/, false /*include_reversible*/ /*, filter_low, filter_high*/ } );
    auto ah2 = caf.condenser_api->get_account_history( condenser_api::get_account_history_args( {account_name, 100 /*start*/, 100 /*limit*/ /*, filter_low, filter_high*/} ) );
    BOOST_REQUIRE( ah1.history.size() == ah2.size() );
    BOOST_REQUIRE( expected_for_account.size() == ah2.size() );
    ilog( "${n} operation(s) in account ${account} history", ("n", ah2.size())("account", account_name) );

    // For each event (operation) in account history ...
    auto it_ah = ah1.history.begin();
    auto it_cn = ah2.begin();
    for( size_t op_index = 0; it_cn != ah2.end(); ++it_ah, ++it_cn, ++op_index )
    {
      ilog("ah op: ${op}", ("op", *it_ah));
      ilog("cn op: ${op}", ("op", *it_cn));

      // Compare operations in their serialized form with expected patterns:
      const auto expected = expected_for_account[ op_index ];
      BOOST_REQUIRE_EQUAL( expected.first, fc::json::to_string(*it_ah) );
      BOOST_REQUIRE_EQUAL( expected.second, fc::json::to_string(*it_cn) );
    }

    // Do additional checks of condenser variant
    // Too few arguments
    BOOST_REQUIRE_THROW( caf.condenser_api->get_account_history( condenser_api::get_account_history_args( {account_name, 100 /*start*/ } ) ), fc::assert_exception );
    // Too many arguments
    BOOST_REQUIRE_THROW( caf.condenser_api->get_account_history( condenser_api::get_account_history_args( 
      {account_name, 100 /*start*/, 100 /*limit*/, 50 /*filter_low*/, 200 /*filter_high*/, 100 /*redundant*/ } ) ), fc::assert_exception );
  }
}

// TODO create get_account_history_hf1 test here
// TODO create get_account_history_hf12 test here
// TODO Create get_account_history_hf13 here
// TODO create get_account_history_comment_and_reward test here
// TODO create get_account_history_vesting test here

BOOST_AUTO_TEST_CASE( get_account_history_convert_and_limit_order )
{ try {

  BOOST_TEST_MESSAGE( "testing get_account_history with convert_and_limit_order_scenario" );

  auto check_point_tester = [ this ]( uint32_t generate_no_further_than )
  {
    generate_until_irreversible_block( 1684 );
    BOOST_REQUIRE( db->head_block_num() <= generate_no_further_than );

    expected_t expected_carol3ah_history = { {
      R"~([0,{"trx_id":"99b3d543d8dbd7da66dc4939cea81138e9171073","block":3,"trx_in_block":2,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:06","op":{"type":"account_create_operation","value":{"fee":{"amount":"0","precision":3,"nai":"@@000000021"},"creator":"initminer","new_account_name":"carol3ah","owner":{"weight_threshold":1,"account_auths":[],"key_auths":[["TST7jcq47bH93zcuTCdP394BYJLrhGWzyGwqkukB46zyFsghQPeoz",1]]},"active":{"weight_threshold":1,"account_auths":[],"key_auths":[["TST7jcq47bH93zcuTCdP394BYJLrhGWzyGwqkukB46zyFsghQPeoz",1]]},"posting":{"weight_threshold":1,"account_auths":[],"key_auths":[["TST5kavbaHAVwb9mANYyUEubtGybsJ4zySnVrpdmpDM8pDRhKzyN3",1]]},"memo_key":"TST7jcq47bH93zcuTCdP394BYJLrhGWzyGwqkukB46zyFsghQPeoz","json_metadata":""}},"operation_id":0}])~",
      R"~([0,{"trx_id":"99b3d543d8dbd7da66dc4939cea81138e9171073","block":3,"trx_in_block":2,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:06","op":["account_create",{"fee":"0.000 TESTS","creator":"initminer","new_account_name":"carol3ah","owner":{"weight_threshold":1,"account_auths":[],"key_auths":[["TST7jcq47bH93zcuTCdP394BYJLrhGWzyGwqkukB46zyFsghQPeoz",1]]},"active":{"weight_threshold":1,"account_auths":[],"key_auths":[["TST7jcq47bH93zcuTCdP394BYJLrhGWzyGwqkukB46zyFsghQPeoz",1]]},"posting":{"weight_threshold":1,"account_auths":[],"key_auths":[["TST5kavbaHAVwb9mANYyUEubtGybsJ4zySnVrpdmpDM8pDRhKzyN3",1]]},"memo_key":"TST7jcq47bH93zcuTCdP394BYJLrhGWzyGwqkukB46zyFsghQPeoz","json_metadata":""}]}])~"
      }, {
      R"~([1,{"trx_id":"99b3d543d8dbd7da66dc4939cea81138e9171073","block":3,"trx_in_block":2,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:00:06","op":{"type":"account_created_operation","value":{"new_account_name":"carol3ah","creator":"initminer","initial_vesting_shares":{"amount":"0","precision":6,"nai":"@@000000037"},"initial_delegation":{"amount":"0","precision":6,"nai":"@@000000037"}}},"operation_id":0}])~",
      R"~([1,{"trx_id":"99b3d543d8dbd7da66dc4939cea81138e9171073","block":3,"trx_in_block":2,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:00:06","op":["account_created",{"new_account_name":"carol3ah","creator":"initminer","initial_vesting_shares":"0.000000 VESTS","initial_delegation":"0.000000 VESTS"}]}])~"
      }, {
      R"~([2,{"trx_id":"eae9a5104a476a51007f1e99f787df120cc6def2","block":3,"trx_in_block":3,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:06","op":{"type":"transfer_to_vesting_operation","value":{"from":"initminer","to":"carol3ah","amount":{"amount":"100","precision":3,"nai":"@@000000021"}}},"operation_id":0}])~",
      R"~([2,{"trx_id":"eae9a5104a476a51007f1e99f787df120cc6def2","block":3,"trx_in_block":3,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:06","op":["transfer_to_vesting",{"from":"initminer","to":"carol3ah","amount":"0.100 TESTS"}]}])~"
      }, {
      R"~([3,{"trx_id":"eae9a5104a476a51007f1e99f787df120cc6def2","block":3,"trx_in_block":3,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:00:06","op":{"type":"transfer_to_vesting_completed_operation","value":{"from_account":"initminer","to_account":"carol3ah","hive_vested":{"amount":"100","precision":3,"nai":"@@000000021"},"vesting_shares_received":{"amount":"98716683119","precision":6,"nai":"@@000000037"}}},"operation_id":0}])~",
      R"~([3,{"trx_id":"eae9a5104a476a51007f1e99f787df120cc6def2","block":3,"trx_in_block":3,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:00:06","op":["transfer_to_vesting_completed",{"from_account":"initminer","to_account":"carol3ah","hive_vested":"0.100 TESTS","vesting_shares_received":"98716.683119 VESTS"}]}])~"
      }, {
      R"~([4,{"trx_id":"9f3d234471e6b053be812e180f8f63a4811f462d","block":5,"trx_in_block":1,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:12","op":{"type":"collateralized_convert_operation","value":{"owner":"carol3ah","requestid":0,"amount":{"amount":"22102","precision":3,"nai":"@@000000021"}}},"operation_id":0}])~",
      R"~([4,{"trx_id":"9f3d234471e6b053be812e180f8f63a4811f462d","block":5,"trx_in_block":1,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:12","op":["collateralized_convert",{"owner":"carol3ah","requestid":0,"amount":"22.102 TESTS"}]}])~"
      }, {
      R"~([5,{"trx_id":"9f3d234471e6b053be812e180f8f63a4811f462d","block":5,"trx_in_block":1,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:00:12","op":{"type":"collateralized_convert_immediate_conversion_operation","value":{"owner":"carol3ah","requestid":0,"hbd_out":{"amount":"10524","precision":3,"nai":"@@000000013"}}},"operation_id":0}])~",
      R"~([5,{"trx_id":"9f3d234471e6b053be812e180f8f63a4811f462d","block":5,"trx_in_block":1,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:00:12","op":["collateralized_convert_immediate_conversion",{"owner":"carol3ah","requestid":0,"hbd_out":"10.524 TBD"}]}])~"
      }, {
      R"~([6,{"trx_id":"ad19c50dc64931096ca6bd82574f03330c37c7d9","block":5,"trx_in_block":2,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:12","op":{"type":"limit_order_create_operation","value":{"owner":"carol3ah","orderid":1,"amount_to_sell":{"amount":"11400","precision":3,"nai":"@@000000021"},"min_to_receive":{"amount":"11650","precision":3,"nai":"@@000000013"},"fill_or_kill":false,"expiration":"2016-01-29T00:00:12"}},"operation_id":0}])~",
      R"~([6,{"trx_id":"ad19c50dc64931096ca6bd82574f03330c37c7d9","block":5,"trx_in_block":2,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:12","op":["limit_order_create",{"owner":"carol3ah","orderid":1,"amount_to_sell":"11.400 TESTS","min_to_receive":"11.650 TBD","fill_or_kill":false,"expiration":"2016-01-29T00:00:12"}]}])~"
      }, {
      R"~([7,{"trx_id":"5381fe94856170a97821cf6c1a3518d279111d05","block":5,"trx_in_block":3,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:12","op":{"type":"limit_order_create2_operation","value":{"owner":"carol3ah","orderid":2,"amount_to_sell":{"amount":"22075","precision":3,"nai":"@@000000021"},"exchange_rate":{"base":{"amount":"10","precision":3,"nai":"@@000000021"},"quote":{"amount":"10","precision":3,"nai":"@@000000013"}},"fill_or_kill":false,"expiration":"2016-01-29T00:00:12"}},"operation_id":0}])~",
      R"~([7,{"trx_id":"5381fe94856170a97821cf6c1a3518d279111d05","block":5,"trx_in_block":3,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:12","op":["limit_order_create2",{"owner":"carol3ah","orderid":2,"amount_to_sell":"22.075 TESTS","exchange_rate":{"base":"0.010 TESTS","quote":"0.010 TBD"},"fill_or_kill":false,"expiration":"2016-01-29T00:00:12"}]}])~"
      }, {
      R"~([8,{"trx_id":"8c57b6cc735c077c0f0e41974e3f74dafab89319","block":5,"trx_in_block":4,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:12","op":{"type":"limit_order_cancel_operation","value":{"owner":"carol3ah","orderid":1}},"operation_id":0}])~",
      R"~([8,{"trx_id":"8c57b6cc735c077c0f0e41974e3f74dafab89319","block":5,"trx_in_block":4,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:12","op":["limit_order_cancel",{"owner":"carol3ah","orderid":1}]}])~"
      }, {
      R"~([9,{"trx_id":"8c57b6cc735c077c0f0e41974e3f74dafab89319","block":5,"trx_in_block":4,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:00:12","op":{"type":"limit_order_cancelled_operation","value":{"seller":"carol3ah","orderid":1,"amount_back":{"amount":"11400","precision":3,"nai":"@@000000021"}}},"operation_id":0}])~",
      R"~([9,{"trx_id":"8c57b6cc735c077c0f0e41974e3f74dafab89319","block":5,"trx_in_block":4,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:00:12","op":["limit_order_cancelled",{"seller":"carol3ah","orderid":1,"amount_back":"11.400 TESTS"}]}])~"
      }, {
      R"~([10,{"trx_id":"0000000000000000000000000000000000000000","block":1684,"trx_in_block":4294967295,"op_in_trx":3,"virtual_op":true,"timestamp":"2016-01-01T01:24:12","op":{"type":"fill_collateralized_convert_request_operation","value":{"owner":"carol3ah","requestid":0,"amount_in":{"amount":"11050","precision":3,"nai":"@@000000021"},"amount_out":{"amount":"10524","precision":3,"nai":"@@000000013"},"excess_collateral":{"amount":"11052","precision":3,"nai":"@@000000021"}}},"operation_id":0}])~",
      R"~([10,{"trx_id":"0000000000000000000000000000000000000000","block":1684,"trx_in_block":4294967295,"op_in_trx":3,"virtual_op":true,"timestamp":"2016-01-01T01:24:12","op":["fill_collateralized_convert_request",{"owner":"carol3ah","requestid":0,"amount_in":"11.050 TESTS","amount_out":"10.524 TBD","excess_collateral":"11.052 TESTS"}]}])~"
      } };
    expected_t expected_edgar3ah_history = { {
      R"~([0,{"trx_id":"7a63597b9b1d07c6be6a5cb906c7477be31b140c","block":3,"trx_in_block":0,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:06","op":{"type":"account_create_operation","value":{"fee":{"amount":"0","precision":3,"nai":"@@000000021"},"creator":"initminer","new_account_name":"edgar3ah","owner":{"weight_threshold":1,"account_auths":[],"key_auths":[["TST5WY8U7VmTGmJuHeEJBtXrZwDfwVhGMwgGPh5Rrrskw3XmhSDgs",1]]},"active":{"weight_threshold":1,"account_auths":[],"key_auths":[["TST5WY8U7VmTGmJuHeEJBtXrZwDfwVhGMwgGPh5Rrrskw3XmhSDgs",1]]},"posting":{"weight_threshold":1,"account_auths":[],"key_auths":[["TST5EUgu1BPGi9mPmFhfjELAeZvwdRMdPuKRuPE9CXGbpX9TgzDA5",1]]},"memo_key":"TST5WY8U7VmTGmJuHeEJBtXrZwDfwVhGMwgGPh5Rrrskw3XmhSDgs","json_metadata":""}},"operation_id":0}])~",
      R"~([0,{"trx_id":"7a63597b9b1d07c6be6a5cb906c7477be31b140c","block":3,"trx_in_block":0,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:06","op":["account_create",{"fee":"0.000 TESTS","creator":"initminer","new_account_name":"edgar3ah","owner":{"weight_threshold":1,"account_auths":[],"key_auths":[["TST5WY8U7VmTGmJuHeEJBtXrZwDfwVhGMwgGPh5Rrrskw3XmhSDgs",1]]},"active":{"weight_threshold":1,"account_auths":[],"key_auths":[["TST5WY8U7VmTGmJuHeEJBtXrZwDfwVhGMwgGPh5Rrrskw3XmhSDgs",1]]},"posting":{"weight_threshold":1,"account_auths":[],"key_auths":[["TST5EUgu1BPGi9mPmFhfjELAeZvwdRMdPuKRuPE9CXGbpX9TgzDA5",1]]},"memo_key":"TST5WY8U7VmTGmJuHeEJBtXrZwDfwVhGMwgGPh5Rrrskw3XmhSDgs","json_metadata":""}]}])~"
      }, {
      R"~([1,{"trx_id":"7a63597b9b1d07c6be6a5cb906c7477be31b140c","block":3,"trx_in_block":0,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:00:06","op":{"type":"account_created_operation","value":{"new_account_name":"edgar3ah","creator":"initminer","initial_vesting_shares":{"amount":"0","precision":6,"nai":"@@000000037"},"initial_delegation":{"amount":"0","precision":6,"nai":"@@000000037"}}},"operation_id":0}])~",
      R"~([1,{"trx_id":"7a63597b9b1d07c6be6a5cb906c7477be31b140c","block":3,"trx_in_block":0,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:00:06","op":["account_created",{"new_account_name":"edgar3ah","creator":"initminer","initial_vesting_shares":"0.000000 VESTS","initial_delegation":"0.000000 VESTS"}]}])~"
      }, {
      R"~([2,{"trx_id":"98e8e0aa2b7c3422360f79285c183935df1d14ea","block":3,"trx_in_block":1,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:06","op":{"type":"transfer_to_vesting_operation","value":{"from":"initminer","to":"edgar3ah","amount":{"amount":"100","precision":3,"nai":"@@000000021"}}},"operation_id":0}])~",
      R"~([2,{"trx_id":"98e8e0aa2b7c3422360f79285c183935df1d14ea","block":3,"trx_in_block":1,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:06","op":["transfer_to_vesting",{"from":"initminer","to":"edgar3ah","amount":"0.100 TESTS"}]}])~"
      }, {
      R"~([3,{"trx_id":"98e8e0aa2b7c3422360f79285c183935df1d14ea","block":3,"trx_in_block":1,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:00:06","op":{"type":"transfer_to_vesting_completed_operation","value":{"from_account":"initminer","to_account":"edgar3ah","hive_vested":{"amount":"100","precision":3,"nai":"@@000000021"},"vesting_shares_received":{"amount":"98716683119","precision":6,"nai":"@@000000037"}}},"operation_id":0}])~",
      R"~([3,{"trx_id":"98e8e0aa2b7c3422360f79285c183935df1d14ea","block":3,"trx_in_block":1,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:00:06","op":["transfer_to_vesting_completed",{"from_account":"initminer","to_account":"edgar3ah","hive_vested":"0.100 TESTS","vesting_shares_received":"98716.683119 VESTS"}]}])~"
      }, {
      R"~([4,{"trx_id":"6a79f548e62e1013e6fcbc7442f215cd7879f431","block":5,"trx_in_block":0,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:12","op":{"type":"convert_operation","value":{"owner":"edgar3ah","requestid":0,"amount":{"amount":"11201","precision":3,"nai":"@@000000013"}}},"operation_id":0}])~",
      R"~([4,{"trx_id":"6a79f548e62e1013e6fcbc7442f215cd7879f431","block":5,"trx_in_block":0,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:12","op":["convert",{"owner":"edgar3ah","requestid":0,"amount":"11.201 TBD"}]}])~"
      }, {
      R"~([5,{"trx_id":"0000000000000000000000000000000000000000","block":1684,"trx_in_block":4294967295,"op_in_trx":2,"virtual_op":true,"timestamp":"2016-01-01T01:24:12","op":{"type":"fill_convert_request_operation","value":{"owner":"edgar3ah","requestid":0,"amount_in":{"amount":"11201","precision":3,"nai":"@@000000013"},"amount_out":{"amount":"11201","precision":3,"nai":"@@000000021"}}},"operation_id":0}])~",
      R"~([5,{"trx_id":"0000000000000000000000000000000000000000","block":1684,"trx_in_block":4294967295,"op_in_trx":2,"virtual_op":true,"timestamp":"2016-01-01T01:24:12","op":["fill_convert_request",{"owner":"edgar3ah","requestid":0,"amount_in":"11.201 TBD","amount_out":"11.201 TESTS"}]}])~"
      } };
    test_get_account_history( *this, { "carol3ah", "edgar3ah" }, { expected_carol3ah_history, expected_edgar3ah_history } );
  };

  convert_and_limit_order_scenario( check_point_tester );

} FC_LOG_AND_RETHROW() }
  
BOOST_AUTO_TEST_CASE( account_history_by_condenser_test ) // To be split into scenarios / triple tests
{ try {

  BOOST_TEST_MESSAGE( "get_ops_in_block / get_transaction test" );

  // The container for the kinds of operations that we expect to be found in blocks.
  // We'll use it to be sure that all kind of operations have been used during testing.
  expected_t expected_operations;

  db->set_hardfork( HIVE_HARDFORK_1_27 );
  generate_block();
  
  /*witness_create( "carol0ah", carol0ah_private_key, "foo.bar", carol0ah_private_key.get_public_key(), 1000 );
  expected_operations.insert( { OP_TAG(witness_update_operation), fc::optional< expected_operation_result_t >() } );

  witness_feed_publish( "carol0ah", price( ASSET( "1.000 TBD" ), ASSET( "1.000 TESTS" ) ), carol0ah_private_key );
  expected_operations.insert( { OP_TAG(feed_publish_operation), fc::optional< expected_operation_result_t >() } );
  
  // witness_block_approve_operation - never appears in block (see its evaluator)
  
  proxy( "edgar0ah", "dan0ah" );
  expected_operations.insert( { OP_TAG(account_witness_proxy_operation), fc::optional< expected_operation_result_t >() } );

  witness_vote( "dan0ah", "carol0ah", dan0ah_private_key );
  expected_operations.insert( { OP_TAG(account_witness_vote_operation), fc::optional< expected_operation_result_t >() } );
  
  // Note that we don't use existing database_fixture::set_witness_props function below,
  // because we don't want "uncontrolled" block generation that happens there. We only
  // need the operation in block, so we can do our test on it.
  // witness_set_properties_operation
  fc::flat_map< std::string, std::vector<char> > props;
  props[ "hbd_interest_rate" ] = fc::raw::pack_to_vector( 0 );
  props["key"] = fc::raw::pack_to_vector( carol0ah_private_key.get_public_key() );
  witness_set_properties_operation op;
  op.owner = "carol0ah";
  op.props = props;
  push_transaction( op, carol0ah_private_key );
  expected_operations.insert( { OP_TAG(witness_set_properties_operation), fc::optional< expected_operation_result_t >() } );

  escrow_transfer( "carol0ah", "dan0ah", "edgar0ah", ASSET( "0.071 TESTS" ), ASSET( "0.000 TBD" ), ASSET( "0.001 TESTS" ), "",
                   fc::seconds( HIVE_BLOCK_INTERVAL * 10 ), fc::seconds( HIVE_BLOCK_INTERVAL * 20 ), carol0ah_private_key );
  expected_operations.insert( { OP_TAG(escrow_transfer_operation), fc::optional< expected_operation_result_t >() } );

  escrow_approve( "carol0ah", "dan0ah", "edgar0ah", "edgar0ah", edgar0ah_private_key );
  expected_operations.insert( { OP_TAG(escrow_approve_operation), fc::optional< expected_operation_result_t >() } );

  escrow_approve( "carol0ah", "dan0ah", "edgar0ah", "dan0ah", dan0ah_private_key );
  expected_operations.insert( { OP_TAG(escrow_approved_operation), fc::optional< expected_operation_result_t >() } );

  escrow_release( "carol0ah", "dan0ah", "edgar0ah", "carol0ah", "dan0ah", ASSET( "0.013 TESTS" ), ASSET( "0.000 TBD" ), carol0ah_private_key );
  expected_operations.insert( { OP_TAG(escrow_release_operation), fc::optional< expected_operation_result_t >() } );

  escrow_dispute( "carol0ah", "dan0ah", "edgar0ah", "dan0ah", dan0ah_private_key );
  expected_operations.insert( { OP_TAG(escrow_dispute_operation), fc::optional< expected_operation_result_t >() } );

  transfer_to_savings( "carol0ah", "carol0ah", ASSET( "0.009 TESTS" ), "ah savings", carol0ah_private_key );
  expected_operations.insert( { OP_TAG(transfer_to_savings_operation), fc::optional< expected_operation_result_t >() } );
  
  transfer_from_savings( "carol0ah", "carol0ah", ASSET( "0.006 TESTS" ), 0, carol0ah_private_key );
  expected_operations.insert( { OP_TAG(transfer_from_savings_operation), fc::optional< expected_operation_result_t >() } );
  
  cancel_transfer_from_savings( "carol0ah", 0, carol0ah_private_key );
  expected_operations.insert( { OP_TAG(cancel_transfer_from_savings_operation), fc::optional< expected_operation_result_t >() } );

  fund( "carol0ah", ASSET( "800.000 TBD" ) );
  dhf_database_fixture::create_proposal_data cpd(db->head_block_time());
  cpd.end_date = cpd.start_date + fc::days( 2 );
*///  int64_t proposal_id = 
//    create_proposal( "carol0ah", "dan0ah", cpd.start_date, cpd.end_date, cpd.daily_pay, carol0ah_private_key, false/*with_block_generation*/ );
/*  const proposal_object* proposal = find_proposal( proposal_id );
  BOOST_REQUIRE_NE( proposal, nullptr );
  expected_operations.insert( { OP_TAG(create_proposal_operation), fc::optional< expected_operation_result_t >() } );
  expected_operations.insert( { OP_TAG(proposal_fee_operation), fc::optional< expected_operation_result_t >() } );
  expected_operations.insert( { OP_TAG(dhf_funding_operation), fc::optional< expected_operation_result_t >() } );

  update_proposal( proposal_id, "carol0ah", asset( 80, HBD_SYMBOL ), "new subject", proposal->permlink, carol0ah_private_key);
  expected_operations.insert( { OP_TAG(update_proposal_operation), fc::optional< expected_operation_result_t >() } );
*/
//  vote_proposal( "edgar0ah", { proposal_id }, true/*approve*/, edgar0ah_private_key);
/*  expected_operations.insert( { OP_TAG(update_proposal_votes_operation), fc::optional< expected_operation_result_t >() } );

  remove_proposal( "carol0ah", { proposal_id }, carol0ah_private_key );
  expected_operations.insert( { OP_TAG(remove_proposal_operation), fc::optional< expected_operation_result_t >() } );

  claim_account( "edgar0ah", ASSET( "0.000 TESTS" ), edgar0ah_private_key );
  expected_operations.insert( { OP_TAG(claim_account_operation), fc::optional< expected_operation_result_t >() } );

  PREP_ACTOR( bob0ah )
  create_claimed_account( "edgar0ah", "bob0ah", bob0ah_public_key, bob0ah_post_key.get_public_key(), "", edgar0ah_private_key );
  expected_operations.insert( { OP_TAG(create_claimed_account_operation), fc::optional< expected_operation_result_t >() } );

  vest( HIVE_INIT_MINER_NAME, "bob0ah", ASSET( "1000.000 TESTS" ) );

  change_recovery_account( "bob0ah", HIVE_INIT_MINER_NAME, bob0ah_private_key );
  expected_operations.insert( { OP_TAG(change_recovery_account_operation), fc::optional< expected_operation_result_t >() } );

  account_update( "bob0ah", bob0ah_private_key.get_public_key(), "{"success":true}",
                  authority(1, carol0ah_public_key,1), fc::optional<authority>(), fc::optional<authority>(), bob0ah_private_key );
  expected_operations.insert( { OP_TAG(account_update_operation), fc::optional< expected_operation_result_t >() } );

  request_account_recovery( "edgar0ah", "bob0ah", authority( 1, edgar0ah_private_key.get_public_key(), 1 ), edgar0ah_private_key );
  expected_operations.insert( { OP_TAG(request_account_recovery_operation), fc::optional< expected_operation_result_t >() } );

  recover_account( "bob0ah", edgar0ah_private_key, bob0ah_private_key );
  expected_operations.insert( { OP_TAG(recover_account_operation), fc::optional< expected_operation_result_t >() } );

  push_custom_operation( { "carol0ah" }, 7, { 'D', 'A', 'T', 'A' }, carol0ah_private_key );
  expected_operations.insert( { OP_TAG(custom_operation), fc::optional< expected_operation_result_t >() } );

  push_custom_json_operation( {}, { "carol0ah" }, "7id", "{"type": "json"}", carol0ah_private_key );
  expected_operations.insert( { OP_TAG(custom_json_operation), fc::optional< expected_operation_result_t >() } );
  // custom_binary_operation, reset_account_operation & set_reset_account_operation have been disabled and do not occur in blockchain

  decline_voting_rights( "dan0ah", true, dan0ah_private_key );
  expected_operations.insert( { OP_TAG(decline_voting_rights_operation), fc::optional< expected_operation_result_t >() } );

  recurrent_transfer( "carol0ah", "dan0ah", ASSET( "0.037 TESTS" ), "With love", 24, 2, carol0ah_private_key );
  expected_operations.insert( { OP_TAG(recurrent_transfer_operation), fc::optional< expected_operation_result_t >() } );
  expected_operations.insert( { OP_TAG(fill_recurrent_transfer_operation), fc::optional< expected_operation_result_t >() } );

  ACTORS((alice0ah))
  expected_operations.insert( { OP_TAG(account_create_operation), fc::optional< expected_operation_result_t >() } );
  fund( "alice0ah", 500000000 );

  account_update2( "alice0ah", fc::optional<authority>(), fc::optional<authority>(), fc::optional<authority>(),
                   fc::optional<fc::ecc::public_key>(), "{"position":"top"}", "{"winner":"me"}", alice0ah_private_key );
  expected_operations.insert( { OP_TAG(account_update2_operation), fc::optional< expected_operation_result_t >() } );

  // transfer_operation from alice0ah
  transfer("alice0ah", "bob0ah", asset(1234, HIVE_SYMBOL));
  expected_operations.insert( { OP_TAG(transfer_operation), fc::optional< expected_operation_result_t >() } );

  generate_until_irreversible_block( *this, expected_operations, fc::optional<uint32_t>() ); // clears the container nominally

  expected_operations.insert( { OP_TAG(changed_recovery_account_operation), fc::optional< expected_operation_result_t >() } );
  generate_until_irreversible_block( *this, expected_operations, 1313 ); // clears the container nominally

*/

} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_SUITE_END() // condenser_get_account_history_tests
#endif

