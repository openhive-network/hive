#include <fc/macros.hpp>

#if defined IS_TEST_NET && defined HIVE_ENABLE_SMT

FC_TODO(Extend testing scenarios to support multiple NAIs per account)

#include <boost/test/unit_test.hpp>

#include <hive/chain/hive_fwd.hpp>

#include <hive/protocol/exceptions.hpp>
#include <hive/protocol/hardfork.hpp>

#include <hive/chain/database.hpp>
#include <hive/chain/database_exceptions.hpp>
#include <hive/chain/hive_objects.hpp>
#include <hive/chain/smt_objects.hpp>

#include <hive/chain/util/smt_token.hpp>

#include "../db_fixture/clean_database_fixture.hpp"

using namespace hive::chain;
using namespace hive::protocol;
using fc::string;
using boost::container::flat_set;
using boost::container::flat_map;

BOOST_FIXTURE_TEST_SUITE( smt_tests, smt_database_fixture )

BOOST_AUTO_TEST_CASE( smt_transfer_validate )
{
  try
  {
    ACTORS( (alice) )

    generate_block();

    asset_symbol_type alice_symbol = create_smt("alice", alice_private_key, 0);

    transfer_operation op;
    op.from = "alice";
    op.to = "bob";
    op.amount = asset(100, alice_symbol);
    op.validate();

    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( smt_transfer_apply )
{
  // This simple test touches SMT account balance objects, related functions (get/adjust)
  // and transfer operation that builds on them.
  try
  {
    ACTORS( (alice)(bob) )

    generate_block();

    // Create SMT.
    asset_symbol_type alice_symbol = create_smt("alice", alice_private_key, 0);
    asset_symbol_type bob_symbol = create_smt("bob", bob_private_key, 1);

    // Give some SMT to creators.
    FUND( "alice", asset( 100, alice_symbol ) );
    FUND( "bob", asset( 110, bob_symbol ) );

    // Check pre-tranfer amounts.
    FC_ASSERT( db->get_balance( "alice", alice_symbol ).amount == 100, "SMT balance adjusting error" );
    FC_ASSERT( db->get_balance( "alice", bob_symbol ).amount == 0, "SMT balance adjusting error" );
    FC_ASSERT( db->get_balance( "bob", alice_symbol ).amount == 0, "SMT balance adjusting error" );
    FC_ASSERT( db->get_balance( "bob", bob_symbol ).amount == 110, "SMT balance adjusting error" );

    // Transfer SMT.
    transfer( "alice", "bob", asset(20, alice_symbol) );
    transfer( "bob", "alice", asset(50, bob_symbol) );

    // Check transfer outcome.
    FC_ASSERT( db->get_balance( "alice", alice_symbol ).amount == 80, "SMT transfer error" );
    FC_ASSERT( db->get_balance( "alice", bob_symbol ).amount == 50, "SMT transfer error" );
    FC_ASSERT( db->get_balance( "bob", alice_symbol ).amount == 20, "SMT transfer error" );
    FC_ASSERT( db->get_balance( "bob", bob_symbol ).amount == 60, "SMT transfer error" );

    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( comment_votable_assers_validate )
{
  try
  {
    BOOST_TEST_MESSAGE( "Test Comment Votable Assets Validate" );
    ACTORS((alice));

    generate_block();

    std::array<asset_symbol_type, SMT_MAX_VOTABLE_ASSETS + 1> smts;
    /// Create one more than limit to test negative cases
    for(size_t i = 0; i < SMT_MAX_VOTABLE_ASSETS + 1; ++i)
    {
      asset_symbol_type smt = create_smt("alice", alice_private_key, 0);
      smts[i] = std::move(smt);
    }

    {
      comment_options_operation op;

      op.author = "alice";
      op.permlink = "test";

      BOOST_TEST_MESSAGE( "--- Testing valid configuration: no votable_assets" );
      allowed_vote_assets ava;
      op.extensions.insert( ava );
      op.validate();
    }

    {
      comment_options_operation op;

      op.author = "alice";
      op.permlink = "test";

      BOOST_TEST_MESSAGE( "--- Testing valid configuration of votable_assets" );
      allowed_vote_assets ava;
      for(size_t i = 0; i < SMT_MAX_VOTABLE_ASSETS; ++i)
      {
        const auto& smt = smts[i];
        ava.add_votable_asset(smt, share_type(10 + i), (i & 2) != 0);
      }

      op.extensions.insert( ava );
      op.validate();
    }

    {
      comment_options_operation op;

      op.author = "alice";
      op.permlink = "test";

      BOOST_TEST_MESSAGE( "--- Testing invalid configuration of votable_assets - too much assets specified" );
      allowed_vote_assets ava;
      for(size_t i = 0; i < smts.size(); ++i)
      {
        const auto& smt = smts[i];
        ava.add_votable_asset(smt, share_type(20 + i), (i & 2) != 0);
      }

      op.extensions.insert( ava );
      HIVE_REQUIRE_THROW( op.validate(), fc::assert_exception );
    }

    {
      comment_options_operation op;

      op.author = "alice";
      op.permlink = "test";

      BOOST_TEST_MESSAGE( "--- Testing invalid configuration of votable_assets - HIVE added to container" );
      allowed_vote_assets ava;
      const auto& smt = smts.front();
      ava.add_votable_asset(smt, share_type(20), false);
      ava.add_votable_asset(HIVE_SYMBOL, share_type(20), true);
      op.extensions.insert( ava );
      HIVE_REQUIRE_THROW( op.validate(), fc::assert_exception );
    }
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( asset_symbol_vesting_methods )
{
  try
  {
    BOOST_TEST_MESSAGE( "Test asset_symbol vesting methods" );

    asset_symbol_type Hive = HIVE_SYMBOL;
    FC_ASSERT( Hive.is_vesting() == false );
    FC_ASSERT( Hive.get_paired_symbol() == VESTS_SYMBOL );

    asset_symbol_type Vests = VESTS_SYMBOL;
    FC_ASSERT( Vests.is_vesting() );
    FC_ASSERT( Vests.get_paired_symbol() == HIVE_SYMBOL );

    asset_symbol_type Hbd = HBD_SYMBOL;
    FC_ASSERT( Hbd.is_vesting() == false );
    FC_ASSERT( Hbd.get_paired_symbol() == HBD_SYMBOL );

    ACTORS( (alice) )
    generate_block();
    auto smts = create_smt_3("alice", alice_private_key);
    {
      for( const asset_symbol_type& liquid_smt : smts )
      {
        FC_ASSERT( liquid_smt.is_vesting() == false );
        auto vesting_smt = liquid_smt.get_paired_symbol();
        FC_ASSERT( vesting_smt != liquid_smt );
        FC_ASSERT( vesting_smt.is_vesting() );
        FC_ASSERT( vesting_smt.get_paired_symbol() == liquid_smt );
      }
    }
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( vesting_smt_creation )
{
  try
  {
    BOOST_TEST_MESSAGE( "Test Creation of vesting SMT" );

    ACTORS((alice));
    generate_block();

    asset_symbol_type liquid_symbol = create_smt("alice", alice_private_key, 6);
    // Use liquid symbol/NAI to confirm smt object was created.
    auto liquid_object_by_symbol = util::smt::find_token( *db, liquid_symbol );
    FC_ASSERT( liquid_object_by_symbol != nullptr );

    asset_symbol_type vesting_symbol = liquid_symbol.get_paired_symbol();
    // Use vesting symbol/NAI to confirm smt object was created.
    auto vesting_object_by_symbol = util::smt::find_token( *db, vesting_symbol );
    FC_ASSERT( vesting_object_by_symbol != nullptr );

    // Check that liquid and vesting objects are the same one.
    FC_ASSERT( liquid_object_by_symbol == vesting_object_by_symbol );
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( setup_validate )
{
  try
  {
    smt_setup_operation op;

    ACTORS( (alice) )
    generate_block();
    asset_symbol_type alice_symbol = create_smt("alice", alice_private_key, 4);

    op.control_account = "";
    HIVE_REQUIRE_THROW( op.validate(), fc::exception );

    //Invalid account
    op.control_account = "&&&&&&";
    HIVE_REQUIRE_THROW( op.validate(), fc::exception );

    //FC_ASSERT( max_supply > 0 )
    op.control_account = "abcd";
    op.max_supply = -1;
    HIVE_REQUIRE_THROW( op.validate(), fc::exception );

    op.symbol = alice_symbol;

    //FC_ASSERT( max_supply > 0 )
    op.max_supply = 0;
    HIVE_REQUIRE_THROW( op.validate(), fc::exception );

    //FC_ASSERT( max_supply <= HIVE_MAX_SHARE_SUPPLY )
    op.max_supply = HIVE_MAX_SHARE_SUPPLY + 1;
    HIVE_REQUIRE_THROW( op.validate(), fc::exception );

    //FC_ASSERT( generation_begin_time > HIVE_GENESIS_TIME )
    op.max_supply = HIVE_MAX_SHARE_SUPPLY / 1000;
    op.contribution_begin_time = HIVE_GENESIS_TIME;
    HIVE_REQUIRE_THROW( op.validate(), fc::exception );

    fc::time_point_sec start_time = fc::variant( "2018-03-07T00:00:00" ).as< fc::time_point_sec >();
    fc::time_point_sec t50 = start_time + fc::seconds( 50 );
    fc::time_point_sec t100 = start_time + fc::seconds( 100 );
    fc::time_point_sec t200 = start_time + fc::seconds( 200 );
    fc::time_point_sec t300 = start_time + fc::seconds( 300 );

    op.contribution_begin_time = t100;
    op.contribution_end_time = t50;
    HIVE_REQUIRE_THROW( op.validate(), fc::exception );

    op.contribution_begin_time = t100;
    HIVE_REQUIRE_THROW( op.validate(), fc::exception );

    op.launch_time = t200;
    op.contribution_end_time = t300;
    HIVE_REQUIRE_THROW( op.validate(), fc::exception );

    op.contribution_begin_time = t50;
    op.contribution_end_time = t100;
    op.launch_time = t300;
    HIVE_REQUIRE_THROW( op.validate(), fc::exception );

    op.launch_time = t200;
    smt_capped_generation_policy gp = get_capped_generation_policy
    (
      get_generation_unit( { { "xyz", 1 } }, { { "xyz2", 2 } } )/*pre_soft_cap_unit*/,
      get_generation_unit()/*post_soft_cap_unit*/,
      HIVE_100_PERCENT/*soft_cap_percent*/,
      1/*min_unit_ratio*/,
      2/*max_unit_ratio*/
    );
    op.initial_generation_policy = gp;
    HIVE_REQUIRE_THROW( op.validate(), fc::exception );

    units to_many_units;
    for( uint32_t i = 0; i < SMT_MAX_UNIT_ROUTES + 1; ++i )
      to_many_units.emplace( "alice" + std::to_string( i ), 1 );

    //FC_ASSERT( hive_unit.size() <= SMT_MAX_UNIT_ROUTES )
    gp.pre_soft_cap_unit.hive_unit = to_many_units;
    gp.pre_soft_cap_unit.token_unit = { { "bob",3 } };
    op.initial_generation_policy = gp;
    HIVE_REQUIRE_THROW( op.validate(), fc::exception );

    gp.pre_soft_cap_unit.hive_unit = { { "bob2", 33 } };
    gp.pre_soft_cap_unit.token_unit = to_many_units;
    op.initial_generation_policy = gp;
    HIVE_REQUIRE_THROW( op.validate(), fc::exception );

    //Invalid account
    gp.pre_soft_cap_unit.hive_unit = { { "{}{}", 12 } };
    gp.pre_soft_cap_unit.token_unit = { { "xyz", 13 } };
    op.initial_generation_policy = gp;
    HIVE_REQUIRE_THROW( op.validate(), fc::exception );

    gp.pre_soft_cap_unit.hive_unit = { { "xyz2", 14 } };
    gp.pre_soft_cap_unit.token_unit = { { "{}", 15 } };
    op.initial_generation_policy = gp;
    HIVE_REQUIRE_THROW( op.validate(), fc::exception );

    //Invalid account -> valid is '$from'
    gp.pre_soft_cap_unit.hive_unit = { { "$fromx", 1 } };
    gp.pre_soft_cap_unit.token_unit = { { "$from", 2 } };
    op.initial_generation_policy = gp;
    HIVE_REQUIRE_THROW( op.validate(), fc::exception );

    gp.pre_soft_cap_unit.hive_unit = { { "$from", 3 } };
    gp.pre_soft_cap_unit.token_unit = { { "$from_", 4 } };
    op.initial_generation_policy = gp;
    HIVE_REQUIRE_THROW( op.validate(), fc::exception );

    //Invalid account -> valid is '$from.vesting'
    gp.pre_soft_cap_unit.hive_unit = { { "$from.vestingx", 2 } };
    gp.pre_soft_cap_unit.token_unit = { { "$from.vesting", 222 } };
    op.initial_generation_policy = gp;
    HIVE_REQUIRE_THROW( op.validate(), fc::exception );

    gp.pre_soft_cap_unit.hive_unit = { { "$from.vesting", 13 } };
    // test failure on conversion from string to account_name_type (too long)
    HIVE_REQUIRE_THROW( ( gp.pre_soft_cap_unit.token_unit = { { "$from.vesting.vesting", 3 } } ), fc::exception );
    gp.pre_soft_cap_unit.token_unit = { { "$from.vesting.ve", 3 } }; // test failure of name validation (too short name element 've')
    op.initial_generation_policy = gp;
    HIVE_REQUIRE_THROW( op.validate(), fc::exception );

    //FC_ASSERT( hive_unit.value > 0 );
    gp.pre_soft_cap_unit.hive_unit = { { "$from.vesting", 0 } };
    gp.pre_soft_cap_unit.token_unit = { { "$from.vesting", 2 } };
    op.initial_generation_policy = gp;
    HIVE_REQUIRE_THROW( op.validate(), fc::exception );

    gp.pre_soft_cap_unit.hive_unit = { { "$from.vesting", 10 } };
    gp.pre_soft_cap_unit.token_unit = { { "$from.vesting", 0 } };
    op.initial_generation_policy = gp;
    HIVE_REQUIRE_THROW( op.validate(), fc::exception );

    //FC_ASSERT( hive_unit.value > 0 );
    gp.pre_soft_cap_unit.hive_unit = { { "$from", 0 } };
    gp.pre_soft_cap_unit.token_unit = { { "$from", 100 } };
    op.initial_generation_policy = gp;
    HIVE_REQUIRE_THROW( op.validate(), fc::exception );

    gp.pre_soft_cap_unit.hive_unit = { { "$from", 33 } };
    gp.pre_soft_cap_unit.token_unit = { { "$from", 0 } };
    op.initial_generation_policy = gp;
    HIVE_REQUIRE_THROW( op.validate(), fc::exception );

    //FC_ASSERT( hive_unit.value > 0 );
    gp.pre_soft_cap_unit.hive_unit = { { "qprst", 0 } };
    gp.pre_soft_cap_unit.token_unit = { { "qprst", 67 } };
    op.initial_generation_policy = gp;
    HIVE_REQUIRE_THROW( op.validate(), fc::exception );

    gp.pre_soft_cap_unit.hive_unit = { { "my_account2", 55 } };
    gp.pre_soft_cap_unit.token_unit = { { "my_account", 0 } };
    op.initial_generation_policy = gp;
    HIVE_REQUIRE_THROW( op.validate(), fc::exception );

    gp.pre_soft_cap_unit.hive_unit = { { "bob", 2 }, { "$from.vesting", 3 }, { "$from", 4 } };
    gp.pre_soft_cap_unit.token_unit = { { "alice", 5 }, { "$from", 3 } };
    op.initial_generation_policy = gp;
    HIVE_REQUIRE_THROW( op.validate(), fc::exception );

    gp.soft_cap_percent = 0;
    op.initial_generation_policy = gp;
    HIVE_REQUIRE_THROW( op.validate(), fc::exception );

    gp.soft_cap_percent = HIVE_100_PERCENT + 1;
    op.initial_generation_policy = gp;
    HIVE_REQUIRE_THROW( op.validate(), fc::exception );

    gp.soft_cap_percent = HIVE_100_PERCENT;
    gp.post_soft_cap_unit.hive_unit = { { "bob", 2 } };
    gp.post_soft_cap_unit.token_unit = {};
    op.initial_generation_policy = gp;
    HIVE_REQUIRE_THROW( op.validate(), fc::exception );

    gp.soft_cap_percent = HIVE_100_PERCENT;
    gp.post_soft_cap_unit.hive_unit = {};
    gp.post_soft_cap_unit.token_unit = { { "alice", 3 } };
    op.initial_generation_policy = gp;
    HIVE_REQUIRE_THROW( op.validate(), fc::exception );

    gp.soft_cap_percent = HIVE_100_PERCENT / 2;
    gp.post_soft_cap_unit.hive_unit = {};
    gp.post_soft_cap_unit.token_unit = {};
    op.initial_generation_policy = gp;
    HIVE_REQUIRE_THROW( op.validate(), fc::exception );

    gp.soft_cap_percent = HIVE_100_PERCENT;
    gp.post_soft_cap_unit.hive_unit = {};
    gp.post_soft_cap_unit.token_unit = {};
    op.initial_generation_policy = gp;
    HIVE_REQUIRE_THROW( op.validate(), fc::exception );

    op.hive_units_soft_cap = SMT_MIN_SOFT_CAP_HIVE_UNITS;
    op.hive_units_hard_cap = SMT_MIN_HARD_CAP_HIVE_UNITS;
    op.validate();

    gp.max_unit_ratio = ( ( 11 * SMT_MIN_HARD_CAP_HIVE_UNITS ) / SMT_MIN_SATURATION_HIVE_UNITS ) * 2;
    op.initial_generation_policy = gp;
    op.validate();

    gp.max_unit_ratio = 2;
    op.initial_generation_policy = gp;
    op.validate();

    smt_capped_generation_policy gp_valid = gp;

    gp.soft_cap_percent = 1;
    gp.post_soft_cap_unit.hive_unit = { { "bob", 2 } };
    op.initial_generation_policy = gp;
    op.validate();

    gp = gp_valid;
    op.initial_generation_policy = gp;
    op.validate();

    uint16_t max_val_16 = std::numeric_limits<uint16_t>::max();
    uint32_t max_val_32 = std::numeric_limits<uint32_t>::max();

    gp.soft_cap_percent = HIVE_100_PERCENT - 1;
    gp.min_unit_ratio = max_val_32;
    gp.post_soft_cap_unit.hive_unit = { { "abc", 1 } };
    gp.post_soft_cap_unit.token_unit = { { "abc1", max_val_16 } };
    gp.pre_soft_cap_unit.token_unit = { { "abc2", max_val_16 } };
    op.initial_generation_policy = gp;
    op.validate();

    gp.min_unit_ratio = 1;
    gp.post_soft_cap_unit.token_unit = { { "abc1", 1 } };
    gp.pre_soft_cap_unit.token_unit = { { "abc2", 1 } };
    gp.post_soft_cap_unit.hive_unit = { { "abc3", max_val_16 } };
    gp.pre_soft_cap_unit.hive_unit = { { "abc34", max_val_16 } };
    op.initial_generation_policy = gp;
    op.validate();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( setup_authorities )
{
  try
  {
    smt_setup_operation op;
    op.control_account = "alice";

    flat_set< account_name_type > auths;
    flat_set< account_name_type > expected;

    op.get_required_owner_authorities( auths );
    BOOST_REQUIRE( auths == expected );

    op.get_required_posting_authorities( auths );
    BOOST_REQUIRE( auths == expected );

    expected.insert( "alice" );
    op.get_required_active_authorities( auths );
    BOOST_REQUIRE( auths == expected );
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( setup_apply )
{
  try
  {
    ACTORS( (alice)(bob) )

    generate_block();

    FUND( "alice", 10 * 1000 * 1000 );
    FUND( "bob", 10 * 1000 * 1000 );

    set_price_feed( price( ASSET( "1.000 TBD" ), ASSET( "1.000 TESTS" ) ) );

    smt_setup_operation op;
    op.control_account = "alice";
    op.hive_units_soft_cap = SMT_MIN_SOFT_CAP_HIVE_UNITS;
    op.hive_units_hard_cap = SMT_MIN_HARD_CAP_HIVE_UNITS;

    smt_capped_generation_policy gp = get_capped_generation_policy
    (
      get_generation_unit( { { "xyz", 1 } }, { { "xyz2", 2 } } )/*pre_soft_cap_unit*/,
      get_generation_unit()/*post_soft_cap_unit*/,
      HIVE_100_PERCENT/*soft_cap_percent*/,
      1/*min_unit_ratio*/,
      2/*max_unit_ratio*/
    );

    fc::time_point_sec start_time        = fc::variant( "2021-01-01T00:00:00" ).as< fc::time_point_sec >();
    fc::time_point_sec start_time_plus_1 = start_time + fc::seconds(1);

    op.initial_generation_policy = gp;
    op.contribution_begin_time = start_time;
    op.contribution_end_time = op.launch_time = start_time_plus_1;

    signed_transaction tx;

    //SMT doesn't exist
    tx.operations.push_back( op );
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    HIVE_REQUIRE_THROW( push_transaction( tx, alice_private_key ), fc::exception );
    tx.operations.clear();

    //Try to elevate account
    asset_symbol_type alice_symbol = create_smt( "alice", alice_private_key, 3 );
    tx.operations.clear();

    //Make transaction again. Everything is correct.
    op.symbol = alice_symbol;
    tx.operations.push_back( op );
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    push_transaction( tx, alice_private_key );
    tx.operations.clear();
  }
  FC_LOG_AND_RETHROW()
}

/*
  * SMT legacy tests
  *
  * The logic tests in legacy tests *should* be entirely duplicated in smt_operation_tests.cpp
  * We are keeping these tests around to provide an additional layer re-assurance for the time being
  */
FC_TODO( "Remove SMT legacy tests and ensure code coverage is not reduced" );

BOOST_AUTO_TEST_CASE( smt_create_apply )
{
  try
  {
    ACTORS( (alice)(bob) )

    generate_block();

    set_price_feed( price( ASSET( "1.000 TBD" ), ASSET( "1.000 TESTS" ) ) );

    const dynamic_global_property_object& dgpo = db->get_dynamic_global_properties();
    asset required_creation_fee = dgpo.smt_creation_fee;
    unsigned int test_amount = required_creation_fee.amount.value;

    smt_create_operation op;
    op.control_account = "alice";
    op.symbol = get_new_smt_symbol( 3, db );
    op.precision = op.symbol.decimals();

    BOOST_TEST_MESSAGE( " -- SMT create with insufficient HBD balance" );
    // Fund with HIVE, and set fee with HBD.
    FUND( "alice", test_amount );
    // Declare fee in HBD/TBD though alice has none.
    op.smt_creation_fee = asset( test_amount, HBD_SYMBOL );
    // Throw due to insufficient balance of HBD/TBD.
    FAIL_WITH_OP(op, alice_private_key, fc::assert_exception);

    BOOST_TEST_MESSAGE( " -- SMT create with insufficient HIVE balance" );
    // Now fund with HBD, and set fee with HIVE.
    convert( "alice", asset( test_amount, HIVE_SYMBOL ) );
    // Declare fee in HIVE though alice has none.
    op.smt_creation_fee = asset( test_amount, HIVE_SYMBOL );
    // Throw due to insufficient balance of HIVE.
    FAIL_WITH_OP(op, alice_private_key, fc::assert_exception);

    BOOST_TEST_MESSAGE( " -- SMT create with available funds" );
    // Push valid operation.
    op.smt_creation_fee = asset( test_amount, HBD_SYMBOL );
    PUSH_OP( op, alice_private_key );

    BOOST_TEST_MESSAGE( " -- SMT cannot be created twice even with different precision" );
    create_conflicting_smt(op.symbol, "alice", alice_private_key);

    BOOST_TEST_MESSAGE( " -- Another user cannot create an SMT twice even with different precision" );
    // Check that another user/account can't be used to create duplicating SMT even with different precision.
    create_conflicting_smt(op.symbol, "bob", bob_private_key);

    BOOST_TEST_MESSAGE( " -- Check that an SMT cannot be created with decimals greater than HIVE_MAX_DECIMALS" );
    // Check that invalid SMT can't be created
    create_invalid_smt("alice", alice_private_key);

    BOOST_TEST_MESSAGE( " -- Check that an SMT cannot be created with a creation fee lower than required" );
    // Check fee set too low.
    asset fee_too_low = required_creation_fee;
    unsigned int too_low_fee_amount = required_creation_fee.amount.value-1;
    fee_too_low.amount -= 1;

    SMT_SYMBOL( bob, 0, db );
    op.control_account = "bob";
    op.symbol = bob_symbol;
    op.precision = op.symbol.decimals();

    BOOST_TEST_MESSAGE( " -- Check that we cannot create an SMT with an insufficent HIVE creation fee" );
    // Check too low fee in HIVE.
    FUND( "bob", too_low_fee_amount );
    op.smt_creation_fee = asset( too_low_fee_amount, HIVE_SYMBOL );
    FAIL_WITH_OP(op, bob_private_key, fc::assert_exception);

    BOOST_TEST_MESSAGE( " -- Check that we cannot create an SMT with an insufficent HBD creation fee" );
    // Check too low fee in HBD.
    convert( "bob", asset( too_low_fee_amount, HIVE_SYMBOL ) );
    op.smt_creation_fee = asset( too_low_fee_amount, HBD_SYMBOL );
    FAIL_WITH_OP(op, bob_private_key, fc::assert_exception);

    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_SUITE_END()
#endif
