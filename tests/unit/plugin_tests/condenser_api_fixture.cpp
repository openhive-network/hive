#if defined IS_TEST_NET

#include "condenser_api_fixture.hpp"

#include <hive/plugins/account_history_api/account_history_api_plugin.hpp>
#include <hive/plugins/account_history_api/account_history_api.hpp>
#include <hive/plugins/database_api/database_api_plugin.hpp>
#include <hive/plugins/condenser_api/condenser_api_plugin.hpp>
#include <hive/plugins/condenser_api/condenser_api.hpp>

#include <boost/test/unit_test.hpp>

condenser_api_fixture::condenser_api_fixture()
{
  auto _data_dir = common_init( [&]( appbase::application& app, int argc, char** argv )
  {
    // Set cashout values to absolute safe minimum to speed up the scenarios and their tests.
    configuration_data.set_cashout_related_values( 0, 2, 4, 12, 1 );
    configuration_data.set_feed_related_values( 1, 3*24*7 ); // see comment to set_feed_related_values
    configuration_data.set_savings_related_values( 20 );
    configuration_data.set_min_recurrent_transfers_recurrence( 1 );
    configuration_data.set_generate_missed_block_operations( true );
    configuration_data.set_witness_shutdown_threshold( 0 );

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
    database_api = app.get_plugin< hive::plugins::database_api::database_api_plugin >().api;
    BOOST_REQUIRE( database_api );

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

  open_database( _data_dir/*, shared_file_size_in_mb_512*/ );

  generate_block();
  validate_database();
}

condenser_api_fixture::~condenser_api_fixture()
{
  try {
    configuration_data.reset_cashout_values();
    configuration_data.reset_feed_values();
    configuration_data.reset_savings_values();
    configuration_data.reset_recurrent_transfers_values();
    configuration_data.set_generate_missed_block_operations( false );
    configuration_data.set_witness_shutdown_threshold( HIVE_BLOCKS_PER_DAY ); // reset to default value

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

void condenser_api_fixture::hf1_scenario( check_point_tester_t check_point_tester )
{
  generate_block(); // block 1
  
  // Set first hardfork to test virtual operations that happen only there.
  db->set_hardfork( HIVE_HARDFORK_0_1 );
  generate_block(); // block 2

  check_point_tester( std::numeric_limits<uint32_t>::max() ); // <- no limit to max number of block generated inside.
}

void condenser_api_fixture::hf8_scenario( check_point_tester_t check_point_tester )
{
  db->set_hardfork( HIVE_HARDFORK_0_8 );
  generate_block(); // block 1
  
  ACTORS( (hf8alice)(hf8ben) );
  generate_block();

  fund( "hf8alice", ASSET( "2000.000 TESTS" ) );
  fund( "hf8ben", ASSET( "2000.000 TBD" ) );
  fund( "hf8alice", ASSET( "2000.000 TBD" ) );
  fund( "hf8ben", ASSET( "2000.000 TESTS" ) );
  generate_block();

  limit_order_create( "hf8alice", ASSET( "12.800 TESTS" ), ASSET( "13.300 TBD" ), false, fc::seconds( HIVE_MAX_LIMIT_ORDER_EXPIRATION ), 3, hf8alice_private_key );
  limit_order_create( "hf8alice", ASSET( "21.800 TBD" ), ASSET( "21.300 TESTS" ), false, fc::seconds( HIVE_MAX_LIMIT_ORDER_EXPIRATION ), 4, hf8alice_private_key );

  for( int i = 0; i<60; ++i ) // <- The number reduced by moving from HF11 back to HF8
    generate_block();

  limit_order_create( "hf8ben", ASSET( "0.650 TBD" ), ASSET( "0.400 TESTS" ), false, fc::seconds( HIVE_MAX_LIMIT_ORDER_EXPIRATION ), 1, hf8ben_private_key );
  limit_order_create( "hf8ben", ASSET( "11.650 TESTS" ), ASSET( "11.400 TBD" ), false, fc::seconds( HIVE_MAX_LIMIT_ORDER_EXPIRATION ), 3, hf8ben_private_key );

  // Regardless of the fixture configuration interest_operation & fill_order_operation appear in block 65 now.
  // liquidity_reward_operation shows up in block 1200, see HIVE_LIQUIDITY_REWARD_BLOCKS and
  // assertion in database::get_liquidity_reward().
  check_point_tester( std::numeric_limits<uint32_t>::max() ); // <- no limit to max number of block generated inside.
}

void condenser_api_fixture::hf12_scenario( check_point_tester_t check_point_tester )
{
  db->set_hardfork( HIVE_HARDFORK_0_12 );
  generate_block(); // block 1

  PREP_ACTOR( carol0ah )
  create_with_pow( "carol0ah", carol0ah_public_key, carol0ah_private_key );

  check_point_tester( std::numeric_limits<uint32_t>::max() ); // <- no limit to max number of block generated inside.
}

void condenser_api_fixture::hf13_scenario( check_point_tester_t check_point_1_tester, check_point_tester_t check_point_2_tester )
{
  db->set_hardfork( HIVE_HARDFORK_0_13 );
  vest( HIVE_INIT_MINER_NAME, HIVE_INIT_MINER_NAME, ASSET( "1000.000 TESTS" ) );
  generate_block();
  
  PREP_ACTOR( dan0ah )
  create_with_pow2( "dan0ah", dan0ah_public_key, dan0ah_private_key );

  PREP_ACTOR( edgar0ah )
  create_with_delegation( HIVE_INIT_MINER_NAME, "edgar0ah", edgar0ah_public_key, edgar0ah_post_key, ASSET( "100000000.000000 VESTS" ), init_account_priv_key );

  post_comment("edgar0ah", "permlink1", "Title 1", "Body 1", "parentpermlink1", edgar0ah_private_key);
  set_comment_options( "edgar0ah", "permlink1", ASSET( "50.010 TBD" ), HIVE_1_PERCENT * 51, false, true, comment_options_extensions_type(), edgar0ah_private_key );

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

void condenser_api_fixture::hf19_scenario( check_point_tester_t check_point_tester )
{
  db->set_hardfork( HIVE_HARDFORK_0_19 );
  generate_block();

  ACTORS( (alice19ah)(ben19ah) );
  generate_block();
  
  witness_create( "alice19ah", alice19ah_private_key, "foo.bar", alice19ah_private_key.get_public_key(), 1000 );
  witness_vote( "ben19ah", "alice19ah", ben19ah_private_key );

  // Now all the operations mentioned above can be checked. They can appear as early as 27th block, depending on configuration.
  check_point_tester( std::numeric_limits<uint32_t>::max() ); // <- no limit to max number of block generated inside.
}

void condenser_api_fixture::hf23_scenario( check_point_tester_t check_point_tester )
{
  db->set_hardfork( HIVE_HARDFORK_0_22 );
  generate_block();

  ACTORS( (steemflower) );
  generate_block();
  fund( "steemflower", ASSET( "123456789.012 TBD" ) );
  generate_block();

  // Trigger clear_null_account_balance_operation
  transfer( "steemflower", HIVE_NULL_ACCOUNT, ASSET( "0.012 TBD" ), "For flowers :)", steemflower_private_key );
  generate_block();

  // Trigger hardfork_hive_operation
  db->set_hardfork( HIVE_HARDFORK_0_23 );
  generate_block();

  // Trigger hardfork_hive_restore_operation & consolidate_treasury_balance_operation
  db->set_hardfork( HIVE_HARDFORK_1_24 );
  generate_block();

  // clear_null_account_balance_operation & hardfork_hive_operation appear in 5th block, while
  // hardfork_hive_restore_operation & consolidate_treasury_balance_operation in 6th block,
  // regardless of fixture or test configuration.
  check_point_tester( std::numeric_limits<uint32_t>::max() ); // <- no limit to max number of block generated inside.
}

void condenser_api_fixture::comment_and_reward_scenario( check_point_tester_t check_point_1_tester, check_point_tester_t check_point_2_tester )
{
  db->set_hardfork( HIVE_HARDFORK_1_28 );
  generate_block();
  
  ACTORS( (dan0ah)(edgar0ah) );

  beneficiary_route_type beneficiary( account_name_type( "dan0ah" ), HIVE_1_PERCENT*50 );
  comment_payout_beneficiaries beneficiaries;
  beneficiaries.beneficiaries = { beneficiary };
  comment_options_extensions_type extensions = { beneficiaries };

  post_comment("edgar0ah", "permlink1", "Title 1", "Body 1", "parentpermlink1", edgar0ah_private_key);
  set_comment_options( "edgar0ah", "permlink1", ASSET( "10000.000 TBD" ), HIVE_100_PERCENT, true, true, extensions, edgar0ah_private_key );
  vote("edgar0ah", "permlink1", "dan0ah", - HIVE_1_PERCENT * 100, dan0ah_private_key);
  vote("edgar0ah", "permlink1", "dan0ah", HIVE_1_PERCENT * 100, dan0ah_private_key); // Changed his mind

  // In check_point_1_tester generate as many blocks as needed for these virtual operations to appear in block:
  // curation_reward_operation, author_reward_operation, comment_benefactor_reward_operation,
  // comment_reward_operation, comment_payout_update_operation & producer_reward_operation.
  // In standard configuration of this fixture it's 6th block.
  check_point_1_tester( std::numeric_limits<uint32_t>::max() ); // <- no limit to max number of block generated inside.

  // The absolute minimum of claimed values is used here to allow greater flexibility for the tests using this scenario.
  claim_reward_balance( "edgar0ah", ASSET( "0.000 TESTS" ), ASSET( "0.001 TBD" ), ASSET( "0.000001 VESTS" ), edgar0ah_private_key );

  // In following block (which number depends on how many were generated in check_point_1_tester) these operations appear
  // and can be checked:  claim_reward_balance_operation & producer_reward_operation.
  check_point_2_tester( std::numeric_limits<uint32_t>::max() ); // <- no limit to max number of block generated inside.
}

void condenser_api_fixture::convert_and_limit_order_scenario( check_point_tester_t check_point_tester )
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
  limit_order2_create( "edgar3ah", ASSET( "22.075 TBD" ), price( ASSET( "0.010 TBD" ), ASSET( "0.010 TESTS" ) ), true, fc::seconds( HIVE_MAX_LIMIT_ORDER_EXPIRATION ), 3, edgar3ah_private_key );
  limit_order_cancel( "carol3ah", 1, carol3ah_private_key );

  // Now all the operations mentioned above can be checked. All of them will appear in 5th block,
  // except fill_convert_request_operation, fill_collateralized_convert_request_operation - their block number depends of test configuration.
  // In standard configuration of this fixture it's 88th block.
  check_point_tester( std::numeric_limits<uint32_t>::max() ); // <- no limit to max number of block generated inside.
}

void condenser_api_fixture::vesting_scenario( check_point_tester_t check_point_tester )
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

void condenser_api_fixture::witness_scenario( check_point_tester_t check_point_tester )
{
  db->set_hardfork( HIVE_HARDFORK_1_27 );
  generate_block();

  ACTORS( (alice5ah)(ben5ah)(carol5ah) );
  generate_block();
  
  witness_create( "alice5ah", alice5ah_private_key, "foo.bar", alice5ah_private_key.get_public_key(), 1000 );
  witness_feed_publish( "alice5ah", price( ASSET( "1.000 TBD" ), ASSET( "1.000 TESTS" ) ), alice5ah_private_key );
  proxy( "ben5ah", "carol5ah" );
  witness_vote( "carol5ah", "alice5ah", carol5ah_private_key, true ); // mistakenly voted
  witness_vote( "carol5ah", "alice5ah", carol5ah_private_key, false ); // fixed the mistake
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

void condenser_api_fixture::escrow_and_savings_scenario( check_point_tester_t check_point_tester )
{
  db->set_hardfork( HIVE_HARDFORK_1_27 );
  generate_block();

  ACTORS( (alice6ah)(ben6ah)(carol6ah) );
  generate_block();
  fund( "alice6ah", ASSET( "1.111 TESTS" ) );
  fund( "alice6ah", ASSET( "2.121 TBD" ) );
  generate_block();
  
  escrow_transfer( "alice6ah", "ben6ah", "carol6ah", ASSET( "0.071 TESTS" ), ASSET( "0.000 TBD" ), ASSET( "0.001 TESTS" ), "",
                  fc::seconds( HIVE_BLOCK_INTERVAL * 10 ), fc::seconds( HIVE_BLOCK_INTERVAL * 20 ), 30, alice6ah_private_key );
  escrow_transfer( "alice6ah", "ben6ah", "carol6ah", ASSET( "0.000 TESTS" ), ASSET( "0.007 TBD" ), ASSET( "0.001 TESTS" ), "{\"go\":\"now\"}",
                  fc::seconds( HIVE_BLOCK_INTERVAL * 10 ), fc::seconds( HIVE_BLOCK_INTERVAL * 20 ), 31, alice6ah_private_key );

  escrow_approve( "alice6ah", "ben6ah", "carol6ah", "carol6ah", true, 30, carol6ah_private_key );
  escrow_approve( "alice6ah", "ben6ah", "carol6ah", "ben6ah", true, 30, ben6ah_private_key );
  escrow_release( "alice6ah", "ben6ah", "carol6ah", "alice6ah", "ben6ah", ASSET( "0.013 TESTS" ), ASSET( "0.000 TBD" ), 30, alice6ah_private_key );
  escrow_dispute( "alice6ah", "ben6ah", "carol6ah", "ben6ah", 30, ben6ah_private_key );

  escrow_approve( "alice6ah", "ben6ah", "carol6ah", "carol6ah", false, 31, carol6ah_private_key );

  transfer_to_savings( "alice6ah", "ben6ah", ASSET( "0.009 TESTS" ), "ah savings", alice6ah_private_key );
  transfer_from_savings( "ben6ah", "alice6ah", ASSET( "0.006 TESTS" ), 0, ben6ah_private_key );
  transfer_from_savings( "ben6ah", "carol6ah", ASSET( "0.003 TESTS" ), 1, ben6ah_private_key );
  cancel_transfer_from_savings( "ben6ah", 0, ben6ah_private_key );

  // Now all the operations mentioned above can be checked. All of them will appear in 5th block,
  // except fill_transfer_from_savings_operation - its block number depends on test configuration.
  // In standard configuration of this fixture it's 11th block.
  check_point_tester( std::numeric_limits<uint32_t>::max() ); // <- no limit to max number of block generated inside.
}

void condenser_api_fixture::proposal_scenario( check_point_tester_t check_point_tester )
{
  db->set_hardfork( HIVE_HARDFORK_1_27 );
  generate_block();

  ACTORS( (alice7ah)(ben7ah)(carol7ah) );
  generate_block();
  fund( "alice7ah", ASSET( "800.000 TBD" ) );
  fund( "carol7ah", ASSET( "100000.000 TESTS") );
  generate_block();

  // Fill treasury handsomely so that the payout occurres early.
  transfer( "carol7ah", db->get_treasury_name(), ASSET( "30000.333 TESTS" ) ); // <- trigger dhf_conversion_operation

  // Create the proposal for the first time to be updated and removed.
  post_comment("alice7ah", "permlink0", "title", "body", "test", alice7ah_private_key);
  int64_t proposal_id = 
  create_proposal( "alice7ah", "ben7ah", "0" /*subject*/, "permlink0", db->head_block_time() - fc::days( 1 ), 
                    db->head_block_time() + fc::days( 2 ), asset( 100, HBD_SYMBOL ), alice7ah_private_key );
  const proposal_object* proposal = find_proposal( proposal_id );
  std::string subject( proposal->subject );
  std::string permlink( proposal->permlink );
  BOOST_REQUIRE_NE( proposal, nullptr );
  update_proposal( proposal_id, "alice7ah", asset( 80, HBD_SYMBOL ), "new subject", proposal->permlink, alice7ah_private_key);
  vote_proposal( "carol7ah", { proposal_id }, false/*approve*/, carol7ah_private_key);
  remove_proposal( "alice7ah", { proposal_id }, alice7ah_private_key );

  generate_block();

  // Create the same proposal again to be paid from treasury.
  proposal_id = create_proposal( "alice7ah", "ben7ah", subject, permlink, db->head_block_time() - fc::days( 1 ),
    db->head_block_time() + fc::days( 2 ), asset( 100, HBD_SYMBOL ), alice7ah_private_key );
  vote_proposal( "carol7ah", { proposal_id }, true/*approve*/, carol7ah_private_key);

  // All operations related to first proposal can be checked now in 5th block except dhf_funding_operation (2nd block),
  // regardless of the fixture configuration.
  // The virtual operations related to second proposal may occur as early as 32nd block (delayed_voting_operation) and
  // 42nd block (proposal_pay_operation), depending on test configuration.
  check_point_tester( std::numeric_limits<uint32_t>::max() ); // <- no limit to max number of block generated inside.
}

void condenser_api_fixture::account_scenario( check_point_tester_t check_point_tester )
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
  create_claimed_account( "alice8ah", "ben8ah", ben8ah_public_key, ben8ah_post_key.get_public_key(), R"~("{"go":"now"}")~", alice8ah_private_key );

  vest( HIVE_INIT_MINER_NAME, "ben8ah", ASSET( "1000.000 TESTS" ) );
  change_recovery_account( "ben8ah", HIVE_INIT_MINER_NAME, ben8ah_private_key );
  account_update2( "ben8ah", authority(1, carol8ah_public_key,1), fc::optional<authority>(), fc::optional<authority>(),
                    ben8ah_private_key.get_public_key(), R"~("{"success":true}")~", R"~("{"winner":"me"}")~", ben8ah_private_key );
  request_account_recovery( "alice8ah", "ben8ah", authority( 1, alice8ah_private_key.get_public_key(), 1 ), alice8ah_private_key );
  recover_account( "ben8ah", alice8ah_private_key, ben8ah_private_key );
  // accidentally make another request ...
  request_account_recovery( "alice8ah", "ben8ah", authority( 1, ben8ah_private_key.get_public_key(), 1 ), alice8ah_private_key );
  // ... to cancel it immediately
  request_account_recovery( "alice8ah", "ben8ah", authority( 0, ben8ah_private_key.get_public_key(), 0 ), alice8ah_private_key );
  // accidentally change recovery account ...
  change_recovery_account( "alice8ah", "ben8ah", alice8ah_private_key );
  // ... to cancel it immediately
  change_recovery_account( "alice8ah", HIVE_INIT_MINER_NAME, alice8ah_private_key );

  // Now all the operations mentioned above can be checked. All of them will appear in 46th block,
  // except changed_recovery_account_operation - its block number depends on test configuration.
  // In standard configuration of this fixture it's 65th block.
  check_point_tester( std::numeric_limits<uint32_t>::max() ); // <- no limit to max number of block generated inside.
}

void condenser_api_fixture::custom_scenario( check_point_tester_t check_point_tester )
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

void condenser_api_fixture::recurrent_transfer_scenario( check_point_tester_t check_point_tester )
{
  db->set_hardfork( HIVE_HARDFORK_1_27 );
  generate_block();

  ACTORS( (alice10ah)(ben10ah) );
  generate_block();
  fund( "alice10ah", ASSET("0.040 TESTS") );
  fund( "ben10ah", ASSET("13.777 TBD") );
  generate_block();

  recurrent_transfer( "alice10ah", "ben10ah", ASSET( "0.037 TESTS" ), "With love", 1, 2, alice10ah_private_key );
  recurrent_transfer( "ben10ah", "alice10ah", ASSET( "7.713 TBD" ), "", 2, 4, ben10ah_private_key );
  recurrent_transfer( "ben10ah", "alice10ah", ASSET( "0.000 TBD" ), "", 3, 7, ben10ah_private_key );

  // The operations mentioned above can be checked now in 5th block, except 
  // failed_recurrent_transfer_operation - its block number depends on test configuration.
  // In standard configuration of this fixture it's 1204th block.
  check_point_tester( std::numeric_limits<uint32_t>::max() ); // <- no limit to max number of block generated inside.
}

void condenser_api_fixture::decline_voting_rights_scenario( check_point_tester_t check_point_tester )
{
  db->set_hardfork( HIVE_HARDFORK_1_28 );
  generate_block();

  ACTORS( (alice11ah)(ben11ah) );
  generate_block();

  proxy( "alice11ah", "ben11ah" );
  decline_voting_rights( "alice11ah", true, alice11ah_private_key );

  // decline_voting_rights_operation can be checked in 5th block and declined_voting_rights_operation in the 23rd,
  // regardless of the fixture configuration.
  check_point_tester( std::numeric_limits<uint32_t>::max() ); // <- no limit to max number of block generated inside.
}

void condenser_api_fixture::combo_1_scenario( check_point_tester_t check_point_tester1, check_point_tester_t check_point_tester2 )
{
  db->set_hardfork( HIVE_HARDFORK_1_27 );
  generate_block();

  PREP_ACTOR( alice12ah );
  account_create( "alice12ah", alice12ah_public_key );
  post_comment( "alice12ah", "permlink12-1", "Title 12-1", "Body 12-1", "parentpermlink12", alice12ah_private_key );

  PREP_ACTOR( ben12ah );
  account_create( "ben12ah", "alice12ah", alice12ah_private_key, 0, ben12ah_public_key, ben12ah_public_key, "{\"relation\":\"sibling\"}" );
  delegate_vest( "alice12ah", "ben12ah", asset( 1507, VESTS_SYMBOL ), alice12ah_private_key );
  post_comment_to_comment( "ben12ah", "permlink12-2", "Title 12-1", "Body 12-1", "alice12ah", "permlink12-1", ben12ah_private_key );

  decline_voting_rights( "alice12ah", true, alice12ah_private_key );

  check_point_tester1( std::numeric_limits<uint32_t>::max() ); // <- no limit to max number of block generated inside.

  decline_voting_rights( "alice12ah", false, alice12ah_private_key ); // Changed her mind

  check_point_tester2( std::numeric_limits<uint32_t>::max() ); // <- no limit to max number of block generated inside.
}

#endif
