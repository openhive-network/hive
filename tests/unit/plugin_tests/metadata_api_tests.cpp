#if defined IS_TEST_NET
#include <boost/test/unit_test.hpp>
#include <hive/plugins/metadata_api/metadata_api_plugin.hpp>
#include <hive/plugins/metadata_api/metadata_api.hpp>
#include <hive/plugins/metadata/metadata_plugin.hpp>

#include "../db_fixture/hived_fixture.hpp"

using namespace hive::chain;
using namespace hive::protocol;

struct metadata_api_fixture : hived_fixture
{
  metadata_api_fixture()
  {
    configuration_data.set_initial_asset_supply( INITIAL_TEST_SUPPLY, HBD_INITIAL_TEST_SUPPLY );

    hive::plugins::metadata::metadata_api_plugin* metadata_api_plugin = nullptr;
    postponed_init(
      {
        config_line_t( { "plugin", { HIVE_METADATA_PLUGIN_NAME } } ),
        config_line_t( { "plugin", { HIVE_METADATA_API_PLUGIN_NAME } } ),
        config_line_t( { "shared-file-size",
          { std::to_string( 1024 * 1024 * shared_file_size_small ) } }
        )
      },
      &metadata_api_plugin
    );

    metadata_api = metadata_api_plugin->api.get();
    BOOST_REQUIRE( metadata_api );

    init_account_pub_key = init_account_priv_key.get_public_key();

    generate_block();
    db->set_hardfork( HIVE_NUM_HARDFORKS );
    generate_block();
    db->_log_hardforks = true;

    validate_database();
  }

  hive::plugins::metadata::metadata_api* metadata_api = nullptr;
};

BOOST_FIXTURE_TEST_SUITE( metadata_api_tests, metadata_api_fixture )

BOOST_AUTO_TEST_CASE( get_account_metadata_test )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: get_account_metadata" );

    // Create an account with json_metadata
    ACTORS( (alice) )

    // Update account with metadata
    account_update2_operation op;
    op.account = "alice";
    op.json_metadata = "{\"profile\":{\"name\":\"Alice\"}}";
    op.posting_json_metadata = "{\"app\":\"test/1.0\"}";
    push_transaction( op, alice_private_key );

    generate_block();

    // Test get_account_metadata
    hive::plugins::metadata::get_account_metadata_args args;
    args.account = "alice";
    auto result = metadata_api->get_account_metadata( args );

    BOOST_CHECK_EQUAL( result.json_metadata, "{\"profile\":{\"name\":\"Alice\"}}" );
    BOOST_CHECK_EQUAL( result.posting_json_metadata, "{\"app\":\"test/1.0\"}" );

    // Test for non-existent account
    args.account = "nonexistent";
    BOOST_CHECK_THROW( metadata_api->get_account_metadata( args ), fc::assert_exception );
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( find_account_metadata_test )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: find_account_metadata" );

    // Create accounts with metadata
    ACTORS( (bob)(carol) )

    // Update bob's metadata
    account_update2_operation op;
    op.account = "bob";
    op.json_metadata = "{\"type\":\"user\"}";
    op.posting_json_metadata = "{\"following\":[]}";
    push_transaction( op, bob_private_key );

    // Update carol's metadata
    op.account = "carol";
    op.json_metadata = "{\"type\":\"business\"}";
    op.posting_json_metadata = "{\"verified\":true}";
    push_transaction( op, carol_private_key );

    generate_block();

    // Test find_account_metadata with multiple accounts
    hive::plugins::metadata::find_account_metadata_args args;
    args.accounts = { "bob", "carol" };
    auto result = metadata_api->find_account_metadata( args );

    BOOST_REQUIRE_EQUAL( result.metadata.size(), 2u );

    // Check bob's metadata
    BOOST_CHECK( result.metadata[0].account == account_name_type( "bob" ) );
    BOOST_CHECK_EQUAL( result.metadata[0].json_metadata, "{\"type\":\"user\"}" );
    BOOST_CHECK_EQUAL( result.metadata[0].posting_json_metadata, "{\"following\":[]}" );

    // Check carol's metadata
    BOOST_CHECK( result.metadata[1].account == account_name_type( "carol" ) );
    BOOST_CHECK_EQUAL( result.metadata[1].json_metadata, "{\"type\":\"business\"}" );
    BOOST_CHECK_EQUAL( result.metadata[1].posting_json_metadata, "{\"verified\":true}" );

    // Test with non-existent account in list (should be skipped)
    args.accounts = { "bob", "nonexistent", "carol" };
    result = metadata_api->find_account_metadata( args );
    BOOST_CHECK_EQUAL( result.metadata.size(), 2u ); // Only existing accounts returned
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( account_create_metadata_test )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: account creation with json_metadata" );

    const auto creation_fee = db->get_witness_schedule_object().median_props.account_creation_fee;

    // Create account with initial json_metadata
    account_create_operation create_op;
    create_op.fee = creation_fee;
    create_op.creator = HIVE_INIT_MINER_NAME;
    create_op.new_account_name = "newuser";
    create_op.owner = authority( 1, generate_private_key( "newuser_owner" ).get_public_key(), 1 );
    create_op.active = authority( 1, generate_private_key( "newuser_active" ).get_public_key(), 1 );
    create_op.posting = authority( 1, generate_private_key( "newuser_posting" ).get_public_key(), 1 );
    create_op.memo_key = generate_private_key( "newuser_memo" ).get_public_key();
    create_op.json_metadata = "{\"created_by\":\"test\"}";

    push_transaction( create_op, init_account_priv_key );
    generate_block();

    // Verify metadata was stored
    hive::plugins::metadata::get_account_metadata_args args;
    args.account = "newuser";
    auto result = metadata_api->get_account_metadata( args );

    BOOST_CHECK_EQUAL( result.json_metadata, "{\"created_by\":\"test\"}" );
    BOOST_CHECK_EQUAL( result.posting_json_metadata, "" ); // posting_json_metadata not set at creation
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_SUITE_END()

#endif
