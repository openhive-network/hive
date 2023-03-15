#if defined IS_TEST_NET

#include "condenser_api_fixture.hpp"

#include <hive/plugins/database_api/database_api_plugin.hpp>
#include <hive/plugins/condenser_api/condenser_api_plugin.hpp>
#include <hive/plugins/condenser_api/condenser_api.hpp>

#include <boost/test/unit_test.hpp>

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


#endif
