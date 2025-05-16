#if defined IS_TEST_NET
#include <boost/test/unit_test.hpp>
#include <hive/plugins/database_api/database_api_plugin.hpp>
#include <hive/plugins/database_api/database_api.hpp>

#include "../db_fixture/hived_fixture.hpp"

using namespace hive::chain;
using namespace hive::protocol;

template<uint32_t HARDFORK_NUMBER>
struct database_api_fixture_basic : hived_fixture
{
  database_api_fixture_basic()
  {
    configuration_data.set_initial_asset_supply( INITIAL_TEST_SUPPLY, HBD_INITIAL_TEST_SUPPLY );

    hive::plugins::database_api::database_api_plugin* db_api_plugin = nullptr;
    postponed_init(
      {
        config_line_t( { "plugin", { HIVE_DATABASE_API_PLUGIN_NAME } } ),
        config_line_t( { "shared-file-size",
          { std::to_string( 1024 * 1024 * shared_file_size_small ) } }
        )
      },
      &db_api_plugin
    );

    database_api = db_api_plugin->api.get();
    BOOST_REQUIRE( database_api );

    init_account_pub_key = init_account_priv_key.get_public_key();

    generate_block();
    db->set_hardfork( HARDFORK_NUMBER );
    generate_block();
    db->_log_hardforks = true;

    ACTORS((top1)(top2)(top3)(top4)(top5)(top6)(top7)(top8)(top9)(top10)
           (top11)(top12)(top13)(top14)(top15)(top16)(top17)(top18)(top19)(top20))
    ACTORS((backup1)(backup2)(backup3)(backup4)(backup5)(backup6)(backup7)(backup8)(backup9)(backup10))

    const auto create_witness = [&]( const std::string& witness_name )
    {
      const private_key_type account_key = generate_private_key( witness_name );
      const private_key_type witness_key = generate_private_key( witness_name + "_witness" );
      witness_create( witness_name, account_key, witness_name + ".com", witness_key.get_public_key(), 1000 );
      witness_plugin->add_signing_key( witness_key );
    };
    for( int i = 1; i <= 20; ++i )
      create_witness( "top" + std::to_string(i) );
    for( int i = 1; i <= 10; ++i )
      create_witness( "backup" + std::to_string(i) );

    ACTORS((whale)(voter1)(voter2)(voter3)(voter4)(voter5)(voter6)(voter7)(voter8)(voter9)(voter10))

    vest( "whale", ASSET( "500000.000 TESTS" ) );

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
      auto key = generate_private_key( name );
      fund( name, ASSET( "10000.000 TESTS" ) );
      vest( name, "", asset( 10000000 / i, HIVE_SYMBOL ), key );
      op.account = name;
      for( int v = 1; v <= i; ++v )
      {
        op.witness = "backup" + std::to_string(v);
        push_transaction( op, key );
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

  hive::plugins::database_api::database_api* database_api = nullptr;
};

using database_api_fixture = database_api_fixture_basic<HIVE_NUM_HARDFORKS>;
using database_api_fixture_27 = database_api_fixture_basic<27>;

BOOST_FIXTURE_TEST_SUITE( database_api_tests, database_api_fixture );

BOOST_AUTO_TEST_CASE( get_witness_schedule_test )
{ try {

  BOOST_TEST_MESSAGE( "get_active_witnesses / get_witness_schedule test" );

  auto scheduled_witnesses_1 = database_api->get_active_witnesses( { false } );
  auto active_schedule_1 = database_api->get_witness_schedule( { false } );
  ilog( "initial active schedule: ${active_schedule_1}", ( active_schedule_1 ) );
  auto all_scheduled_witnesses_1 = database_api->get_active_witnesses( { true } );
  auto full_schedule_1 = database_api->get_witness_schedule( { true } );
  ilog( "initial full schedule: ${full_schedule_1}", ( full_schedule_1 ) );

  BOOST_REQUIRE( active_schedule_1.future_changes.valid() == false );
  BOOST_REQUIRE_EQUAL( active_schedule_1.current_shuffled_witnesses.size(), HIVE_MAX_WITNESSES );
  BOOST_REQUIRE( active_schedule_1.future_shuffled_witnesses.valid() == false );
  BOOST_REQUIRE_EQUAL( scheduled_witnesses_1.witnesses.size(), HIVE_MAX_WITNESSES );
  BOOST_REQUIRE( scheduled_witnesses_1.future_witnesses.valid() == false );
  for( int i = 0; i < HIVE_MAX_WITNESSES; ++i )
    BOOST_REQUIRE( active_schedule_1.current_shuffled_witnesses[i] == scheduled_witnesses_1.witnesses[i] );
  BOOST_REQUIRE_GT( active_schedule_1.next_shuffle_block_num, db->head_block_num() );
  BOOST_REQUIRE_EQUAL( active_schedule_1.next_shuffle_block_num, full_schedule_1.next_shuffle_block_num );
  BOOST_REQUIRE_EQUAL( full_schedule_1.current_shuffled_witnesses.size(), HIVE_MAX_WITNESSES );
  BOOST_REQUIRE( full_schedule_1.future_shuffled_witnesses.valid() == true );
  BOOST_REQUIRE_EQUAL( full_schedule_1.future_shuffled_witnesses->size(), HIVE_MAX_WITNESSES );
  BOOST_REQUIRE_EQUAL( all_scheduled_witnesses_1.witnesses.size(), HIVE_MAX_WITNESSES );
  BOOST_REQUIRE( all_scheduled_witnesses_1.future_witnesses.valid() == true );
  BOOST_REQUIRE_EQUAL( all_scheduled_witnesses_1.future_witnesses->size(), HIVE_MAX_WITNESSES );
  for( int i = 0; i < HIVE_MAX_WITNESSES; ++i )
  {
    BOOST_REQUIRE( full_schedule_1.current_shuffled_witnesses[i] == all_scheduled_witnesses_1.witnesses[i] );
    BOOST_REQUIRE( full_schedule_1.future_shuffled_witnesses.value()[i] == all_scheduled_witnesses_1.future_witnesses.value()[i] );
  }
  BOOST_REQUIRE( full_schedule_1.future_changes.valid() == true );
  BOOST_REQUIRE( full_schedule_1.future_changes->majority_version.valid() == true );
  BOOST_REQUIRE( full_schedule_1.future_changes->majority_version.value() > active_schedule_1.majority_version );

  generate_blocks( db->head_block_time() + fc::seconds( 3 * 21 ), false ); // one full schedule

  auto active_schedule_2 = database_api->get_witness_schedule( { false } );
  ilog( " active schedule: ${active_schedule_2}", ( active_schedule_2 ) );
  auto full_schedule_2 = database_api->get_witness_schedule( { true } );
  ilog( "initial full schedule: ${full_schedule_2}", ( full_schedule_2 ) );

  BOOST_REQUIRE( active_schedule_2.future_changes.valid() == false );
  BOOST_REQUIRE( active_schedule_2.current_virtual_time > active_schedule_1.current_virtual_time );
  BOOST_REQUIRE_EQUAL( active_schedule_2.next_shuffle_block_num, active_schedule_1.next_shuffle_block_num + HIVE_MAX_WITNESSES );
  BOOST_REQUIRE_EQUAL( active_schedule_2.num_scheduled_witnesses, active_schedule_1.num_scheduled_witnesses );
  BOOST_REQUIRE_EQUAL( active_schedule_2.elected_weight, active_schedule_1.elected_weight );
  BOOST_REQUIRE_EQUAL( active_schedule_2.timeshare_weight, active_schedule_1.timeshare_weight );
  BOOST_REQUIRE_EQUAL( active_schedule_2.miner_weight, active_schedule_1.miner_weight );
  BOOST_REQUIRE_EQUAL( active_schedule_2.witness_pay_normalization_factor, active_schedule_1.witness_pay_normalization_factor );
  BOOST_REQUIRE( active_schedule_2.median_props.account_creation_fee == active_schedule_1.median_props.account_creation_fee );
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
    BOOST_REQUIRE_EQUAL( active_schedule_2.current_shuffled_witnesses[i], full_schedule_1.future_shuffled_witnesses.value()[i] );
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
  auto full_schedule = database_api->get_witness_schedule( { true } );

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
  full_schedule = database_api->get_witness_schedule( { true } );

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

BOOST_AUTO_TEST_CASE( verify_account_authority_test )
{ try {
  auto fee = db->get_witness_schedule_object().median_props.account_creation_fee;

#define KEYS( name ) \
  auto name ## _owner = generate_private_key( BOOST_PP_STRINGIZE( name ) "_owner" ).get_public_key(); \
  auto name ## _active = generate_private_key( BOOST_PP_STRINGIZE( name ) "_active" ).get_public_key(); \
  auto name ## _posting = generate_private_key( BOOST_PP_STRINGIZE( name ) "_posting" ).get_public_key(); \
  auto name ## _memo = generate_private_key( BOOST_PP_STRINGIZE( name ) "_memo" ).get_public_key()
#define CREATE_ACCOUNT( name ) \
  do { \
    account_create_operation op; \
    op.creator = HIVE_INIT_MINER_NAME; \
    op.fee = fee; \
    op.new_account_name = BOOST_PP_STRINGIZE( name ); \
    op.owner = authority( 1, name ## _owner, 1 ); \
    op.active = authority( 1, name ## _active, 1 ); \
    op.posting = authority( 1, name ## _posting, 1 ); \
    op.memo_key = name ## _memo; \
    push_transaction( op, init_account_priv_key ); \
  } while( 0 )

  KEYS( single1 );
  KEYS( single2 );
  KEYS( multi1 );
  KEYS( multi2 );
  KEYS( multi3 );
  KEYS( alice );
  KEYS( bob );
  KEYS( carol );

  CREATE_ACCOUNT( alice );
  CREATE_ACCOUNT( bob );
  CREATE_ACCOUNT( carol );
  {
    account_create_operation op;
    op.creator = HIVE_INIT_MINER_NAME;
    op.fee = fee;
    op.new_account_name = "single";
    op.owner = authority( 1, single1_owner, 1, single2_owner, 1, "alice", 1, "bob", 1 );
    op.active = authority( 1, single1_active, 1, single2_active, 1, "alice", 1, "bob", 1 );
    op.posting = authority( 1, single1_posting, 1, single2_posting, 1, "alice", 1, "bob", 1 );
    op.memo_key = single1_memo;
    push_transaction( op, init_account_priv_key );
  }
  {
    account_create_operation op;
    op.creator = HIVE_INIT_MINER_NAME;
    op.fee = fee;
    op.new_account_name = "open";
    op.owner = authority();
    op.active = authority();
    op.posting = authority();
    op.memo_key = multi3_memo; // has to have some key
    push_transaction( op, init_account_priv_key );
  }
  {
    account_create_operation op;
    op.creator = HIVE_INIT_MINER_NAME;
    op.fee = fee;
    op.new_account_name = "multi";
    op.owner = authority( 3, multi1_owner, 1, multi2_owner, 1, multi3_owner, 1,
      "alice", 2, "bob", 2, "carol", 2 );
    op.active = authority( 3, multi1_active, 1, multi2_active, 1, multi3_active, 1,
      "alice", 2, "bob", 2, "carol", 2 );
    op.posting = authority( 3, multi1_posting, 1, multi2_posting, 1, multi3_posting, 1,
      "alice", 2, "bob", 2, "carol", 2 );
    op.memo_key = multi1_memo;
    push_transaction( op, init_account_priv_key );
  }
  generate_block();

#undef KEYS
#undef CREATE_ACCOUNT

  using namespace hive::plugins::database_api;

  auto OK = [&]( const account_name_type& actor, const flat_set< public_key_type >& sig_keys, authority_level level )
  {
    BOOST_CHECK( database_api->verify_account_authority( { actor, sig_keys, level } ).valid );
  };
  auto FAIL = [&]( const account_name_type& actor, const flat_set< public_key_type >& sig_keys, authority_level  level )
  {
    BOOST_CHECK( not database_api->verify_account_authority( { actor, sig_keys, level } ).valid );
  };

  BOOST_TEST_MESSAGE( "Testing database_api::verify_account_authority on regular account" );

  // alice can sign posting with only posting
  OK( "alice", { alice_posting }, authority_level::posting );
  // alice can't sign posting with active/owner
  FAIL( "alice", { alice_active }, authority_level::posting );
  FAIL( "alice", { alice_owner }, authority_level::posting );
  // can't sign with memo, with other account key, with no keys
  FAIL( "alice", { alice_memo }, authority_level::posting );
  FAIL( "alice", { bob_posting }, authority_level::posting );
  FAIL( "alice", {}, authority_level::posting );
  //redundant keys are allowed
  OK( "alice", { alice_active, alice_posting }, authority_level::posting );

  // alice can sign active with active
  OK( "alice", { alice_active }, authority_level::active );
  // alice can't sign active with owner
  FAIL( "alice", { alice_owner }, authority_level::active );
  // can't sign with posting/memo, with other account key, with no keys
  FAIL( "alice", { alice_posting }, authority_level::active );
  FAIL( "alice", { alice_memo }, authority_level::active );
  FAIL( "alice", { bob_active }, authority_level::active );
  FAIL( "alice", {}, authority_level::active );
  //redundant keys are allowed
  OK( "alice", { alice_active, alice_owner }, authority_level::active );

  // alice can sign owner with owner
  OK( "alice", { alice_owner }, authority_level::owner );
  // can't sign posting/active/memo, with other account key, with no keys
  FAIL( "alice", { alice_posting }, authority_level::owner );
  FAIL( "alice", { alice_active }, authority_level::owner );
  FAIL( "alice", { alice_memo }, authority_level::owner );
  FAIL( "alice", { bob_active }, authority_level::owner );
  FAIL( "alice", {}, authority_level::owner );
  //redundant keys are allowed
  OK( "alice", { alice_owner, alice_active }, authority_level::owner );

  BOOST_TEST_MESSAGE( "Testing database_api::verify_account_authority on account with alternative keys" );

  // single can sign posting with posting in both versions as well as with posting of alice/bob
  OK( "single", { single1_posting }, authority_level::posting );
  OK( "single", { single2_posting }, authority_level::posting );
  OK( "single", { alice_posting }, authority_level::posting );
  OK( "single", { bob_posting }, authority_level::posting );
  // single can't sign posting with active/owner in both versions
  FAIL( "single", { single1_active }, authority_level::posting );
  FAIL( "single", { single2_active }, authority_level::posting );
  FAIL( "single", { single1_owner }, authority_level::posting );
  FAIL( "single", { single2_owner }, authority_level::posting );
  // can't sign with memo, with unrelated account key, with no keys
  FAIL( "single", { single1_memo }, authority_level::posting );
  FAIL( "single", { carol_posting }, authority_level::posting );
  FAIL( "single", {}, authority_level::posting );
  //redundant keys are allowed
  OK( "single", { single1_posting, single2_posting }, authority_level::posting );
  OK( "single", { single1_posting, alice_posting }, authority_level::posting );
  OK( "single", { alice_posting, bob_posting }, authority_level::posting );
  // NOTE: can't sign with active/owner of alice/bob
  FAIL( "single", { alice_active }, authority_level::posting );
  FAIL( "single", { bob_active }, authority_level::posting );
  FAIL( "single", { alice_owner }, authority_level::posting );
  FAIL( "single", { bob_owner }, authority_level::posting );

  // single can sign active with active in both versions as well as with active of alice/bob
  OK( "single", { single1_active }, authority_level::active );
  OK( "single", { single2_active }, authority_level::active );
  OK( "single", { alice_active }, authority_level::active );
  OK( "single", { bob_active }, authority_level::active );
  // single can't sign active with owner
  FAIL( "single", { single1_owner }, authority_level::active );
  FAIL( "single", { single2_owner }, authority_level::active );
  // can't sign with posting/memo, with unrelated account key, with no keys
  FAIL( "single", { single1_posting }, authority_level::active );
  FAIL( "single", { single2_posting }, authority_level::active );
  FAIL( "single", { single1_memo }, authority_level::active );
  FAIL( "single", { carol_active }, authority_level::active );
  FAIL( "single", {}, authority_level::active );
  //redundant keys are allowed
  OK( "single", { single1_active, single2_active }, authority_level::active );
  OK( "single", { single1_active, alice_active }, authority_level::active );
  OK( "single", { alice_active, bob_active }, authority_level::active );
  // NOTE: can't sign with owner of alice/bob (can't sign with posting either but that is normal)
  FAIL( "single", { alice_owner }, authority_level::active );
  FAIL( "single", { bob_owner }, authority_level::active );
  FAIL( "single", { alice_posting }, authority_level::active );
  FAIL( "single", { bob_posting }, authority_level::active );

  // single can sign owner with owner in both versions as well as with active(!) of alice/bob
  OK( "single", { single1_owner }, authority_level::owner );
  OK( "single", { single2_owner }, authority_level::owner );
  OK( "single", { alice_active }, authority_level::owner );
  OK( "single", { bob_active }, authority_level::owner );
  // can't sign with posting/active/memo, with unrelated account key, with with no keys
  FAIL( "single", { single1_posting }, authority_level::owner );
  FAIL( "single", { single2_posting }, authority_level::owner );
  FAIL( "single", { single1_active }, authority_level::owner );
  FAIL( "single", { single2_active }, authority_level::owner );
  FAIL( "single", { single1_memo }, authority_level::owner );
  FAIL( "single", { carol_active }, authority_level::owner );
  FAIL( "single", {}, authority_level::owner );
  //redundant keys are allowed
  OK( "single", { single1_owner, single2_owner }, authority_level::owner );
  OK( "single", { single1_owner, alice_active }, authority_level::owner );
  OK( "single", { alice_active, bob_active }, authority_level::owner );
  // NOTE: can't sign with owner of alice/bob (also can't sign with posting, but that's expected)
  FAIL( "single", { alice_owner }, authority_level::owner );
  FAIL( "single", { bob_owner }, authority_level::owner );
  FAIL( "single", { alice_posting }, authority_level::owner );
  FAIL( "single", { bob_posting }, authority_level::owner );

  BOOST_TEST_MESSAGE( "Testing database_api::verify_account_authority on account with open authority" );

  // open can sign posting with no keys
  OK( "open", {}, authority_level::posting );
  OK( "open", { single1_memo }, authority_level::posting );
  OK( "open", { alice_posting }, authority_level::posting );
  OK( "open", { bob_active }, authority_level::posting );
  OK( "open", { carol_owner }, authority_level::posting );

  // open can sign active with no keys
  OK( "open", {}, authority_level::active );
  OK( "open", { single1_memo }, authority_level::active );
  OK( "open", { alice_posting }, authority_level::active );
  OK( "open", { bob_active }, authority_level::active );
  OK( "open", { carol_owner }, authority_level::active );

  // open can sign owner with no keys
  OK( "open", {}, authority_level::owner );
  OK( "open", { single1_memo }, authority_level::owner );
  OK( "open", { alice_posting }, authority_level::owner );
  OK( "open", { bob_active }, authority_level::owner );
  OK( "open", { carol_owner }, authority_level::owner );

  BOOST_TEST_MESSAGE( "Testing database_api::verify_account_authority on account with multisig authority" );

  // multi can sign posting with all 3 posting (but not mixed) as well as with
  // two posting of alice/bob/carol
  OK( "multi", { multi1_posting, multi2_posting, multi3_posting }, authority_level::posting );
  OK( "multi", { alice_posting, bob_posting }, authority_level::posting );
  OK( "multi", { alice_posting, carol_posting }, authority_level::posting );
  OK( "multi", { bob_posting, carol_posting }, authority_level::posting );
  OK( "multi", { multi1_posting, alice_posting }, authority_level::posting );
  // multi can't sign posting with all 3 active/owner (but not mixed) as well as with
  // one own active/owner key and one posting of alice/bob/carol
  FAIL( "multi", { multi1_active, multi2_active, multi3_active }, authority_level::posting );
  FAIL( "multi", { multi1_owner, multi2_owner, multi3_owner }, authority_level::posting );
  FAIL( "multi", { multi2_active, bob_posting }, authority_level::posting );
  FAIL( "multi", { multi3_owner, carol_posting }, authority_level::posting );
  // it is ok to sign with two own posting and external posting, but only because of order of
  // checks (if order was different, then it would reach threshold without one of own keys making
  // its use superfluous)
  OK( "multi", { multi1_posting, multi2_posting, alice_posting }, authority_level::posting );
  // can't sign with memo, with unrelated account key, with too few
  // can't mix keys with different strength either
  FAIL( "multi", { multi1_memo }, authority_level::posting );
  FAIL( "multi", { single1_posting }, authority_level::posting );
  FAIL( "multi", { multi1_posting, multi2_posting }, authority_level::posting );
  FAIL( "multi", {}, authority_level::posting );
  FAIL( "multi", { multi1_posting, multi2_active, multi3_owner }, authority_level::posting );
  //redundant keys are allowed
  OK( "multi", { multi1_posting, alice_posting, bob_posting }, authority_level::posting );
  // NOTE: can't sign with active/owner of alice/bob/carol
  FAIL( "multi", { multi1_posting, bob_active }, authority_level::posting );
  FAIL( "multi", { multi1_active, bob_active }, authority_level::posting );
  FAIL( "multi", { multi1_owner, bob_active }, authority_level::posting );
  FAIL( "multi", { alice_posting, bob_active }, authority_level::posting );
  FAIL( "multi", { alice_active, bob_active }, authority_level::posting );
  FAIL( "multi", { multi1_posting, bob_owner }, authority_level::posting );
  FAIL( "multi", { multi1_active, bob_owner }, authority_level::posting );
  FAIL( "multi", { multi1_owner, bob_owner }, authority_level::posting );
  FAIL( "multi", { alice_posting, bob_owner }, authority_level::posting );
  FAIL( "multi", { alice_owner, bob_owner }, authority_level::posting );

  // multi can sign active with all 3 active (but not mixed) as well as with
  // two active of alice/bob/carol or one own active key and one active of alice/bob/carol
  OK( "multi", { multi1_active, multi2_active, multi3_active }, authority_level::active );
  OK( "multi", { alice_active, bob_active }, authority_level::active );
  OK( "multi", { alice_active, carol_active }, authority_level::active );
  OK( "multi", { bob_active, carol_active }, authority_level::active );
  OK( "multi", { multi1_active, bob_active }, authority_level::active );
  // multi can't sign active with all 3 owner (but not mixed) as well as with one own owner key and one active of alice/bob/carol
  FAIL( "multi", { multi1_owner, multi2_owner, multi3_owner }, authority_level::active );
  FAIL( "multi", { multi2_owner, carol_active }, authority_level::active );
  // it is ok to sign with two own active and external active, but only because of order of
  // checks (if order was different, then it would reach threshold without one of own keys making
  // its use superfluous)
  OK( "multi", { multi1_active, multi2_active, alice_active }, authority_level::active );
  // can't sign with posting/memo, with unrelated account key, with too few
  // can't mix keys with different strength either
  FAIL( "multi", { multi1_memo }, authority_level::active );
  FAIL( "multi", { single1_active }, authority_level::active );
  FAIL( "multi", { multi1_active, multi2_active }, authority_level::active );
  FAIL( "multi", {}, authority_level::active );
  FAIL( "multi", { multi1_posting, multi2_posting, multi3_posting }, authority_level::active );
  FAIL( "multi", { multi1_posting, alice_posting }, authority_level::active );
  FAIL( "multi", { multi1_posting, multi2_active, multi3_owner }, authority_level::active );
  FAIL( "multi", { multi1_active, multi2_owner, multi3_active }, authority_level::active );
  //redundant kays are allowed
  OK( "multi", { multi1_active, alice_active, bob_active }, authority_level::active );
  // NOTE: can't sign with owner of alice/bob/carol
  FAIL( "multi", { multi1_active, bob_owner }, authority_level::active );
  FAIL( "multi", { multi1_owner, bob_owner }, authority_level::active );
  FAIL( "multi", { alice_active, bob_owner }, authority_level::active );
  FAIL( "multi", { alice_owner, bob_owner }, authority_level::active );

  // multi can sign owner with all 3 owner as well as with two active(!) of alice/bob/carol or
  // one own owner key and one active of alice/bob/carol
  OK( "multi", { multi1_owner, multi2_owner, multi3_owner }, authority_level::owner );
  OK( "multi", { alice_active, bob_active }, authority_level::owner );
  OK( "multi", { alice_active, carol_active }, authority_level::owner );
  OK( "multi", { bob_active, carol_active }, authority_level::owner );
  OK( "multi", { multi1_owner, bob_active }, authority_level::owner );
  // it is ok to sign with two own owner and external active, but only because of order of
  // checks (if order was different, then it would reach threshold without one of own keys making
  // its use superfluous)
  OK( "multi", { multi1_owner, multi2_owner, alice_active }, authority_level::owner );
  // can't sign with posting/active/memo, with unrelated account key, with too few
  // valid keys; can't mix keys with different strength either
  FAIL( "multi", { multi1_memo }, authority_level::owner );
  FAIL( "multi", { single1_active }, authority_level::owner );
  FAIL( "multi", { multi1_owner, multi2_owner }, authority_level::owner );
  FAIL( "multi", {}, authority_level::owner );
  FAIL( "multi", { multi1_posting, multi2_posting, multi3_posting }, authority_level::owner );
  FAIL( "multi", { multi1_active, multi2_active, multi3_active }, authority_level::owner );
  FAIL( "multi", { multi1_owner, alice_posting }, authority_level::owner );
  FAIL( "multi", { multi1_posting, multi2_active, multi3_owner }, authority_level::owner );
  FAIL( "multi", { multi1_owner, multi2_active, multi3_owner }, authority_level::owner );
  //redundant keys are allowed
  OK( "multi", { multi1_owner, alice_active, bob_active }, authority_level::owner );
  // NOTE: can't sign with owner of alice/bob/carol
  FAIL( "multi", { multi1_owner, bob_owner }, authority_level::owner );
  FAIL( "multi", { alice_active, bob_owner }, authority_level::owner );
  FAIL( "multi", { alice_owner, bob_owner }, authority_level::owner );

} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_SUITE_END()

BOOST_FIXTURE_TEST_SUITE( database_api_tests_27, database_api_fixture_27 );

BOOST_AUTO_TEST_CASE( verify_account_authority_test )
{ try {
  auto fee = db->get_witness_schedule_object().median_props.account_creation_fee;

#define KEYS( name ) \
  auto name ## _owner = generate_private_key( BOOST_PP_STRINGIZE( name ) "_owner" ).get_public_key(); \
  auto name ## _active = generate_private_key( BOOST_PP_STRINGIZE( name ) "_active" ).get_public_key(); \
  auto name ## _posting = generate_private_key( BOOST_PP_STRINGIZE( name ) "_posting" ).get_public_key(); \
  auto name ## _memo = generate_private_key( BOOST_PP_STRINGIZE( name ) "_memo" ).get_public_key()
#define CREATE_ACCOUNT( name ) \
  do { \
    account_create_operation op; \
    op.creator = HIVE_INIT_MINER_NAME; \
    op.fee = fee; \
    op.new_account_name = BOOST_PP_STRINGIZE( name ); \
    op.owner = authority( 1, name ## _owner, 1 ); \
    op.active = authority( 1, name ## _active, 1 ); \
    op.posting = authority( 1, name ## _posting, 1 ); \
    op.memo_key = name ## _memo; \
    push_transaction( op, init_account_priv_key ); \
  } while( 0 )

  KEYS( single1 );
  KEYS( single2 );
  KEYS( multi1 );
  KEYS( multi2 );
  KEYS( multi3 );
  KEYS( alice );
  KEYS( bob );
  KEYS( carol );

  CREATE_ACCOUNT( alice );
  CREATE_ACCOUNT( bob );
  CREATE_ACCOUNT( carol );
  {
    account_create_operation op;
    op.creator = HIVE_INIT_MINER_NAME;
    op.fee = fee;
    op.new_account_name = "single";
    op.owner = authority( 1, single1_owner, 1, single2_owner, 1, "alice", 1, "bob", 1 );
    op.active = authority( 1, single1_active, 1, single2_active, 1, "alice", 1, "bob", 1 );
    op.posting = authority( 1, single1_posting, 1, single2_posting, 1, "alice", 1, "bob", 1 );
    op.memo_key = single1_memo;
    push_transaction( op, init_account_priv_key );
  }
  {
    account_create_operation op;
    op.creator = HIVE_INIT_MINER_NAME;
    op.fee = fee;
    op.new_account_name = "open";
    op.owner = authority();
    op.active = authority();
    op.posting = authority();
    op.memo_key = multi3_memo; // has to have some key
    push_transaction( op, init_account_priv_key );
  }
  {
    account_create_operation op;
    op.creator = HIVE_INIT_MINER_NAME;
    op.fee = fee;
    op.new_account_name = "multi";
    op.owner = authority( 3, multi1_owner, 1, multi2_owner, 1, multi3_owner, 1,
      "alice", 2, "bob", 2, "carol", 2 );
    op.active = authority( 3, multi1_active, 1, multi2_active, 1, multi3_active, 1,
      "alice", 2, "bob", 2, "carol", 2 );
    op.posting = authority( 3, multi1_posting, 1, multi2_posting, 1, multi3_posting, 1,
      "alice", 2, "bob", 2, "carol", 2 );
    op.memo_key = multi1_memo;
    push_transaction( op, init_account_priv_key );
  }
  generate_block();

#undef KEYS
#undef CREATE_ACCOUNT

  using namespace hive::plugins::database_api;

  auto OK = [&]( const account_name_type& actor, const flat_set< public_key_type >& sig_keys, authority_level level )
  {
    BOOST_CHECK( database_api->verify_account_authority( { actor, sig_keys, level } ).valid );
  };
  auto FAIL = [&]( const account_name_type& actor, const flat_set< public_key_type >& sig_keys, authority_level  level )
  {
    BOOST_CHECK( not database_api->verify_account_authority( { actor, sig_keys, level } ).valid );
  };

  BOOST_TEST_MESSAGE( "Testing database_api::verify_account_authority on regular account" );

  // alice can sign posting with posting/active/owner
  OK( "alice", { alice_posting }, authority_level::posting );
  OK( "alice", { alice_active }, authority_level::posting );
  OK( "alice", { alice_owner }, authority_level::posting );
  // can't sign with memo, with other account key, with two of valid keys nor with no keys
  FAIL( "alice", { alice_memo }, authority_level::posting );
  FAIL( "alice", { bob_posting }, authority_level::posting );
  FAIL( "alice", { alice_active, alice_posting }, authority_level::posting );
  FAIL( "alice", {}, authority_level::posting );

  // alice can sign active with active/owner
  OK( "alice", { alice_active }, authority_level::active );
  OK( "alice", { alice_owner }, authority_level::active );
  // can't sign with posting/memo, with other account key, with two of valid keys nor with no keys
  FAIL( "alice", { alice_posting }, authority_level::active );
  FAIL( "alice", { alice_memo }, authority_level::active );
  FAIL( "alice", { bob_active }, authority_level::active );
  FAIL( "alice", { alice_active, alice_owner }, authority_level::active );
  FAIL( "alice", {}, authority_level::active );

  // alice can sign owner with owner
  OK( "alice", { alice_owner }, authority_level::owner );
  // can't sign posting/active/memo, with other account key, with two keys nor with no keys
  FAIL( "alice", { alice_posting }, authority_level::owner );
  FAIL( "alice", { alice_active }, authority_level::owner );
  FAIL( "alice", { alice_memo }, authority_level::owner );
  FAIL( "alice", { bob_active }, authority_level::owner );
  FAIL( "alice", { alice_owner, alice_active }, authority_level::owner );
  FAIL( "alice", {}, authority_level::owner );

  BOOST_TEST_MESSAGE( "Testing database_api::verify_account_authority on account with alternative keys" );

  // single can sign posting with posting/active/owner in both versions as well as with posting of alice/bob
  OK( "single", { single1_posting }, authority_level::posting );
  OK( "single", { single2_posting }, authority_level::posting );
  OK( "single", { single1_active }, authority_level::posting );
  OK( "single", { single2_active }, authority_level::posting );
  OK( "single", { single1_owner }, authority_level::posting );
  OK( "single", { single2_owner }, authority_level::posting );
  OK( "single", { alice_posting }, authority_level::posting );
  OK( "single", { bob_posting }, authority_level::posting );
  // can't sign with memo, with unrelated account key, with two of valid keys nor with no keys
  FAIL( "single", { single1_memo }, authority_level::posting );
  FAIL( "single", { carol_posting }, authority_level::posting );
  FAIL( "single", { single1_posting, single2_posting }, authority_level::posting );
  FAIL( "single", { single1_posting, alice_posting }, authority_level::posting );
  FAIL( "single", { alice_posting, bob_posting }, authority_level::posting );
  FAIL( "single", {}, authority_level::posting );
  // NOTE: can't sign with active/owner of alice/bob
  FAIL( "single", { alice_active }, authority_level::posting );
  FAIL( "single", { bob_active }, authority_level::posting );
  FAIL( "single", { alice_owner }, authority_level::posting );
  FAIL( "single", { bob_owner }, authority_level::posting );

  // single can sign active with active/owner in both versions as well as with active of alice/bob
  OK( "single", { single1_active }, authority_level::active );
  OK( "single", { single2_active }, authority_level::active );
  OK( "single", { single1_owner }, authority_level::active );
  OK( "single", { single2_owner }, authority_level::active );
  OK( "single", { alice_active }, authority_level::active );
  OK( "single", { bob_active }, authority_level::active );
  // can't sign with posting/memo, with unrelated account key, with two of valid keys nor with no keys
  FAIL( "single", { single1_posting }, authority_level::active );
  FAIL( "single", { single2_posting }, authority_level::active );
  FAIL( "single", { single1_memo }, authority_level::active );
  FAIL( "single", { carol_active }, authority_level::active );
  FAIL( "single", { single1_active, single2_active }, authority_level::active );
  FAIL( "single", { single1_active, alice_active }, authority_level::active );
  FAIL( "single", { alice_active, bob_active }, authority_level::active );
  FAIL( "single", {}, authority_level::active );
  // NOTE: can't sign with owner of alice/bob (can't sign with posting either but that is normal)
  FAIL( "single", { alice_owner }, authority_level::active );
  FAIL( "single", { bob_owner }, authority_level::active );
  FAIL( "single", { alice_posting }, authority_level::active );
  FAIL( "single", { bob_posting }, authority_level::active );

  // single can sign owner with owner in both versions as well as with active(!) of alice/bob
  OK( "single", { single1_owner }, authority_level::owner );
  OK( "single", { single2_owner }, authority_level::owner );
  OK( "single", { alice_active }, authority_level::owner );
  OK( "single", { bob_active }, authority_level::owner );
  // can't sign with posting/active/memo, with unrelated account key, with two of valid keys nor with no keys
  FAIL( "single", { single1_posting }, authority_level::owner );
  FAIL( "single", { single2_posting }, authority_level::owner );
  FAIL( "single", { single1_active }, authority_level::owner );
  FAIL( "single", { single2_active }, authority_level::owner );
  FAIL( "single", { single1_memo }, authority_level::owner );
  FAIL( "single", { carol_active }, authority_level::owner );
  FAIL( "single", { single1_owner, single2_owner }, authority_level::owner );
  FAIL( "single", { single1_owner, alice_active }, authority_level::owner );
  FAIL( "single", { alice_active, bob_active }, authority_level::owner );
  FAIL( "single", {}, authority_level::owner );
  // NOTE: can't sign with owner of alice/bob (also can't sign with posting, but that's expected)
  FAIL( "single", { alice_owner }, authority_level::owner );
  FAIL( "single", { bob_owner }, authority_level::owner );
  FAIL( "single", { alice_posting }, authority_level::owner );
  FAIL( "single", { bob_posting }, authority_level::owner );

  BOOST_TEST_MESSAGE( "Testing database_api::verify_account_authority on account with open authority" );

  // open can sign posting with no keys
  OK( "open", {}, authority_level::posting );
  // can't sign with any other key
  FAIL( "open", { single1_memo }, authority_level::posting );
  FAIL( "open", { alice_posting }, authority_level::posting );
  FAIL( "open", { bob_active }, authority_level::posting );
  FAIL( "open", { carol_owner }, authority_level::posting );

  // open can sign active with no keys
  OK( "open", {}, authority_level::active );
  // can't sign with any other key
  FAIL( "open", { single1_memo }, authority_level::active );
  FAIL( "open", { alice_posting }, authority_level::active );
  FAIL( "open", { bob_active }, authority_level::active );
  FAIL( "open", { carol_owner }, authority_level::active );

  // open can sign owner with no keys
  OK( "open", {}, authority_level::owner );
  // can't sign with any other key
  FAIL( "open", { single1_memo }, authority_level::owner );
  FAIL( "open", { alice_posting }, authority_level::owner );
  FAIL( "open", { bob_active }, authority_level::owner );
  FAIL( "open", { carol_owner }, authority_level::owner );

  BOOST_TEST_MESSAGE( "Testing database_api::verify_account_authority on account with multisig authority" );

  // multi can sign posting with all 3 posting/active/owner (but not mixed) as well as with
  // two posting of alice/bob/carol or one own posting/active/owner key and one posting of alice/bob/carol
  OK( "multi", { multi1_posting, multi2_posting, multi3_posting }, authority_level::posting );
  OK( "multi", { multi1_active, multi2_active, multi3_active }, authority_level::posting );
  OK( "multi", { multi1_owner, multi2_owner, multi3_owner }, authority_level::posting );
  OK( "multi", { alice_posting, bob_posting }, authority_level::posting );
  OK( "multi", { alice_posting, carol_posting }, authority_level::posting );
  OK( "multi", { bob_posting, carol_posting }, authority_level::posting );
  OK( "multi", { multi1_posting, alice_posting }, authority_level::posting );
  OK( "multi", { multi2_active, bob_posting }, authority_level::posting );
  OK( "multi", { multi3_owner, carol_posting }, authority_level::posting );
  // it is ok to sign with two own posting and external posting, but only because of order of
  // checks (if order was different, then it would reach threshold without one of own keys making
  // its use superfluous)
  OK( "multi", { multi1_posting, multi2_posting, alice_posting }, authority_level::posting );
  // can't sign with memo, with unrelated account key, with too few or too many valid keys
  // can't mix keys with different strength either
  FAIL( "multi", { multi1_memo }, authority_level::posting );
  FAIL( "multi", { single1_posting }, authority_level::posting );
  FAIL( "multi", { multi1_posting, multi2_posting }, authority_level::posting );
  FAIL( "multi", { multi1_posting, alice_posting, bob_posting }, authority_level::posting );
  FAIL( "multi", {}, authority_level::posting );
  FAIL( "multi", { multi1_posting, multi2_active, multi3_owner }, authority_level::posting );
  // NOTE: can't sign with active/owner of alice/bob/carol
  FAIL( "multi", { multi1_posting, bob_active }, authority_level::posting );
  FAIL( "multi", { multi1_active, bob_active }, authority_level::posting );
  FAIL( "multi", { multi1_owner, bob_active }, authority_level::posting );
  FAIL( "multi", { alice_posting, bob_active }, authority_level::posting );
  FAIL( "multi", { alice_active, bob_active }, authority_level::posting );
  FAIL( "multi", { multi1_posting, bob_owner }, authority_level::posting );
  FAIL( "multi", { multi1_active, bob_owner }, authority_level::posting );
  FAIL( "multi", { multi1_owner, bob_owner }, authority_level::posting );
  FAIL( "multi", { alice_posting, bob_owner }, authority_level::posting );
  FAIL( "multi", { alice_owner, bob_owner }, authority_level::posting );

  // multi can sign active with all 3 active/owner (but not mixed) as well as with
  // two active of alice/bob/carol or one own active/owner key and one active of alice/bob/carol
  OK( "multi", { multi1_active, multi2_active, multi3_active }, authority_level::active );
  OK( "multi", { multi1_owner, multi2_owner, multi3_owner }, authority_level::active );
  OK( "multi", { alice_active, bob_active }, authority_level::active );
  OK( "multi", { alice_active, carol_active }, authority_level::active );
  OK( "multi", { bob_active, carol_active }, authority_level::active );
  OK( "multi", { multi1_active, bob_active }, authority_level::active );
  OK( "multi", { multi2_owner, carol_active }, authority_level::active );
  // it is ok to sign with two own active and external active, but only because of order of
  // checks (if order was different, then it would reach threshold without one of own keys making
  // its use superfluous)
  OK( "multi", { multi1_active, multi2_active, alice_active }, authority_level::active );
  // can't sign with posting/memo, with unrelated account key, with too few or too many valid keys
  // can't mix keys with different strength either
  FAIL( "multi", { multi1_memo }, authority_level::active );
  FAIL( "multi", { single1_active }, authority_level::active );
  FAIL( "multi", { multi1_active, multi2_active }, authority_level::active );
  FAIL( "multi", { multi1_active, alice_active, bob_active }, authority_level::active );
  FAIL( "multi", {}, authority_level::active );
  FAIL( "multi", { multi1_posting, multi2_posting, multi3_posting }, authority_level::active );
  FAIL( "multi", { multi1_posting, alice_posting }, authority_level::active );
  FAIL( "multi", { multi1_posting, multi2_active, multi3_owner }, authority_level::active );
  FAIL( "multi", { multi1_active, multi2_owner, multi3_active }, authority_level::active );
  // NOTE: can't sign with owner of alice/bob/carol
  FAIL( "multi", { multi1_active, bob_owner }, authority_level::active );
  FAIL( "multi", { multi1_owner, bob_owner }, authority_level::active );
  FAIL( "multi", { alice_active, bob_owner }, authority_level::active );
  FAIL( "multi", { alice_owner, bob_owner }, authority_level::active );

  // multi can sign owner with all 3 owner as well as with two active(!) of alice/bob/carol or
  // one own owner key and one active of alice/bob/carol
  OK( "multi", { multi1_owner, multi2_owner, multi3_owner }, authority_level::owner );
  OK( "multi", { alice_active, bob_active }, authority_level::owner );
  OK( "multi", { alice_active, carol_active }, authority_level::owner );
  OK( "multi", { bob_active, carol_active }, authority_level::owner );
  OK( "multi", { multi1_owner, bob_active }, authority_level::owner );
  // it is ok to sign with two own owner and external active, but only because of order of
  // checks (if order was different, then it would reach threshold without one of own keys making
  // its use superfluous)
  OK( "multi", { multi1_owner, multi2_owner, alice_active }, authority_level::owner );
  // can't sign with posting/active/memo, with unrelated account key, with too few or too many
  // valid keys; can't mix keys with different strength either
  FAIL( "multi", { multi1_memo }, authority_level::owner );
  FAIL( "multi", { single1_active }, authority_level::owner );
  FAIL( "multi", { multi1_owner, multi2_owner }, authority_level::owner );
  FAIL( "multi", { multi1_owner, alice_active, bob_active }, authority_level::owner );
  FAIL( "multi", {}, authority_level::owner );
  FAIL( "multi", { multi1_posting, multi2_posting, multi3_posting }, authority_level::owner );
  FAIL( "multi", { multi1_active, multi2_active, multi3_active }, authority_level::owner );
  FAIL( "multi", { multi1_owner, alice_posting }, authority_level::owner );
  FAIL( "multi", { multi1_posting, multi2_active, multi3_owner }, authority_level::owner );
  FAIL( "multi", { multi1_owner, multi2_active, multi3_owner }, authority_level::owner );
  // NOTE: can't sign with owner of alice/bob/carol
  FAIL( "multi", { multi1_owner, bob_owner }, authority_level::owner );
  FAIL( "multi", { alice_active, bob_owner }, authority_level::owner );
  FAIL( "multi", { alice_owner, bob_owner }, authority_level::owner );

} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_SUITE_END()
#endif

