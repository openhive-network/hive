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
      configuration_data.set_feed_related_values( 1, 3*24*7 ); // see comment to set_feed_related_values
      configuration_data.set_savings_related_values( 20 );

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
      configuration_data.reset_feed_values();
      configuration_data.reset_savings_values();

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
   * 
   * The operations happen in block 2 (vesting_shares_split_operation, when HF1 is set) and block 21 (system_warning_operation),
   * regardless of configurations settings of the fixture.
  */
  void hf1_scenario( check_point_tester_t check_point_tester )
  {
    generate_block(); // block 1
    
    // Set first hardfork to test virtual operations that happen only there.
    db->set_hardfork( HIVE_HARDFORK_0_1 );
    generate_block(); // block 2

    check_point_tester( std::numeric_limits<uint32_t>::max() ); // <- no limit to max number of block generated inside.
  }

  /** 
   * Tests pow_operation that needs hardfork lower than 13.
   * Also tests: pow_reward_operation, account_created_operation, comment_operation (see database_fixture::create_with_pow) & producer_reward_operation.
   * 
   * All tested operations happen in block 3 (when create_with_pow is called) regardless of configurations settings of the fixture.
  */
  void hf12_scenario( check_point_tester_t check_point_tester )
  {
    db->set_hardfork( HIVE_HARDFORK_0_12 );
    generate_block(); // block 1

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

  // Following operations can be checked now. They all appear in block 3 regardless of configurations settings of the fixture.
  // pow2_operation, account_created_operation (x2), pow_reward_operation, transfer_operation,
  // account_create_with_delegation_operation, comment_operation, comment_options_operation & producer_reward_operation.
  check_point_1_tester( std::numeric_limits<uint32_t>::max() ); // <- no limit to max number of block generated inside.

  vote("edgar0ah", "permlink1", "dan0ah", HIVE_1_PERCENT * 100, dan0ah_private_key);
  delete_comment( "edgar0ah", "permlink1", edgar0ah_private_key );

  // In following block (which number depends on how many were generated in check_point_1_tester) these operations appear
  // and can be checked: vote_operation, effective_comment_vote_operation, delete_comment_operation,
  // ineffective_delete_comment_operation & producer_reward_operation.
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

    // In check_point_1_tester generate as many blocks as needed for these virtual operations to appear in block:
    // curation_reward_operation, author_reward_operation, comment_reward_operation, comment_payout_update_operation & producer_reward_operation.
    // In standard configuration of this fixture it's 6th block.
    check_point_1_tester( std::numeric_limits<uint32_t>::max() ); // <- no limit to max number of block generated inside.

    // The absolute minimum of claimed values is used here to allow greater flexibility for the tests using this scenario.
    claim_reward_balance( "edgar0ah", ASSET( "0.000 TESTS" ), ASSET( "0.001 TBD" ), ASSET( "0.000001 VESTS" ), edgar0ah_private_key );

    // In following block (which number depends on how many were generated in check_point_1_tester) these operations appear
    // and can be checked:  claim_reward_balance_operation & producer_reward_operation.
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

    // Now all the operations mentioned above can be checked. All of them will appear in 5th block,
    // except fill_convert_request_operation, fill_collateralized_convert_request_operation - their block number depends of test configuration.
    // In standard configuration of this fixture it's 88th block.
    check_point_tester( std::numeric_limits<uint32_t>::max() ); // <- no limit to max number of block generated inside.
  }

  /**
   * Operations tested here:
   *  transfer_to_vesting_operation, transfer_to_vesting_completed_operation, set_withdraw_vesting_route_operation,
   *  delegate_vesting_shares_operation, withdraw_vesting_operation, producer_reward_operation,
   *  fill_vesting_withdraw_operation & return_vesting_delegation_operation
   */
  void vesting_scenario( check_point_tester_t check_point_tester )
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
    // Now decrease delegation to trigger return_vesting_delegation_operation
    delegate_vest( "alice4ah", "carol4ah", asset(2, VESTS_SYMBOL), alice4ah_private_key );

    // Now all the operations mentioned above can be checked. The immediate ones will appear in 5th block.
    // The delayed ones will appear in blocks, which number depends of test configuration.
    // In standard configuration of this fixture they are 8th (fill_vesting_withdraw_operation) & 9th (return_vesting_delegation_operation) blocks.
    check_point_tester( std::numeric_limits<uint32_t>::max() ); // <- no limit to max number of block generated inside.
  }

  /**
   * Operations tested here:
   *  witness_update_operation, feed_publish_operation, account_witness_proxy_operation, account_witness_vote_operation, witness_set_properties_operation
   * Also tested here: 
   *  producer_reward_operation
   *  
   * Note that witness_block_approve_operation never appears in block (see its evaluator).
   */
  void witness_scenario( check_point_tester_t check_point_tester )
  {
    db->set_hardfork( HIVE_HARDFORK_1_27 );
    generate_block();

    ACTORS( (alice5ah)(ben5ah)(carol5ah) );
    generate_block();
    
    witness_create( "alice5ah", alice5ah_private_key, "foo.bar", alice5ah_private_key.get_public_key(), 1000 );
    witness_feed_publish( "alice5ah", price( ASSET( "1.000 TBD" ), ASSET( "1.000 TESTS" ) ), alice5ah_private_key );
    proxy( "ben5ah", "carol5ah" );
    witness_vote( "carol5ah", "alice5ah", carol5ah_private_key );
    // Note that we don't use existing database_fixture::set_witness_props function below,
    // because we don't want "uncontrolled" block generation that happens there. We only
    // need the operation in block, so we can do our test on it.
    // witness_set_properties_operation
    fc::flat_map< std::string, std::vector<char> > props;
    props[ "hbd_interest_rate" ] = fc::raw::pack_to_vector( 0 );
    props["key"] = fc::raw::pack_to_vector( alice5ah_private_key.get_public_key() );
    witness_set_properties_operation op;
    op.owner = "alice5ah";
    op.props = props;
    push_transaction( op, alice5ah_private_key );

    // Now all the operations mentioned above can be checked. All of them will appear in 4th block, regardless of the fixture configuration.
    check_point_tester( std::numeric_limits<uint32_t>::max() ); // <- no limit to max number of block generated inside.
  }

  /**
   * Operations tested here:
   *  escrow_transfer_operation, escrow_approve_operation, escrow_approved_operation, escrow_release_operation, escrow_dispute_operation,
   *  transfer_to_savings_operation, transfer_from_savings_operation, cancel_transfer_from_savings_operation & fill_transfer_from_savings_operation
   */
  void escrow_and_savings_scenario( check_point_tester_t check_point_tester )
  {
    db->set_hardfork( HIVE_HARDFORK_1_27 );
    generate_block();

    ACTORS( (alice6ah)(ben6ah)(carol6ah) );
    generate_block();
    fund( "alice6ah", ASSET( "1.111 TESTS" ) );
    generate_block();
    
    escrow_transfer( "alice6ah", "ben6ah", "carol6ah", ASSET( "0.071 TESTS" ), ASSET( "0.000 TBD" ), ASSET( "0.001 TESTS" ), "",
                    fc::seconds( HIVE_BLOCK_INTERVAL * 10 ), fc::seconds( HIVE_BLOCK_INTERVAL * 20 ), 30, alice6ah_private_key );
    escrow_transfer( "alice6ah", "ben6ah", "carol6ah", ASSET( "0.007 TESTS" ), ASSET( "0.000 TBD" ), ASSET( "0.001 TESTS" ), "",
                    fc::seconds( HIVE_BLOCK_INTERVAL * 10 ), fc::seconds( HIVE_BLOCK_INTERVAL * 20 ), 31, alice6ah_private_key );

    escrow_approve( "alice6ah", "ben6ah", "carol6ah", "carol6ah", true, 30, carol6ah_private_key );
    escrow_approve( "alice6ah", "ben6ah", "carol6ah", "ben6ah", true, 30, ben6ah_private_key );
    escrow_release( "alice6ah", "ben6ah", "carol6ah", "alice6ah", "ben6ah", ASSET( "0.013 TESTS" ), ASSET( "0.000 TBD" ), 30, alice6ah_private_key );
    escrow_dispute( "alice6ah", "ben6ah", "carol6ah", "ben6ah", 30, ben6ah_private_key );

    escrow_approve( "alice6ah", "ben6ah", "carol6ah", "carol6ah", false, 31, carol6ah_private_key );

    transfer_to_savings( "alice6ah", "alice6ah", ASSET( "0.009 TESTS" ), "ah savings", alice6ah_private_key );
    transfer_from_savings( "alice6ah", "alice6ah", ASSET( "0.006 TESTS" ), 0, alice6ah_private_key );
    transfer_from_savings( "alice6ah", "alice6ah", ASSET( "0.003 TESTS" ), 1, alice6ah_private_key );
    cancel_transfer_from_savings( "alice6ah", 0, alice6ah_private_key );

    // Now all the operations mentioned above can be checked. All of them will appear in 5th block,
    // except fill_transfer_from_savings_operation - its block number depends on test configuration.
    // In standard configuration of this fixture it's 11th block.
    check_point_tester( std::numeric_limits<uint32_t>::max() ); // <- no limit to max number of block generated inside.
  }

  /**
   * Operations tested here:
   *  dhf_funding_operation, dhf_conversion_operation, transfer_operation,
   *  create_proposal_operation, proposal_fee_operation, update_proposal_operation, update_proposal_votes_operation & remove_proposal_operation
   */
  void proposal_scenario( check_point_tester_t check_point_tester )
  {
    db->set_hardfork( HIVE_HARDFORK_1_27 );
    generate_block();

    ACTORS( (alice7ah)(ben7ah)(carol7ah) );
    generate_block();
    fund( "alice7ah", ASSET( "800.000 TBD" ) );
    fund( "carol7ah", ASSET( "10.000 TESTS") );
    generate_block();

    transfer( "carol7ah", db->get_treasury_name(), ASSET( "3.333 TESTS" ) ); // <- trigger dhf_conversion_operation

    int64_t proposal_id = 
      create_proposal( "alice7ah", "ben7ah", db->head_block_time() + fc::days( 1 ), db->head_block_time() + fc::days( 2 ),
                       asset( 100, HBD_SYMBOL ), alice7ah_private_key, false/*with_block_generation*/ );
    const proposal_object* proposal = find_proposal( proposal_id );
    BOOST_REQUIRE_NE( proposal, nullptr );

    update_proposal( proposal_id, "alice7ah", asset( 80, HBD_SYMBOL ), "new subject", proposal->permlink, alice7ah_private_key);
    vote_proposal( "carol7ah", { proposal_id }, true/*approve*/, carol7ah_private_key);
    remove_proposal( "alice7ah", { proposal_id }, alice7ah_private_key );

    // All operations mentioned above can be checked now in 5th block except dhf_funding_operation (2nd block),
    // regardless of the fixture configuration.
    check_point_tester( std::numeric_limits<uint32_t>::max() ); // <- no limit to max number of block generated inside.
  }

  /**
   * Operations tested here:
   *  claim_account_operation, create_claimed_account_operation,
   *  change_recovery_account_operation, changed_recovery_account_operation,
   *  request_account_recovery_operation, recover_account_operation,
   *  account_update_operation & account_update2_operation
   * 
   * Note that reset_account_operation & set_reset_account_operation have been disabled and do not occur in blockchain.
   */
  void account_scenario( check_point_tester_t check_point_tester )
  {
    db->set_hardfork( HIVE_HARDFORK_1_27 );
    generate_block();

    ACTORS( (alice8ah)(carol8ah) );
    // We need elected witness for claim_account_operation to succeed.
    fund( "alice8ah", 5000 );
    vest( "alice8ah", 5000 );
    generate_block();
    witness_vote( "alice8ah", "initminer", alice8ah_private_key );
    // Generate number of blocks sufficient for the witness to be elected & scheduled (and get a subsidized account).
    for( int i = 0; i < 42; ++i)
      generate_block();

    account_update( "alice8ah", alice8ah_private_key.get_public_key(), R"~("{"position":"top"}")~",
                    fc::optional<authority>(), fc::optional<authority>(), fc::optional<authority>(), alice8ah_private_key );

    claim_account( "alice8ah", ASSET( "0.000 TESTS" ), alice8ah_private_key );
    PREP_ACTOR( ben8ah )
    create_claimed_account( "alice8ah", "ben8ah", ben8ah_public_key, ben8ah_post_key.get_public_key(), "", alice8ah_private_key );

    vest( HIVE_INIT_MINER_NAME, "ben8ah", ASSET( "1000.000 TESTS" ) );
    change_recovery_account( "ben8ah", HIVE_INIT_MINER_NAME, ben8ah_private_key );
    account_update2( "ben8ah", authority(1, carol8ah_public_key,1), fc::optional<authority>(), fc::optional<authority>(),
                     ben8ah_private_key.get_public_key(), R"~("{"success":true}")~", "", ben8ah_private_key );
    request_account_recovery( "alice8ah", "ben8ah", authority( 1, alice8ah_private_key.get_public_key(), 1 ), alice8ah_private_key );
    recover_account( "ben8ah", alice8ah_private_key, ben8ah_private_key );

    // Now all the operations mentioned above can be checked. All of them will appear in 46th block,
    // except changed_recovery_account_operation - its block number depends on test configuration.
    // In standard configuration of this fixture it's 65th block.
    check_point_tester( std::numeric_limits<uint32_t>::max() ); // <- no limit to max number of block generated inside.
  }

  /**
   * Operations tested here:
   *  custom_operation, custom_json_operation & producer_reward_operation
   * 
   * Note that custom_binary_operation has been disabled and does not occur in blockchain.
   */
  void custom_scenario( check_point_tester_t check_point_tester )
  {
    db->set_hardfork( HIVE_HARDFORK_1_27 );
    generate_block();

    ACTORS( (alice9ah) );
    generate_block();

    push_custom_operation( { "alice9ah" }, 7, { 'D', 'A', 'T', 'A' }, alice9ah_private_key );
    push_custom_json_operation( {}, { "alice9ah" }, "7id", R"~("{"type": "json"}")~", alice9ah_private_key );

    // All operations mentioned above can be checked now in 4th block, regardless of the fixture configuration.
    check_point_tester( std::numeric_limits<uint32_t>::max() ); // <- no limit to max number of block generated inside.
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
#ifdef HIVE_ENABLE_SMT //SMTs have different patterns due to different nonces in pow/pow2
    R"~({"trx_id":"217b989264e56dd84736b56ddc68e19d2ee0706f","block":3,"trx_in_block":0,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:06","op":{"type":"pow_operation","value":{"worker_account":"carol0ah","block_id":"0000000206e295c82de8cf026aa103784d5cc90f","nonce":5,"work":{"worker":"TST5Mwq5o6BruTVbCcxkqVKL4eeRm3Jrs5fjRGHshvGUj29FPh7Yr","input":"36336c7dabfb4cc28a6c1a1b012c838629b8b3358b11c7526cfaae0a34f83fcf","signature":"1f7c98f0f679ae20a3a49c8c4799dd4f59f613fa4b7c657581ffa2225adffb5bf042fba3c5ce2911ebd7d00ff19baa99d706175a2cc1574b8a297441c8861b1ead","work":"069554289ab10f11a9bc9e9086e17453f1b528a46fad3269e6d32b4117ac62e3"},"props":{"account_creation_fee":{"amount":"0","precision":3,"nai":"@@000000021"},"maximum_block_size":131072,"hbd_interest_rate":1000}}},"operation_id":0})~",
    R"~({"trx_id":"217b989264e56dd84736b56ddc68e19d2ee0706f","block":3,"trx_in_block":0,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:06","op":["pow",{"worker_account":"carol0ah","block_id":"0000000206e295c82de8cf026aa103784d5cc90f","nonce":5,"work":{"worker":"TST5Mwq5o6BruTVbCcxkqVKL4eeRm3Jrs5fjRGHshvGUj29FPh7Yr","input":"36336c7dabfb4cc28a6c1a1b012c838629b8b3358b11c7526cfaae0a34f83fcf","signature":"1f7c98f0f679ae20a3a49c8c4799dd4f59f613fa4b7c657581ffa2225adffb5bf042fba3c5ce2911ebd7d00ff19baa99d706175a2cc1574b8a297441c8861b1ead","work":"069554289ab10f11a9bc9e9086e17453f1b528a46fad3269e6d32b4117ac62e3"},"props":{"account_creation_fee":"0.000 TESTS","maximum_block_size":131072,"hbd_interest_rate":1000}}]})~"
    }, { // account_created_operation
    R"~({"trx_id":"217b989264e56dd84736b56ddc68e19d2ee0706f","block":3,"trx_in_block":0,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:00:06","op":{"type":"account_created_operation","value":{"new_account_name":"carol0ah","creator":"carol0ah","initial_vesting_shares":{"amount":"0","precision":6,"nai":"@@000000037"},"initial_delegation":{"amount":"0","precision":6,"nai":"@@000000037"}}},"operation_id":0})~",
    R"~({"trx_id":"217b989264e56dd84736b56ddc68e19d2ee0706f","block":3,"trx_in_block":0,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:00:06","op":["account_created",{"new_account_name":"carol0ah","creator":"carol0ah","initial_vesting_shares":"0.000000 VESTS","initial_delegation":"0.000000 VESTS"}]})~"
    }, { // pow_reward_operation / direct result of pow_operation
    R"~({"trx_id":"217b989264e56dd84736b56ddc68e19d2ee0706f","block":3,"trx_in_block":0,"op_in_trx":2,"virtual_op":true,"timestamp":"2016-01-01T00:00:06","op":{"type":"pow_reward_operation","value":{"worker":"initminer","reward":{"amount":"21000","precision":3,"nai":"@@000000021"}}},"operation_id":0})~",
    R"~({"trx_id":"217b989264e56dd84736b56ddc68e19d2ee0706f","block":3,"trx_in_block":0,"op_in_trx":2,"virtual_op":true,"timestamp":"2016-01-01T00:00:06","op":["pow_reward",{"worker":"initminer","reward":"21.000 TESTS"}]})~"
    }, { // comment_operation / see database_fixture::create_with_pow
    R"~({"trx_id":"217b989264e56dd84736b56ddc68e19d2ee0706f","block":3,"trx_in_block":0,"op_in_trx":3,"virtual_op":false,"timestamp":"2016-01-01T00:00:06","op":{"type":"comment_operation","value":{"parent_author":"","parent_permlink":"test","author":"initminer","permlink":"test","title":"","body":"Hello world!","json_metadata":""}},"operation_id":0})~",
    R"~({"trx_id":"217b989264e56dd84736b56ddc68e19d2ee0706f","block":3,"trx_in_block":0,"op_in_trx":3,"virtual_op":false,"timestamp":"2016-01-01T00:00:06","op":["comment",{"parent_author":"","parent_permlink":"test","author":"initminer","permlink":"test","title":"","body":"Hello world!","json_metadata":""}]})~"
#else
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
#endif
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
#ifdef HIVE_ENABLE_SMT //SMTs have different patterns due to different nonces in pow/pow2
    R"~({"trx_id":"1a68f83f293a8633824fcf98780375788763fc83","block":3,"trx_in_block":0,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:06","op":{"type":"pow2_operation","value":{"work":{"type":"pow2","value":{"input":{"worker_account":"dan0ah","prev_block":"00000002506ae2952bf6c6d4f0ca606cf6089fea","nonce":9},"pow_summary":4219531556}},"new_owner_key":"TST7YJmUoKbPQkrMrZbrgPxDMYJA3uD3utaN3WYRwaFGKYbQ9ftKV","props":{"account_creation_fee":{"amount":"0","precision":3,"nai":"@@000000021"},"maximum_block_size":131072,"hbd_interest_rate":1000}}},"operation_id":0})~",
    R"~({"trx_id":"1a68f83f293a8633824fcf98780375788763fc83","block":3,"trx_in_block":0,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:06","op":["pow2",{"work":["pow2",{"input":{"worker_account":"dan0ah","prev_block":"00000002506ae2952bf6c6d4f0ca606cf6089fea","nonce":9},"pow_summary":4219531556}],"new_owner_key":"TST7YJmUoKbPQkrMrZbrgPxDMYJA3uD3utaN3WYRwaFGKYbQ9ftKV","props":{"account_creation_fee":"0.000 TESTS","maximum_block_size":131072,"hbd_interest_rate":1000}}]})~"
    }, { // account_created_operation
    R"~({"trx_id":"1a68f83f293a8633824fcf98780375788763fc83","block":3,"trx_in_block":0,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:00:06","op":{"type":"account_created_operation","value":{"new_account_name":"dan0ah","creator":"dan0ah","initial_vesting_shares":{"amount":"0","precision":6,"nai":"@@000000037"},"initial_delegation":{"amount":"0","precision":6,"nai":"@@000000037"}}},"operation_id":0})~",
    R"~({"trx_id":"1a68f83f293a8633824fcf98780375788763fc83","block":3,"trx_in_block":0,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:00:06","op":["account_created",{"new_account_name":"dan0ah","creator":"dan0ah","initial_vesting_shares":"0.000000 VESTS","initial_delegation":"0.000000 VESTS"}]})~"
    }, { // pow_reward_operation
    R"~({"trx_id":"1a68f83f293a8633824fcf98780375788763fc83","block":3,"trx_in_block":0,"op_in_trx":2,"virtual_op":true,"timestamp":"2016-01-01T00:00:06","op":{"type":"pow_reward_operation","value":{"worker":"initminer","reward":{"amount":"1000000000000","precision":6,"nai":"@@000000037"}}},"operation_id":0})~",
    R"~({"trx_id":"1a68f83f293a8633824fcf98780375788763fc83","block":3,"trx_in_block":0,"op_in_trx":2,"virtual_op":true,"timestamp":"2016-01-01T00:00:06","op":["pow_reward",{"worker":"initminer","reward":"1000000.000000 VESTS"}]})~"
    }, { // transfer_operation
    R"~({"trx_id":"1a68f83f293a8633824fcf98780375788763fc83","block":3,"trx_in_block":0,"op_in_trx":3,"virtual_op":false,"timestamp":"2016-01-01T00:00:06","op":{"type":"transfer_operation","value":{"from":"initminer","to":"dan0ah","amount":{"amount":"1","precision":3,"nai":"@@000000021"},"memo":"test"}},"operation_id":0})~",
    R"~({"trx_id":"1a68f83f293a8633824fcf98780375788763fc83","block":3,"trx_in_block":0,"op_in_trx":3,"virtual_op":false,"timestamp":"2016-01-01T00:00:06","op":["transfer",{"from":"initminer","to":"dan0ah","amount":"0.001 TESTS","memo":"test"}]})~"
#else
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
#endif
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
      R"~({"trx_id":"daa9aa439e8af76a93cf4b539c3337b1bc303466","block":28,"trx_in_block":0,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:01:21","op":{"type":"claim_reward_balance_operation","value":{"account":"edgar0ah","reward_hive":{"amount":"0","precision":3,"nai":"@@000000021"},"reward_hbd":{"amount":"1","precision":3,"nai":"@@000000013"},"reward_vests":{"amount":"1","precision":6,"nai":"@@000000037"}}},"operation_id":0})~",
      R"~({"trx_id":"daa9aa439e8af76a93cf4b539c3337b1bc303466","block":28,"trx_in_block":0,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:01:21","op":["claim_reward_balance",{"account":"edgar0ah","reward_hive":"0.000 TESTS","reward_hbd":"0.001 TBD","reward_vests":"0.000001 VESTS"}]})~"
    }, { // producer_reward_operation
      R"~({"trx_id":"0000000000000000000000000000000000000000","block":28,"trx_in_block":4294967295,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:01:24","op":{"type":"producer_reward_operation","value":{"producer":"initminer","vesting_shares":{"amount":"7076153007","precision":6,"nai":"@@000000037"}}},"operation_id":0})~",
      R"~({"trx_id":"0000000000000000000000000000000000000000","block":28,"trx_in_block":4294967295,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:01:24","op":["producer_reward",{"producer":"initminer","vesting_shares":"7076.153007 VESTS"}]})~"
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

    generate_until_irreversible_block( 88 );
    BOOST_REQUIRE( db->head_block_num() <= generate_no_further_than );

    // Check virtual operations spawned by the ones obove:
    expected_operations = { { // producer_reward_operation
      R"~({"trx_id":"0000000000000000000000000000000000000000","block":88,"trx_in_block":4294967295,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:04:24","op":{"type":"producer_reward_operation","value":{"producer":"initminer","vesting_shares":{"amount":"150192950111","precision":6,"nai":"@@000000037"}}},"operation_id":0})~",
      R"~({"trx_id":"0000000000000000000000000000000000000000","block":88,"trx_in_block":4294967295,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:04:24","op":["producer_reward",{"producer":"initminer","vesting_shares":"150192.950111 VESTS"}]})~"
      }, { // fill_convert_request_operation
      R"~({"trx_id":"0000000000000000000000000000000000000000","block":88,"trx_in_block":4294967295,"op_in_trx":2,"virtual_op":true,"timestamp":"2016-01-01T00:04:24","op":{"type":"fill_convert_request_operation","value":{"owner":"edgar3ah","requestid":0,"amount_in":{"amount":"11201","precision":3,"nai":"@@000000013"},"amount_out":{"amount":"11201","precision":3,"nai":"@@000000021"}}},"operation_id":0})~",
      R"~({"trx_id":"0000000000000000000000000000000000000000","block":88,"trx_in_block":4294967295,"op_in_trx":2,"virtual_op":true,"timestamp":"2016-01-01T00:04:24","op":["fill_convert_request",{"owner":"edgar3ah","requestid":0,"amount_in":"11.201 TBD","amount_out":"11.201 TESTS"}]})~"
      }, { // fill_collateralized_convert_request_operation
      R"~({"trx_id":"0000000000000000000000000000000000000000","block":88,"trx_in_block":4294967295,"op_in_trx":3,"virtual_op":true,"timestamp":"2016-01-01T00:04:24","op":{"type":"fill_collateralized_convert_request_operation","value":{"owner":"carol3ah","requestid":0,"amount_in":{"amount":"11050","precision":3,"nai":"@@000000021"},"amount_out":{"amount":"10524","precision":3,"nai":"@@000000013"},"excess_collateral":{"amount":"11052","precision":3,"nai":"@@000000021"}}},"operation_id":0})~",
      R"~({"trx_id":"0000000000000000000000000000000000000000","block":88,"trx_in_block":4294967295,"op_in_trx":3,"virtual_op":true,"timestamp":"2016-01-01T00:04:24","op":["fill_collateralized_convert_request",{"owner":"carol3ah","requestid":0,"amount_in":"11.050 TESTS","amount_out":"10.524 TBD","excess_collateral":"11.052 TESTS"}]})~"
      } };
    // Note that all operations of this block are virtual, hence we can reuse the same expected container here.
    test_get_ops_in_block( *this, expected_operations, expected_operations, 88 );
  };

  convert_and_limit_order_scenario( check_point_tester );

} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_CASE( get_ops_in_block_vesting )
{ try {

  BOOST_TEST_MESSAGE( "testing get_ops_in_block with vesting_scenario" );

  auto check_point_tester = [ this ]( uint32_t generate_no_further_than )
  {
    generate_until_irreversible_block( 9 );
    BOOST_REQUIRE( db->head_block_num() <= generate_no_further_than );

    expected_t expected_operations = { { // transfer_to_vesting_operation
      R"~({"trx_id":"0ace3d6e57043dfde1daf6c00d5d7a6c1e556126","block":5,"trx_in_block":0,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:12","op":{"type":"transfer_to_vesting_operation","value":{"from":"alice4ah","to":"alice4ah","amount":{"amount":"2000","precision":3,"nai":"@@000000021"}}},"operation_id":0})~",
      R"~({"trx_id":"0ace3d6e57043dfde1daf6c00d5d7a6c1e556126","block":5,"trx_in_block":0,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:12","op":["transfer_to_vesting",{"from":"alice4ah","to":"alice4ah","amount":"2.000 TESTS"}]})~"
      }, { // transfer_to_vesting_completed_operation
      R"~({"trx_id":"0ace3d6e57043dfde1daf6c00d5d7a6c1e556126","block":5,"trx_in_block":0,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:00:12","op":{"type":"transfer_to_vesting_completed_operation","value":{"from_account":"alice4ah","to_account":"alice4ah","hive_vested":{"amount":"2000","precision":3,"nai":"@@000000021"},"vesting_shares_received":{"amount":"1936378093693","precision":6,"nai":"@@000000037"}}},"operation_id":0})~",
      R"~({"trx_id":"0ace3d6e57043dfde1daf6c00d5d7a6c1e556126","block":5,"trx_in_block":0,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:00:12","op":["transfer_to_vesting_completed",{"from_account":"alice4ah","to_account":"alice4ah","hive_vested":"2.000 TESTS","vesting_shares_received":"1936378.093693 VESTS"}]})~"
      }, { // set_withdraw_vesting_route_operation
      R"~({"trx_id":"f836d85674c299b307b8168bd3c18d9018de4bb6","block":5,"trx_in_block":1,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:12","op":{"type":"set_withdraw_vesting_route_operation","value":{"from_account":"alice4ah","to_account":"ben4ah","percent":5000,"auto_vest":true}},"operation_id":0})~",
      R"~({"trx_id":"f836d85674c299b307b8168bd3c18d9018de4bb6","block":5,"trx_in_block":1,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:12","op":["set_withdraw_vesting_route",{"from_account":"alice4ah","to_account":"ben4ah","percent":5000,"auto_vest":true}]})~"
      }, { // delegate_vesting_shares_operation
      R"~({"trx_id":"64261f6d3e997f3e9c7846712bde0b5d7da983fb","block":5,"trx_in_block":2,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:12","op":{"type":"delegate_vesting_shares_operation","value":{"delegator":"alice4ah","delegatee":"carol4ah","vesting_shares":{"amount":"3","precision":6,"nai":"@@000000037"}}},"operation_id":0})~",
      R"~({"trx_id":"64261f6d3e997f3e9c7846712bde0b5d7da983fb","block":5,"trx_in_block":2,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:12","op":["delegate_vesting_shares",{"delegator":"alice4ah","delegatee":"carol4ah","vesting_shares":"0.000003 VESTS"}]})~"
      }, { // withdraw_vesting_operation
      R"~({"trx_id":"f5db21d976c5d7281e3c09838c6b14a8d78e9913","block":5,"trx_in_block":3,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:12","op":{"type":"withdraw_vesting_operation","value":{"account":"alice4ah","vesting_shares":{"amount":"123","precision":6,"nai":"@@000000037"}}},"operation_id":0})~",
      R"~({"trx_id":"f5db21d976c5d7281e3c09838c6b14a8d78e9913","block":5,"trx_in_block":3,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:12","op":["withdraw_vesting",{"account":"alice4ah","vesting_shares":"0.000123 VESTS"}]})~"
      }, { // delegate_vesting_shares_operation
      R"~({"trx_id":"650e7569d7c96f825886f04b4b53168e27c5707a","block":5,"trx_in_block":4,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:12","op":{"type":"delegate_vesting_shares_operation","value":{"delegator":"alice4ah","delegatee":"carol4ah","vesting_shares":{"amount":"2","precision":6,"nai":"@@000000037"}}},"operation_id":0})~",
      R"~({"trx_id":"650e7569d7c96f825886f04b4b53168e27c5707a","block":5,"trx_in_block":4,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:12","op":["delegate_vesting_shares",{"delegator":"alice4ah","delegatee":"carol4ah","vesting_shares":"0.000002 VESTS"}]})~"
      }, { // producer_reward_operation
      R"~({"trx_id":"0000000000000000000000000000000000000000","block":5,"trx_in_block":4294967295,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:00:15","op":{"type":"producer_reward_operation","value":{"producer":"initminer","vesting_shares":{"amount":"8680177266","precision":6,"nai":"@@000000037"}}},"operation_id":0})~",
      R"~({"trx_id":"0000000000000000000000000000000000000000","block":5,"trx_in_block":4294967295,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:00:15","op":["producer_reward",{"producer":"initminer","vesting_shares":"8680.177266 VESTS"}]})~"
      } };
    expected_t expected_virtual_operations = { expected_operations[1], expected_operations[6] };
    test_get_ops_in_block( *this, expected_operations, expected_virtual_operations, 5 );

    expected_operations = { { // producer_reward_operation
      R"~({"trx_id":"0000000000000000000000000000000000000000","block":8,"trx_in_block":4294967295,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:00:24","op":{"type":"producer_reward_operation","value":{"producer":"initminer","vesting_shares":{"amount":"8581651955","precision":6,"nai":"@@000000037"}}},"operation_id":0})~",
      R"~({"trx_id":"0000000000000000000000000000000000000000","block":8,"trx_in_block":4294967295,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:00:24","op":["producer_reward",{"producer":"initminer","vesting_shares":"8581.651955 VESTS"}]})~"
      }, { // fill_vesting_withdraw_operation
      R"~({"trx_id":"0000000000000000000000000000000000000000","block":8,"trx_in_block":4294967295,"op_in_trx":2,"virtual_op":true,"timestamp":"2016-01-01T00:00:24","op":{"type":"fill_vesting_withdraw_operation","value":{"from_account":"alice4ah","to_account":"ben4ah","withdrawn":{"amount":"4","precision":6,"nai":"@@000000037"},"deposited":{"amount":"4","precision":6,"nai":"@@000000037"}}},"operation_id":0})~",
      R"~({"trx_id":"0000000000000000000000000000000000000000","block":8,"trx_in_block":4294967295,"op_in_trx":2,"virtual_op":true,"timestamp":"2016-01-01T00:00:24","op":["fill_vesting_withdraw",{"from_account":"alice4ah","to_account":"ben4ah","withdrawn":"0.000004 VESTS","deposited":"0.000004 VESTS"}]})~"
      }, { // fill_vesting_withdraw_operation
      R"~({"trx_id":"0000000000000000000000000000000000000000","block":8,"trx_in_block":4294967295,"op_in_trx":3,"virtual_op":true,"timestamp":"2016-01-01T00:00:24","op":{"type":"fill_vesting_withdraw_operation","value":{"from_account":"alice4ah","to_account":"alice4ah","withdrawn":{"amount":"5","precision":6,"nai":"@@000000037"},"deposited":{"amount":"0","precision":3,"nai":"@@000000021"}}},"operation_id":0})~",
      R"~({"trx_id":"0000000000000000000000000000000000000000","block":8,"trx_in_block":4294967295,"op_in_trx":3,"virtual_op":true,"timestamp":"2016-01-01T00:00:24","op":["fill_vesting_withdraw",{"from_account":"alice4ah","to_account":"alice4ah","withdrawn":"0.000005 VESTS","deposited":"0.000 TESTS"}]})~"
      } };
    // Note that all operations of this block are virtual, hence we can reuse the same expected container here.
    test_get_ops_in_block( *this, expected_operations, expected_operations, 8 );

    expected_operations = { { // return_vesting_delegation_operation
      R"~({"trx_id":"0000000000000000000000000000000000000000","block":9,"trx_in_block":4294967295,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:00:27","op":{"type":"return_vesting_delegation_operation","value":{"account":"alice4ah","vesting_shares":{"amount":"1","precision":6,"nai":"@@000000037"}}},"operation_id":0})~",
      R"~({"trx_id":"0000000000000000000000000000000000000000","block":9,"trx_in_block":4294967295,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:00:27","op":["return_vesting_delegation",{"account":"alice4ah","vesting_shares":"0.000001 VESTS"}]})~"
      }, { // producer_reward_operation
      R"~({"trx_id":"0000000000000000000000000000000000000000","block":9,"trx_in_block":4294967295,"op_in_trx":2,"virtual_op":true,"timestamp":"2016-01-01T00:00:27","op":{"type":"producer_reward_operation","value":{"producer":"initminer","vesting_shares":{"amount":"8549473854","precision":6,"nai":"@@000000037"}}},"operation_id":0})~",
      R"~({"trx_id":"0000000000000000000000000000000000000000","block":9,"trx_in_block":4294967295,"op_in_trx":2,"virtual_op":true,"timestamp":"2016-01-01T00:00:27","op":["producer_reward",{"producer":"initminer","vesting_shares":"8549.473854 VESTS"}]})~"
      } };
    // Note that all operations of this block are virtual, hence we can reuse the same expected container here.
    test_get_ops_in_block( *this, expected_operations, expected_operations, 9 );
  };

  vesting_scenario( check_point_tester );

} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_CASE( get_ops_in_block_witness )
{ try {

  BOOST_TEST_MESSAGE( "testing get_ops_in_block with witness_scenario" );

  auto check_point_tester = [ this ]( uint32_t generate_no_further_than )
  {
    generate_until_irreversible_block( 4 );
    BOOST_REQUIRE( db->head_block_num() <= generate_no_further_than );

    expected_t expected_operations = { { // witness_update_operation
      R"~({"trx_id":"68dc811e40bbf5298cc49906ac0b07f2b8d91d50","block":4,"trx_in_block":0,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:09","op":{"type":"witness_update_operation","value":{"owner":"alice5ah","url":"foo.bar","block_signing_key":"TST5x2Aroso4sW7B7wp191Zvzs4mV7vH12g9yccbb2rV7kVUwTC5D","props":{"account_creation_fee":{"amount":"0","precision":3,"nai":"@@000000021"},"maximum_block_size":131072,"hbd_interest_rate":1000},"fee":{"amount":"1000","precision":3,"nai":"@@000000021"}}},"operation_id":0})~",
      R"~({"trx_id":"68dc811e40bbf5298cc49906ac0b07f2b8d91d50","block":4,"trx_in_block":0,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:09","op":["witness_update",{"owner":"alice5ah","url":"foo.bar","block_signing_key":"TST5x2Aroso4sW7B7wp191Zvzs4mV7vH12g9yccbb2rV7kVUwTC5D","props":{"account_creation_fee":"0.000 TESTS","maximum_block_size":131072,"hbd_interest_rate":1000},"fee":"1.000 TESTS"}]})~"
      }, { // feed_publish_operation
      R"~({"trx_id":"515f87df80219a8bc5aee9950e3c0f9db74262a8","block":4,"trx_in_block":1,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:09","op":{"type":"feed_publish_operation","value":{"publisher":"alice5ah","exchange_rate":{"base":{"amount":"1000","precision":3,"nai":"@@000000013"},"quote":{"amount":"1000","precision":3,"nai":"@@000000021"}}}},"operation_id":0})~",
      R"~({"trx_id":"515f87df80219a8bc5aee9950e3c0f9db74262a8","block":4,"trx_in_block":1,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:09","op":["feed_publish",{"publisher":"alice5ah","exchange_rate":{"base":"1.000 TBD","quote":"1.000 TESTS"}}]})~"
      }, { // account_witness_proxy_operation
      R"~({"trx_id":"e3fe9799b02e6ed6bee030ab7e1315f2a6e6a6d4","block":4,"trx_in_block":2,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:09","op":{"type":"account_witness_proxy_operation","value":{"account":"ben5ah","proxy":"carol5ah"}},"operation_id":0})~",
      R"~({"trx_id":"e3fe9799b02e6ed6bee030ab7e1315f2a6e6a6d4","block":4,"trx_in_block":2,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:09","op":["account_witness_proxy",{"account":"ben5ah","proxy":"carol5ah"}]})~"
      }, { // account_witness_vote_operation
      R"~({"trx_id":"a0d6ca658067cfeefb822933ae04a1687c5d235d","block":4,"trx_in_block":3,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:09","op":{"type":"account_witness_vote_operation","value":{"account":"carol5ah","witness":"alice5ah","approve":true}},"operation_id":0})~",
      R"~({"trx_id":"a0d6ca658067cfeefb822933ae04a1687c5d235d","block":4,"trx_in_block":3,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:09","op":["account_witness_vote",{"account":"carol5ah","witness":"alice5ah","approve":true}]})~"
      }, { // witness_set_properties_operation
      R"~({"trx_id":"3e7106641de9e1cc93a415ad73169dc62b3fa89c","block":4,"trx_in_block":4,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:09","op":{"type":"witness_set_properties_operation","value":{"owner":"alice5ah","props":[["hbd_interest_rate","00000000"],["key","028bb6e3bfd8633279430bd6026a1178e8e311fe4700902f647856a9f32ae82a8b"]],"extensions":[]}},"operation_id":0})~",
      R"~({"trx_id":"3e7106641de9e1cc93a415ad73169dc62b3fa89c","block":4,"trx_in_block":4,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:09","op":["witness_set_properties",{"owner":"alice5ah","props":[["hbd_interest_rate","00000000"],["key","028bb6e3bfd8633279430bd6026a1178e8e311fe4700902f647856a9f32ae82a8b"]],"extensions":[]}]})~"
      }, { // producer_reward_operation
      R"~({"trx_id":"0000000000000000000000000000000000000000","block":4,"trx_in_block":4294967295,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:00:12","op":{"type":"producer_reward_operation","value":{"producer":"initminer","vesting_shares":{"amount":"8713701421","precision":6,"nai":"@@000000037"}}},"operation_id":0})~",
      R"~({"trx_id":"0000000000000000000000000000000000000000","block":4,"trx_in_block":4294967295,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:00:12","op":["producer_reward",{"producer":"initminer","vesting_shares":"8713.701421 VESTS"}]})~"
      } };
    expected_t expected_virtual_operations = { expected_operations[5] };
    test_get_ops_in_block( *this, expected_operations, expected_virtual_operations, 4 );
  };
  
  witness_scenario( check_point_tester );
  
} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_CASE( get_ops_in_block_escrow_and_savings )
{ try {

  BOOST_TEST_MESSAGE( "testing get_ops_in_block with escrow_and_savings_scenario" );

  auto check_point_tester = [ this ]( uint32_t generate_no_further_than )
  {
    generate_until_irreversible_block( 5 );
    BOOST_REQUIRE( db->head_block_num() <= generate_no_further_than );

    expected_t expected_operations = { { // escrow_transfer_operation
      R"~({"trx_id":"d991732e509c73b78ef79873cded63346f6c0201","block":5,"trx_in_block":0,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:12","op":{"type":"escrow_transfer_operation","value":{"from":"alice6ah","to":"ben6ah","hbd_amount":{"amount":"0","precision":3,"nai":"@@000000013"},"hive_amount":{"amount":"71","precision":3,"nai":"@@000000021"},"escrow_id":30,"agent":"carol6ah","fee":{"amount":"1","precision":3,"nai":"@@000000021"},"json_meta":"","ratification_deadline":"2016-01-01T00:00:42","escrow_expiration":"2016-01-01T00:01:12"}},"operation_id":0})~",
      R"~({"trx_id":"d991732e509c73b78ef79873cded63346f6c0201","block":5,"trx_in_block":0,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:12","op":["escrow_transfer",{"from":"alice6ah","to":"ben6ah","hbd_amount":"0.000 TBD","hive_amount":"0.071 TESTS","escrow_id":30,"agent":"carol6ah","fee":"0.001 TESTS","json_meta":"","ratification_deadline":"2016-01-01T00:00:42","escrow_expiration":"2016-01-01T00:01:12"}]})~"
      }, { // escrow_transfer_operation
      R"~({"trx_id":"25dac8f4f0d88ae7e88d1b58d58eca00bb7124fa","block":5,"trx_in_block":1,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:12","op":{"type":"escrow_transfer_operation","value":{"from":"alice6ah","to":"ben6ah","hbd_amount":{"amount":"0","precision":3,"nai":"@@000000013"},"hive_amount":{"amount":"7","precision":3,"nai":"@@000000021"},"escrow_id":31,"agent":"carol6ah","fee":{"amount":"1","precision":3,"nai":"@@000000021"},"json_meta":"","ratification_deadline":"2016-01-01T00:00:42","escrow_expiration":"2016-01-01T00:01:12"}},"operation_id":0})~",
      R"~({"trx_id":"25dac8f4f0d88ae7e88d1b58d58eca00bb7124fa","block":5,"trx_in_block":1,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:12","op":["escrow_transfer",{"from":"alice6ah","to":"ben6ah","hbd_amount":"0.000 TBD","hive_amount":"0.007 TESTS","escrow_id":31,"agent":"carol6ah","fee":"0.001 TESTS","json_meta":"","ratification_deadline":"2016-01-01T00:00:42","escrow_expiration":"2016-01-01T00:01:12"}]})~"
      }, { // escrow_approve_operation
      R"~({"trx_id":"b10e747da05fe4b9c28106e965bb48c152f13e6b","block":5,"trx_in_block":2,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:12","op":{"type":"escrow_approve_operation","value":{"from":"alice6ah","to":"ben6ah","agent":"carol6ah","who":"carol6ah","escrow_id":30,"approve":true}},"operation_id":0})~",
      R"~({"trx_id":"b10e747da05fe4b9c28106e965bb48c152f13e6b","block":5,"trx_in_block":2,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:12","op":["escrow_approve",{"from":"alice6ah","to":"ben6ah","agent":"carol6ah","who":"carol6ah","escrow_id":30,"approve":true}]})~"
      }, { // escrow_approve_operation
      R"~({"trx_id":"5697a8fe6d4e3e2b367443d0e4cc2e0df60306c5","block":5,"trx_in_block":3,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:12","op":{"type":"escrow_approve_operation","value":{"from":"alice6ah","to":"ben6ah","agent":"carol6ah","who":"ben6ah","escrow_id":30,"approve":true}},"operation_id":0})~",
      R"~({"trx_id":"5697a8fe6d4e3e2b367443d0e4cc2e0df60306c5","block":5,"trx_in_block":3,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:12","op":["escrow_approve",{"from":"alice6ah","to":"ben6ah","agent":"carol6ah","who":"ben6ah","escrow_id":30,"approve":true}]})~"
      }, { // escrow_approved_operation
      R"~({"trx_id":"5697a8fe6d4e3e2b367443d0e4cc2e0df60306c5","block":5,"trx_in_block":3,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:00:12","op":{"type":"escrow_approved_operation","value":{"from":"alice6ah","to":"ben6ah","agent":"carol6ah","escrow_id":30,"fee":{"amount":"1","precision":3,"nai":"@@000000021"}}},"operation_id":0})~",
      R"~({"trx_id":"5697a8fe6d4e3e2b367443d0e4cc2e0df60306c5","block":5,"trx_in_block":3,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:00:12","op":["escrow_approved",{"from":"alice6ah","to":"ben6ah","agent":"carol6ah","escrow_id":30,"fee":"0.001 TESTS"}]})~"
      }, { // escrow_release_operation
      R"~({"trx_id":"b151c38de8ba594a5a53a648ce03d49890ce4cf9","block":5,"trx_in_block":4,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:12","op":{"type":"escrow_release_operation","value":{"from":"alice6ah","to":"ben6ah","agent":"carol6ah","who":"alice6ah","receiver":"ben6ah","escrow_id":30,"hbd_amount":{"amount":"0","precision":3,"nai":"@@000000013"},"hive_amount":{"amount":"13","precision":3,"nai":"@@000000021"}}},"operation_id":0})~",
      R"~({"trx_id":"b151c38de8ba594a5a53a648ce03d49890ce4cf9","block":5,"trx_in_block":4,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:12","op":["escrow_release",{"from":"alice6ah","to":"ben6ah","agent":"carol6ah","who":"alice6ah","receiver":"ben6ah","escrow_id":30,"hbd_amount":"0.000 TBD","hive_amount":"0.013 TESTS"}]})~"
      }, { // escrow_dispute_operation
      R"~({"trx_id":"0e98dacbf33b7454b6928abe85638fac6ee86e00","block":5,"trx_in_block":5,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:12","op":{"type":"escrow_dispute_operation","value":{"from":"alice6ah","to":"ben6ah","agent":"carol6ah","who":"ben6ah","escrow_id":30}},"operation_id":0})~",
      R"~({"trx_id":"0e98dacbf33b7454b6928abe85638fac6ee86e00","block":5,"trx_in_block":5,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:12","op":["escrow_dispute",{"from":"alice6ah","to":"ben6ah","agent":"carol6ah","who":"ben6ah","escrow_id":30}]})~"
      }, { // escrow_approve_operation
      R"~({"trx_id":"25f39679ea1a87ab8cc879ecdb46ea253252cdb6","block":5,"trx_in_block":6,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:12","op":{"type":"escrow_approve_operation","value":{"from":"alice6ah","to":"ben6ah","agent":"carol6ah","who":"carol6ah","escrow_id":31,"approve":false}},"operation_id":0})~",
      R"~({"trx_id":"25f39679ea1a87ab8cc879ecdb46ea253252cdb6","block":5,"trx_in_block":6,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:12","op":["escrow_approve",{"from":"alice6ah","to":"ben6ah","agent":"carol6ah","who":"carol6ah","escrow_id":31,"approve":false}]})~"
      }, { // escrow_rejected_operation
      R"~({"trx_id":"25f39679ea1a87ab8cc879ecdb46ea253252cdb6","block":5,"trx_in_block":6,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:00:12","op":{"type":"escrow_rejected_operation","value":{"from":"alice6ah","to":"ben6ah","agent":"carol6ah","escrow_id":31,"hbd_amount":{"amount":"0","precision":3,"nai":"@@000000013"},"hive_amount":{"amount":"7","precision":3,"nai":"@@000000021"},"fee":{"amount":"1","precision":3,"nai":"@@000000021"}}},"operation_id":0})~",
      R"~({"trx_id":"25f39679ea1a87ab8cc879ecdb46ea253252cdb6","block":5,"trx_in_block":6,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:00:12","op":["escrow_rejected",{"from":"alice6ah","to":"ben6ah","agent":"carol6ah","escrow_id":31,"hbd_amount":"0.000 TBD","hive_amount":"0.007 TESTS","fee":"0.001 TESTS"}]})~"
      }, { // transfer_to_savings_operation
      R"~({"trx_id":"e86269bf580980032b0bff352b8a4af19e0e4b0d","block":5,"trx_in_block":7,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:12","op":{"type":"transfer_to_savings_operation","value":{"from":"alice6ah","to":"alice6ah","amount":{"amount":"9","precision":3,"nai":"@@000000021"},"memo":"ah savings"}},"operation_id":0})~",
      R"~({"trx_id":"e86269bf580980032b0bff352b8a4af19e0e4b0d","block":5,"trx_in_block":7,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:12","op":["transfer_to_savings",{"from":"alice6ah","to":"alice6ah","amount":"0.009 TESTS","memo":"ah savings"}]})~"
      }, { // transfer_from_savings_operation
      R"~({"trx_id":"82bd5c9e139fb35709039a0d63c364c78962220e","block":5,"trx_in_block":8,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:12","op":{"type":"transfer_from_savings_operation","value":{"from":"alice6ah","request_id":0,"to":"alice6ah","amount":{"amount":"6","precision":3,"nai":"@@000000021"},"memo":""}},"operation_id":0})~",
      R"~({"trx_id":"82bd5c9e139fb35709039a0d63c364c78962220e","block":5,"trx_in_block":8,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:12","op":["transfer_from_savings",{"from":"alice6ah","request_id":0,"to":"alice6ah","amount":"0.006 TESTS","memo":""}]})~"
      }, { // transfer_from_savings_operation
      R"~({"trx_id":"e173506f76a3c9b2b878ccade0c86b2eaee07ac4","block":5,"trx_in_block":9,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:12","op":{"type":"transfer_from_savings_operation","value":{"from":"alice6ah","request_id":1,"to":"alice6ah","amount":{"amount":"3","precision":3,"nai":"@@000000021"},"memo":""}},"operation_id":0})~",
      R"~({"trx_id":"e173506f76a3c9b2b878ccade0c86b2eaee07ac4","block":5,"trx_in_block":9,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:12","op":["transfer_from_savings",{"from":"alice6ah","request_id":1,"to":"alice6ah","amount":"0.003 TESTS","memo":""}]})~"
      }, { // cancel_transfer_from_savings_operation
      R"~({"trx_id":"cd620d856a4e6dced656d3efad334cca8a7d3012","block":5,"trx_in_block":10,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:12","op":{"type":"cancel_transfer_from_savings_operation","value":{"from":"alice6ah","request_id":0}},"operation_id":0})~",
      R"~({"trx_id":"cd620d856a4e6dced656d3efad334cca8a7d3012","block":5,"trx_in_block":10,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:12","op":["cancel_transfer_from_savings",{"from":"alice6ah","request_id":0}]})~"
      }, { // producer_reward_operation
      R"~({"trx_id":"0000000000000000000000000000000000000000","block":5,"trx_in_block":4294967295,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:00:15","op":{"type":"producer_reward_operation","value":{"producer":"initminer","vesting_shares":{"amount":"8631556303","precision":6,"nai":"@@000000037"}}},"operation_id":0})~",
      R"~({"trx_id":"0000000000000000000000000000000000000000","block":5,"trx_in_block":4294967295,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:00:15","op":["producer_reward",{"producer":"initminer","vesting_shares":"8631.556303 VESTS"}]})~"
    } };

    generate_until_irreversible_block( 11 );
    BOOST_REQUIRE( db->head_block_num() <= generate_no_further_than );

    expected_operations = { { // producer_reward_operation
      R"~({"trx_id":"0000000000000000000000000000000000000000","block":11,"trx_in_block":4294967295,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:00:33","op":{"type":"producer_reward_operation","value":{"producer":"initminer","vesting_shares":{"amount":"8179054903","precision":6,"nai":"@@000000037"}}},"operation_id":0})~",
      R"~({"trx_id":"0000000000000000000000000000000000000000","block":11,"trx_in_block":4294967295,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:00:33","op":["producer_reward",{"producer":"initminer","vesting_shares":"8179.054903 VESTS"}]})~"
      }, { // fill_transfer_from_savings_operation
      R"~({"trx_id":"0000000000000000000000000000000000000000","block":11,"trx_in_block":4294967295,"op_in_trx":2,"virtual_op":true,"timestamp":"2016-01-01T00:00:33","op":{"type":"fill_transfer_from_savings_operation","value":{"from":"alice6ah","to":"alice6ah","amount":{"amount":"3","precision":3,"nai":"@@000000021"},"request_id":1,"memo":""}},"operation_id":0})~",
      R"~({"trx_id":"0000000000000000000000000000000000000000","block":11,"trx_in_block":4294967295,"op_in_trx":2,"virtual_op":true,"timestamp":"2016-01-01T00:00:33","op":["fill_transfer_from_savings",{"from":"alice6ah","to":"alice6ah","amount":"0.003 TESTS","request_id":1,"memo":""}]})~"
      } };
    // Note that all operations of this block are virtual, hence we can reuse the same expected container here.
    test_get_ops_in_block( *this, expected_operations, expected_operations, 11 );
  };

  escrow_and_savings_scenario( check_point_tester );

} FC_LOG_AND_RETHROW() }


BOOST_AUTO_TEST_CASE( get_ops_in_block_proposal )
{ try {

  BOOST_TEST_MESSAGE( "testing get_ops_in_block with proposal_scenario" );

  auto check_point_tester = [ this ]( uint32_t generate_no_further_than )
  {
    generate_until_irreversible_block( 5 );
    BOOST_REQUIRE( db->head_block_num() <= generate_no_further_than );

    expected_t expected_operations = { { // producer_reward_operation
      R"~({"trx_id":"0000000000000000000000000000000000000000","block":2,"trx_in_block":4294967295,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:00:06","op":{"type":"producer_reward_operation","value":{"producer":"initminer","vesting_shares":{"amount":"8884501480","precision":6,"nai":"@@000000037"}}},"operation_id":0})~",
      R"~({"trx_id":"0000000000000000000000000000000000000000","block":2,"trx_in_block":4294967295,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:00:06","op":["producer_reward",{"producer":"initminer","vesting_shares":"8884.501480 VESTS"}]})~"
      }, { // dhf_funding_operation
      R"~({"trx_id":"0000000000000000000000000000000000000000","block":2,"trx_in_block":4294967295,"op_in_trx":2,"virtual_op":true,"timestamp":"2016-01-01T00:00:06","op":{"type":"dhf_funding_operation","value":{"treasury":"hive.fund","additional_funds":{"amount":"9","precision":3,"nai":"@@000000013"}}},"operation_id":0})~",
      R"~({"trx_id":"0000000000000000000000000000000000000000","block":2,"trx_in_block":4294967295,"op_in_trx":2,"virtual_op":true,"timestamp":"2016-01-01T00:00:06","op":["dhf_funding",{"treasury":"hive.fund","additional_funds":"0.009 TBD"}]})~"
      } };
    // Note that all operations of this block are virtual, hence we can reuse the same expected container here.
    test_get_ops_in_block( *this, expected_operations, expected_operations, 2 );

    expected_operations = { { // transfer_operation
      R"~({"trx_id":"909b8d77940011e1496d49d73f19282ce0d19cef","block":5,"trx_in_block":0,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:12","op":{"type":"transfer_operation","value":{"from":"carol7ah","to":"hive.fund","amount":{"amount":"3333","precision":3,"nai":"@@000000021"},"memo":""}},"operation_id":0})~",
      R"~({"trx_id":"909b8d77940011e1496d49d73f19282ce0d19cef","block":5,"trx_in_block":0,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:12","op":["transfer",{"from":"carol7ah","to":"hive.fund","amount":"3.333 TESTS","memo":""}]})~"
      }, { // dhf_conversion_operation
      R"~({"trx_id":"909b8d77940011e1496d49d73f19282ce0d19cef","block":5,"trx_in_block":0,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:00:12","op":{"type":"dhf_conversion_operation","value":{"treasury":"hive.fund","hive_amount_in":{"amount":"3333","precision":3,"nai":"@@000000021"},"hbd_amount_out":{"amount":"3333","precision":3,"nai":"@@000000013"}}},"operation_id":0})~",
      R"~({"trx_id":"909b8d77940011e1496d49d73f19282ce0d19cef","block":5,"trx_in_block":0,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:00:12","op":["dhf_conversion",{"treasury":"hive.fund","hive_amount_in":"3.333 TESTS","hbd_amount_out":"3.333 TBD"}]})~"
      }, { // comment_operation
      R"~({"trx_id":"3a20708685a9510a1a03a58499cc2a0cd42985d8","block":5,"trx_in_block":1,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:12","op":{"type":"comment_operation","value":{"parent_author":"","parent_permlink":"test","author":"alice7ah","permlink":"permlink0","title":"title","body":"body","json_metadata":""}},"operation_id":0})~",
      R"~({"trx_id":"3a20708685a9510a1a03a58499cc2a0cd42985d8","block":5,"trx_in_block":1,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:12","op":["comment",{"parent_author":"","parent_permlink":"test","author":"alice7ah","permlink":"permlink0","title":"title","body":"body","json_metadata":""}]})~"
      }, { // create_proposal_operation
      R"~({"trx_id":"2309ebc61f5580e870fcbc982f03acc4614335a8","block":5,"trx_in_block":2,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:12","op":{"type":"create_proposal_operation","value":{"creator":"alice7ah","receiver":"ben7ah","start_date":"2016-01-02T00:00:12","end_date":"2016-01-03T00:00:12","daily_pay":{"amount":"100","precision":3,"nai":"@@000000013"},"subject":"0","permlink":"permlink0","extensions":[]}},"operation_id":0})~",
      R"~({"trx_id":"2309ebc61f5580e870fcbc982f03acc4614335a8","block":5,"trx_in_block":2,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:12","op":["create_proposal",{"creator":"alice7ah","receiver":"ben7ah","start_date":"2016-01-02T00:00:12","end_date":"2016-01-03T00:00:12","daily_pay":"0.100 TBD","subject":"0","permlink":"permlink0","extensions":[]}]})~"
      }, { // proposal_fee_operation
      R"~({"trx_id":"2309ebc61f5580e870fcbc982f03acc4614335a8","block":5,"trx_in_block":2,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:00:12","op":{"type":"proposal_fee_operation","value":{"creator":"alice7ah","treasury":"hive.fund","proposal_id":0,"fee":{"amount":"10000","precision":3,"nai":"@@000000013"}}},"operation_id":0})~",
      R"~({"trx_id":"2309ebc61f5580e870fcbc982f03acc4614335a8","block":5,"trx_in_block":2,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:00:12","op":["proposal_fee",{"creator":"alice7ah","treasury":"hive.fund","proposal_id":0,"fee":"10.000 TBD"}]})~"
      }, { // update_proposal_operation
      R"~({"trx_id":"4aa8d1fc867bfaf9c50f3272f5899a2b153d4500","block":5,"trx_in_block":3,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:12","op":{"type":"update_proposal_operation","value":{"proposal_id":0,"creator":"alice7ah","daily_pay":{"amount":"80","precision":3,"nai":"@@000000013"},"subject":"new subject","permlink":"permlink0","extensions":[]}},"operation_id":0})~",
      R"~({"trx_id":"4aa8d1fc867bfaf9c50f3272f5899a2b153d4500","block":5,"trx_in_block":3,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:12","op":["update_proposal",{"proposal_id":0,"creator":"alice7ah","daily_pay":"0.080 TBD","subject":"new subject","permlink":"permlink0","extensions":[]}]})~"
      }, { // update_proposal_votes_operation
      R"~({"trx_id":"edcfd57ce210cf5afd1227c63f9135de69b4e35e","block":5,"trx_in_block":4,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:12","op":{"type":"update_proposal_votes_operation","value":{"voter":"carol7ah","proposal_ids":[0],"approve":true,"extensions":[]}},"operation_id":0})~",
      R"~({"trx_id":"edcfd57ce210cf5afd1227c63f9135de69b4e35e","block":5,"trx_in_block":4,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:12","op":["update_proposal_votes",{"voter":"carol7ah","proposal_ids":[0],"approve":true,"extensions":[]}]})~"
      }, { // remove_proposal_operation
      R"~({"trx_id":"e7a3be6db61976dcb884009a0aad7b01ae5c5221","block":5,"trx_in_block":5,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:12","op":{"type":"remove_proposal_operation","value":{"proposal_owner":"alice7ah","proposal_ids":[0],"extensions":[]}},"operation_id":0})~",
      R"~({"trx_id":"e7a3be6db61976dcb884009a0aad7b01ae5c5221","block":5,"trx_in_block":5,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:12","op":["remove_proposal",{"proposal_owner":"alice7ah","proposal_ids":[0],"extensions":[]}]})~"
      }, { // producer_reward_operation
      R"~({"trx_id":"0000000000000000000000000000000000000000","block":5,"trx_in_block":4294967295,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:00:15","op":{"type":"producer_reward_operation","value":{"producer":"initminer","vesting_shares":{"amount":"8631556303","precision":6,"nai":"@@000000037"}}},"operation_id":0})~",
      R"~({"trx_id":"0000000000000000000000000000000000000000","block":5,"trx_in_block":4294967295,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:00:15","op":["producer_reward",{"producer":"initminer","vesting_shares":"8631.556303 VESTS"}]})~"
      } };
    expected_t expected_virtual_operations = { expected_operations[1], expected_operations[4], expected_operations[8] };
    test_get_ops_in_block( *this, expected_operations, expected_virtual_operations, 5 );
  };

  proposal_scenario( check_point_tester );

} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_CASE( get_ops_in_block_account )
{ try {

  BOOST_TEST_MESSAGE( "testing get_ops_in_block with account_scenario" );

  auto check_point_tester = [ this ]( uint32_t generate_no_further_than )
  {
    generate_until_irreversible_block( 65 );
    BOOST_REQUIRE( db->head_block_num() <= generate_no_further_than );

    expected_t expected_operations = { { // account_update_operation
      R"~({"trx_id":"c585a5c5811c8a0a9bbbdcccaf9fdd25d2cf4f2d","block":46,"trx_in_block":0,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:02:15","op":{"type":"account_update_operation","value":{"account":"alice8ah","memo_key":"TST6gpma8a74jnePfFaR5AeAncusvfEkKLQ6webzKdRLrxcuEsDDx","json_metadata":"\"{\"position\":\"top\"}\""}},"operation_id":0})~",
      R"~({"trx_id":"c585a5c5811c8a0a9bbbdcccaf9fdd25d2cf4f2d","block":46,"trx_in_block":0,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:02:15","op":["account_update",{"account":"alice8ah","memo_key":"TST6gpma8a74jnePfFaR5AeAncusvfEkKLQ6webzKdRLrxcuEsDDx","json_metadata":"\"{\"position\":\"top\"}\""}]})~"
      }, { // claim_account_operation
      R"~({"trx_id":"3e760e26dd8837a42b37f79b1e91ad015b20cf5e","block":46,"trx_in_block":1,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:02:15","op":{"type":"claim_account_operation","value":{"creator":"alice8ah","fee":{"amount":"0","precision":3,"nai":"@@000000021"},"extensions":[]}},"operation_id":0})~",
      R"~({"trx_id":"3e760e26dd8837a42b37f79b1e91ad015b20cf5e","block":46,"trx_in_block":1,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:02:15","op":["claim_account",{"creator":"alice8ah","fee":"0.000 TESTS","extensions":[]}]})~"
      }, { // create_claimed_account_operation
      R"~({"trx_id":"ef27e26c2043a1c9a74700284e65670ec0e4ab23","block":46,"trx_in_block":2,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:02:15","op":{"type":"create_claimed_account_operation","value":{"creator":"alice8ah","new_account_name":"ben8ah","owner":{"weight_threshold":1,"account_auths":[],"key_auths":[["TST7NVJSvcpYMSVkt1mzJ7uo8Ema7uwsuSypk9wjNjEK9cDyN6v3S",1]]},"active":{"weight_threshold":1,"account_auths":[],"key_auths":[["TST7NVJSvcpYMSVkt1mzJ7uo8Ema7uwsuSypk9wjNjEK9cDyN6v3S",1]]},"posting":{"weight_threshold":1,"account_auths":[],"key_auths":[["TST7F7N2n8RYwoBkS3rCtwDkaTdnbkctCm3V3fn2cDvdx988XMNZv",1]]},"memo_key":"TST7F7N2n8RYwoBkS3rCtwDkaTdnbkctCm3V3fn2cDvdx988XMNZv","json_metadata":"","extensions":[]}},"operation_id":0})~",
      R"~({"trx_id":"ef27e26c2043a1c9a74700284e65670ec0e4ab23","block":46,"trx_in_block":2,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:02:15","op":["create_claimed_account",{"creator":"alice8ah","new_account_name":"ben8ah","owner":{"weight_threshold":1,"account_auths":[],"key_auths":[["TST7NVJSvcpYMSVkt1mzJ7uo8Ema7uwsuSypk9wjNjEK9cDyN6v3S",1]]},"active":{"weight_threshold":1,"account_auths":[],"key_auths":[["TST7NVJSvcpYMSVkt1mzJ7uo8Ema7uwsuSypk9wjNjEK9cDyN6v3S",1]]},"posting":{"weight_threshold":1,"account_auths":[],"key_auths":[["TST7F7N2n8RYwoBkS3rCtwDkaTdnbkctCm3V3fn2cDvdx988XMNZv",1]]},"memo_key":"TST7F7N2n8RYwoBkS3rCtwDkaTdnbkctCm3V3fn2cDvdx988XMNZv","json_metadata":"","extensions":[]}]})~"
      }, { // account_created_operation
      R"~({"trx_id":"ef27e26c2043a1c9a74700284e65670ec0e4ab23","block":46,"trx_in_block":2,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:02:15","op":{"type":"account_created_operation","value":{"new_account_name":"ben8ah","creator":"alice8ah","initial_vesting_shares":{"amount":"0","precision":6,"nai":"@@000000037"},"initial_delegation":{"amount":"0","precision":6,"nai":"@@000000037"}}},"operation_id":0})~",
      R"~({"trx_id":"ef27e26c2043a1c9a74700284e65670ec0e4ab23","block":46,"trx_in_block":2,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:02:15","op":["account_created",{"new_account_name":"ben8ah","creator":"alice8ah","initial_vesting_shares":"0.000000 VESTS","initial_delegation":"0.000000 VESTS"}]})~"
      }, { // transfer_to_vesting_operation
      R"~({"trx_id":"58912caa28fd404f51c087d19700847341f98314","block":46,"trx_in_block":3,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:02:15","op":{"type":"transfer_to_vesting_operation","value":{"from":"initminer","to":"ben8ah","amount":{"amount":"1000000","precision":3,"nai":"@@000000021"}}},"operation_id":0})~",
      R"~({"trx_id":"58912caa28fd404f51c087d19700847341f98314","block":46,"trx_in_block":3,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:02:15","op":["transfer_to_vesting",{"from":"initminer","to":"ben8ah","amount":"1000.000 TESTS"}]})~"
      }, { // transfer_to_vesting_completed_operation
      R"~({"trx_id":"58912caa28fd404f51c087d19700847341f98314","block":46,"trx_in_block":3,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:02:15","op":{"type":"transfer_to_vesting_completed_operation","value":{"from_account":"initminer","to_account":"ben8ah","hive_vested":{"amount":"1000000","precision":3,"nai":"@@000000021"},"vesting_shares_received":{"amount":"908200580279667","precision":6,"nai":"@@000000037"}}},"operation_id":0})~",
      R"~({"trx_id":"58912caa28fd404f51c087d19700847341f98314","block":46,"trx_in_block":3,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:02:15","op":["transfer_to_vesting_completed",{"from_account":"initminer","to_account":"ben8ah","hive_vested":"1000.000 TESTS","vesting_shares_received":"908200580.279667 VESTS"}]})~"
      }, { // change_recovery_account_operation
      R"~({"trx_id":"049b20d348ac2a9aa04c58db40de22269b04cce5","block":46,"trx_in_block":4,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:02:15","op":{"type":"change_recovery_account_operation","value":{"account_to_recover":"ben8ah","new_recovery_account":"initminer","extensions":[]}},"operation_id":0})~",
      R"~({"trx_id":"049b20d348ac2a9aa04c58db40de22269b04cce5","block":46,"trx_in_block":4,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:02:15","op":["change_recovery_account",{"account_to_recover":"ben8ah","new_recovery_account":"initminer","extensions":[]}]})~"
      }, { // account_update2_operation
      R"~({"trx_id":"1e39af2a1265a8a658e1c0bb610e5e29bb31e030","block":46,"trx_in_block":5,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:02:15","op":{"type":"account_update2_operation","value":{"account":"ben8ah","owner":{"weight_threshold":1,"account_auths":[],"key_auths":[["TST5Wteiod1TC7Wraux73AZvMsjrA5b3E1LTsv1dZa3CB9V4LhXTN",1]]},"memo_key":"TST7NVJSvcpYMSVkt1mzJ7uo8Ema7uwsuSypk9wjNjEK9cDyN6v3S","json_metadata":"\"{\"success\":true}\"","posting_json_metadata":"","extensions":[]}},"operation_id":0})~",
      R"~({"trx_id":"1e39af2a1265a8a658e1c0bb610e5e29bb31e030","block":46,"trx_in_block":5,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:02:15","op":["account_update2",{"account":"ben8ah","owner":{"weight_threshold":1,"account_auths":[],"key_auths":[["TST5Wteiod1TC7Wraux73AZvMsjrA5b3E1LTsv1dZa3CB9V4LhXTN",1]]},"memo_key":"TST7NVJSvcpYMSVkt1mzJ7uo8Ema7uwsuSypk9wjNjEK9cDyN6v3S","json_metadata":"\"{\"success\":true}\"","posting_json_metadata":"","extensions":[]}]})~"
      }, { // request_account_recovery_operation
      R"~({"trx_id":"2a1f4e331c8d3451837dd4445e5310ce37a4a2b0","block":46,"trx_in_block":6,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:02:15","op":{"type":"request_account_recovery_operation","value":{"recovery_account":"alice8ah","account_to_recover":"ben8ah","new_owner_authority":{"weight_threshold":1,"account_auths":[],"key_auths":[["TST6gpma8a74jnePfFaR5AeAncusvfEkKLQ6webzKdRLrxcuEsDDx",1]]},"extensions":[]}},"operation_id":0})~",
      R"~({"trx_id":"2a1f4e331c8d3451837dd4445e5310ce37a4a2b0","block":46,"trx_in_block":6,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:02:15","op":["request_account_recovery",{"recovery_account":"alice8ah","account_to_recover":"ben8ah","new_owner_authority":{"weight_threshold":1,"account_auths":[],"key_auths":[["TST6gpma8a74jnePfFaR5AeAncusvfEkKLQ6webzKdRLrxcuEsDDx",1]]},"extensions":[]}]})~"
      }, { // recover_account_operation
      R"~({"trx_id":"2b9456a25d3e86df2cde6cfd3c77f445358b143b","block":46,"trx_in_block":7,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:02:15","op":{"type":"recover_account_operation","value":{"account_to_recover":"ben8ah","new_owner_authority":{"weight_threshold":1,"account_auths":[],"key_auths":[["TST6gpma8a74jnePfFaR5AeAncusvfEkKLQ6webzKdRLrxcuEsDDx",1]]},"recent_owner_authority":{"weight_threshold":1,"account_auths":[],"key_auths":[["TST7NVJSvcpYMSVkt1mzJ7uo8Ema7uwsuSypk9wjNjEK9cDyN6v3S",1]]},"extensions":[]}},"operation_id":0})~",
      R"~({"trx_id":"2b9456a25d3e86df2cde6cfd3c77f445358b143b","block":46,"trx_in_block":7,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:02:15","op":["recover_account",{"account_to_recover":"ben8ah","new_owner_authority":{"weight_threshold":1,"account_auths":[],"key_auths":[["TST6gpma8a74jnePfFaR5AeAncusvfEkKLQ6webzKdRLrxcuEsDDx",1]]},"recent_owner_authority":{"weight_threshold":1,"account_auths":[],"key_auths":[["TST7NVJSvcpYMSVkt1mzJ7uo8Ema7uwsuSypk9wjNjEK9cDyN6v3S",1]]},"extensions":[]}]})~"
      }, { // producer_reward_operation
      R"~({"trx_id":"0000000000000000000000000000000000000000","block":46,"trx_in_block":4294967295,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:02:18","op":{"type":"producer_reward_operation","value":{"producer":"initminer","vesting_shares":{"amount":"209791628548","precision":6,"nai":"@@000000037"}}},"operation_id":0})~",
      R"~({"trx_id":"0000000000000000000000000000000000000000","block":46,"trx_in_block":4294967295,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:02:18","op":["producer_reward",{"producer":"initminer","vesting_shares":"209791.628548 VESTS"}]})~"
      } };
    expected_t expected_virtual_operations = { expected_operations[3], expected_operations[5], expected_operations[10] };
    test_get_ops_in_block( *this, expected_operations, expected_virtual_operations, 46 );

    expected_operations = { { // producer_reward_operation
      R"~({"trx_id":"0000000000000000000000000000000000000000","block":65,"trx_in_block":4294967295,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:03:15","op":{"type":"producer_reward_operation","value":{"producer":"initminer","vesting_shares":{"amount":"209740354761","precision":6,"nai":"@@000000037"}}},"operation_id":0})~",
      R"~({"trx_id":"0000000000000000000000000000000000000000","block":65,"trx_in_block":4294967295,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:03:15","op":["producer_reward",{"producer":"initminer","vesting_shares":"209740.354761 VESTS"}]})~"
      }, { // changed_recovery_account_operation
      R"~({"trx_id":"0000000000000000000000000000000000000000","block":65,"trx_in_block":4294967295,"op_in_trx":2,"virtual_op":true,"timestamp":"2016-01-01T00:03:15","op":{"type":"changed_recovery_account_operation","value":{"account":"ben8ah","old_recovery_account":"alice8ah","new_recovery_account":"initminer"}},"operation_id":0})~",
      R"~({"trx_id":"0000000000000000000000000000000000000000","block":65,"trx_in_block":4294967295,"op_in_trx":2,"virtual_op":true,"timestamp":"2016-01-01T00:03:15","op":["changed_recovery_account",{"account":"ben8ah","old_recovery_account":"alice8ah","new_recovery_account":"initminer"}]})~"
      } };
    // Note that all operations of this block are virtual, hence we can reuse the same expected container here.
    test_get_ops_in_block( *this, expected_operations, expected_operations, 65 );
  };

  account_scenario( check_point_tester);

} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_CASE( get_ops_in_block_custom )
{ try {

  BOOST_TEST_MESSAGE( "testing get_ops_in_block with custom_scenario" );

  auto check_point_tester = [ this ]( uint32_t generate_no_further_than )
  {
    generate_until_irreversible_block( 4 );
    BOOST_REQUIRE( db->head_block_num() <= generate_no_further_than );

    expected_t expected_operations = { { // custom_operation
      R"~({"trx_id":"3e44a79c011431391ccb98dff11a0025ee25ecbf","block":4,"trx_in_block":0,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:09","op":{"type":"custom_operation","value":{"required_auths":["alice9ah"],"id":7,"data":"44415441"}},"operation_id":0})~",
      R"~({"trx_id":"3e44a79c011431391ccb98dff11a0025ee25ecbf","block":4,"trx_in_block":0,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:09","op":["custom",{"required_auths":["alice9ah"],"id":7,"data":"44415441"}]})~"
      }, { // custom_json_operation
      R"~({"trx_id":"79b98c81b10d32c2e284cb4b0d62ff609a14ed90","block":4,"trx_in_block":1,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:09","op":{"type":"custom_json_operation","value":{"required_auths":[],"required_posting_auths":["alice9ah"],"id":"7id","json":"\"{\"type\": \"json\"}\""}},"operation_id":0})~",
      R"~({"trx_id":"79b98c81b10d32c2e284cb4b0d62ff609a14ed90","block":4,"trx_in_block":1,"op_in_trx":0,"virtual_op":false,"timestamp":"2016-01-01T00:00:09","op":["custom_json",{"required_auths":[],"required_posting_auths":["alice9ah"],"id":"7id","json":"\"{\"type\": \"json\"}\""}]})~"
      }, { // producer_reward_operation
      R"~({"trx_id":"0000000000000000000000000000000000000000","block":4,"trx_in_block":4294967295,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:00:12","op":{"type":"producer_reward_operation","value":{"producer":"initminer","vesting_shares":{"amount":"8684058191","precision":6,"nai":"@@000000037"}}},"operation_id":0})~",
      R"~({"trx_id":"0000000000000000000000000000000000000000","block":4,"trx_in_block":4294967295,"op_in_trx":1,"virtual_op":true,"timestamp":"2016-01-01T00:00:12","op":["producer_reward",{"producer":"initminer","vesting_shares":"8684.058191 VESTS"}]})~"
      } };
    expected_t expected_virtual_operations = { expected_operations[2] };
    test_get_ops_in_block( *this, expected_operations, expected_virtual_operations, 4 );
  };

  custom_scenario( check_point_tester );

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
      R"~([10,{"trx_id":"0000000000000000000000000000000000000000","block":88,"trx_in_block":4294967295,"op_in_trx":3,"virtual_op":true,"timestamp":"2016-01-01T00:04:24","op":{"type":"fill_collateralized_convert_request_operation","value":{"owner":"carol3ah","requestid":0,"amount_in":{"amount":"11050","precision":3,"nai":"@@000000021"},"amount_out":{"amount":"10524","precision":3,"nai":"@@000000013"},"excess_collateral":{"amount":"11052","precision":3,"nai":"@@000000021"}}},"operation_id":0}])~",
      R"~([10,{"trx_id":"0000000000000000000000000000000000000000","block":88,"trx_in_block":4294967295,"op_in_trx":3,"virtual_op":true,"timestamp":"2016-01-01T00:04:24","op":["fill_collateralized_convert_request",{"owner":"carol3ah","requestid":0,"amount_in":"11.050 TESTS","amount_out":"10.524 TBD","excess_collateral":"11.052 TESTS"}]}])~"
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
      R"~([5,{"trx_id":"0000000000000000000000000000000000000000","block":88,"trx_in_block":4294967295,"op_in_trx":2,"virtual_op":true,"timestamp":"2016-01-01T00:04:24","op":{"type":"fill_convert_request_operation","value":{"owner":"edgar3ah","requestid":0,"amount_in":{"amount":"11201","precision":3,"nai":"@@000000013"},"amount_out":{"amount":"11201","precision":3,"nai":"@@000000021"}}},"operation_id":0}])~",
      R"~([5,{"trx_id":"0000000000000000000000000000000000000000","block":88,"trx_in_block":4294967295,"op_in_trx":2,"virtual_op":true,"timestamp":"2016-01-01T00:04:24","op":["fill_convert_request",{"owner":"edgar3ah","requestid":0,"amount_in":"11.201 TBD","amount_out":"11.201 TESTS"}]}])~"
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
  
  /*
  decline_voting_rights( "dan0ah", true, dan0ah_private_key );
  expected_operations.insert( { OP_TAG(decline_voting_rights_operation), fc::optional< expected_operation_result_t >() } );

  recurrent_transfer( "carol0ah", "dan0ah", ASSET( "0.037 TESTS" ), "With love", 24, 2, carol0ah_private_key );
  expected_operations.insert( { OP_TAG(recurrent_transfer_operation), fc::optional< expected_operation_result_t >() } );
  expected_operations.insert( { OP_TAG(fill_recurrent_transfer_operation), fc::optional< expected_operation_result_t >() } );

*/

} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_SUITE_END() // condenser_get_account_history_tests
#endif

