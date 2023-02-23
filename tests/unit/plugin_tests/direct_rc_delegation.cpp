#ifdef IS_TEST_NET
#include <boost/test/unit_test.hpp>

#include <hive/chain/account_object.hpp>
#include <hive/chain/comment_object.hpp>
#include <hive/chain/database_exceptions.hpp>
#include <hive/chain/smt_objects.hpp>
#include <hive/protocol/hive_operations.hpp>

#include <hive/plugins/rc/rc_objects.hpp>
#include <hive/plugins/rc/rc_config.hpp>
#include <hive/plugins/rc/rc_operations.hpp>

#include "../db_fixture/database_fixture.hpp"

#include <chrono>

using namespace hive::chain;
using namespace hive::protocol;
using namespace hive::plugins::rc;

BOOST_FIXTURE_TEST_SUITE( direct_rc_delegation, clean_database_fixture )

BOOST_AUTO_TEST_CASE( delegate_rc_operation_validate )
{
  try{
    delegate_rc_operation op;
    op.from = "alice";
    op.delegatees = {"eve", "bob"};
    op.max_rc = 10;
    op.validate();

    // max_rc cannot be negative
    op.max_rc = -1;
    BOOST_REQUIRE_THROW( op.validate(), fc::assert_exception );
    op.max_rc = 10;

    // alice- is an invalid account name
    op.from = "alice-";
    BOOST_REQUIRE_THROW( op.validate(), fc::assert_exception );

    // Bob- is an invalid account name
    op.from = "alice";
    op.delegatees = {"eve", "bob-"};
    BOOST_REQUIRE_THROW( op.validate(), fc::assert_exception );

    // Alice is already the delegator
    op.delegatees = {"alice", "bob"};
    BOOST_REQUIRE_THROW( op.validate(), fc::assert_exception );

    // Empty vector
    op.delegatees = {};
    BOOST_REQUIRE_THROW( op.validate(), fc::assert_exception );

    // bob is defined twice, handled by the flat_set
    op.delegatees = {"eve", "bob", "bob"};
    op.validate();

    // There is more than HIVE_RC_MAX_ACCOUNTS_PER_DELEGATION_OP accounts in delegatees
    flat_set<account_name_type> set_too_big;
    for (int i = 0; i < HIVE_RC_MAX_ACCOUNTS_PER_DELEGATION_OP + 1; i++) {
      set_too_big.insert( "actor" + std::to_string(i) );
    }
    op.delegatees = set_too_big;
    BOOST_REQUIRE_THROW( op.validate(), fc::assert_exception );
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( delegate_rc_operation_apply_single )
{
  try
  {
    db_plugin->debug_update( [=]( database& db )
    {
      db.modify( db.get_witness_schedule_object(), [&]( witness_schedule_object& wso )
      {
        wso.median_props.account_creation_fee = ASSET( "3.000 TESTS" ); //mainnet value
      } );
    } );
    generate_block();

    uint64_t min_delegation = 0;
    {
      const auto& gpo = db->get_dynamic_global_properties();
      const auto& wso = db->get_witness_schedule_object();
      min_delegation = (asset( wso.median_props.account_creation_fee.amount / 3, HIVE_SYMBOL ) * gpo.get_vesting_share_price()).amount.value;
    }

    BOOST_TEST_MESSAGE( "Testing:  delegate_rc_operation_apply_single to a single account" );
    ACTORS( (alice)(bob)(dave) )
    vest( HIVE_INIT_MINER_NAME, "alice", ASSET( "10.000 TESTS" ) );
    int64_t alice_vests = alice.vesting_shares.amount.value;

    // Delegating more rc than I have should fail
    delegate_rc_operation op;
    op.from = "alice";
    op.delegatees = {"bob"};
    op.max_rc = alice_vests + 1;

    custom_json_operation custom_op;
    custom_op.required_posting_auths.insert( "alice" );
    custom_op.id = HIVE_RC_PLUGIN_NAME;
    custom_op.json = fc::json::to_string( rc_plugin_operation( op ) );
    BOOST_CHECK_THROW( push_transaction(custom_op, alice_private_key), fc::exception );

    // Delegating less than the minimum should fail
    op.delegatees = {"bob"};
    op.max_rc = min_delegation - 1;
    custom_op.json = fc::json::to_string( rc_plugin_operation( op ) );
    BOOST_CHECK_THROW( push_transaction(custom_op, alice_private_key), fc::exception );

    // Delegating to a non-existing account should fail
    op.delegatees = {"eve"};
    op.max_rc = min_delegation;
    custom_op.json = fc::json::to_string( rc_plugin_operation( op ) );
    BOOST_CHECK_THROW( push_transaction(custom_op, alice_private_key), fc::exception );

    // Delegating 0 shouldn't work if there isn't already a delegation that exists (since 0 deletes the delegation)
    op.delegatees = {"bob"};
    op.max_rc = 0;
    custom_op.json = fc::json::to_string( rc_plugin_operation( op ) );
    BOOST_CHECK_THROW( push_transaction(custom_op, alice_private_key), fc::exception );

    // Successful delegation
    op.delegatees = {"bob"};
    op.max_rc = min_delegation + 1;
    custom_op.json = fc::json::to_string( rc_plugin_operation( op ) );
    push_transaction(custom_op, alice_private_key);

    generate_block();

    const rc_direct_delegation_object* delegation = db->find< rc_direct_delegation_object, by_from_to >( boost::make_tuple( alice_id, bob_id ) );
    BOOST_REQUIRE( delegation->from == alice_id );
    BOOST_REQUIRE( delegation->to == bob_id );
    BOOST_REQUIRE( delegation->delegated_rc == uint64_t(op.max_rc) );

    const rc_account_object& from_rc_account = db->get< rc_account_object, by_name >( op.from );
    const rc_account_object& to_rc_account = db->get< rc_account_object, by_name >( "bob" );

    BOOST_REQUIRE( from_rc_account.delegated_rc == min_delegation + 1 );
    BOOST_REQUIRE( from_rc_account.received_delegated_rc == 0 );
    BOOST_REQUIRE( to_rc_account.delegated_rc == 0 );
    BOOST_REQUIRE( to_rc_account.received_delegated_rc == min_delegation + 1 );


    // Delegating the same amount shouldn't work
    custom_op.json = fc::json::to_string( rc_plugin_operation( op ) );
    BOOST_CHECK_THROW( push_transaction(custom_op, alice_private_key), fc::exception );

    // Decrease the delegation
    op.from = "alice";
    op.delegatees = {"bob"};
    op.max_rc = min_delegation;
    custom_op.required_posting_auths.clear();
    custom_op.required_posting_auths.insert( "alice" );
    custom_op.json = fc::json::to_string( rc_plugin_operation( op ) );
    push_transaction(custom_op, alice_private_key);

    generate_block();

    const rc_direct_delegation_object* delegation_decreased = db->find< rc_direct_delegation_object, by_from_to >( boost::make_tuple( alice_id, bob_id ) );
    BOOST_REQUIRE( delegation_decreased->from == alice_id );
    BOOST_REQUIRE( delegation_decreased->to == bob_id );
    BOOST_REQUIRE( delegation_decreased->delegated_rc == uint64_t(op.max_rc) );

    const rc_account_object& from_rc_account_decreased = db->get< rc_account_object, by_name >( op.from );
    const rc_account_object& to_rc_account_decreased = db->get< rc_account_object, by_name >( "bob" );

    BOOST_REQUIRE( from_rc_account_decreased.delegated_rc == min_delegation );
    BOOST_REQUIRE( from_rc_account_decreased.received_delegated_rc == 0 );
    BOOST_REQUIRE( to_rc_account_decreased.delegated_rc == 0 );
    BOOST_REQUIRE( to_rc_account_decreased.received_delegated_rc == min_delegation );

    // Increase the delegation
    op.from = "alice";
    op.delegatees = {"bob"};
    op.max_rc = min_delegation + 10;
    custom_op.json = fc::json::to_string( rc_plugin_operation( op ) );
    push_transaction(custom_op, alice_private_key);

    generate_block();

    const rc_direct_delegation_object* delegation_increased = db->find< rc_direct_delegation_object, by_from_to >( boost::make_tuple( alice_id, bob_id ) );
    BOOST_REQUIRE( delegation_increased->from == alice_id );
    BOOST_REQUIRE( delegation_increased->to == bob_id );
    BOOST_REQUIRE( delegation_increased->delegated_rc == uint64_t(op.max_rc) );

    const rc_account_object& from_rc_account_increased = db->get< rc_account_object, by_name >( op.from );
    const rc_account_object& to_rc_account_increased = db->get< rc_account_object, by_name >( "bob" );

    BOOST_REQUIRE( from_rc_account_increased.delegated_rc == min_delegation + 10 );
    BOOST_REQUIRE( from_rc_account_increased.received_delegated_rc == 0 );
    BOOST_REQUIRE( to_rc_account_increased.delegated_rc == 0 );
    BOOST_REQUIRE( to_rc_account_increased.received_delegated_rc == min_delegation + 10 );

    // Delete the delegation
    op.from = "alice";
    op.delegatees = {"bob"};
    op.max_rc = 0;
    custom_op.json = fc::json::to_string( rc_plugin_operation( op ) );
    push_transaction(custom_op, alice_private_key);

    generate_block();

    const rc_direct_delegation_object* delegation_deleted = db->find< rc_direct_delegation_object, by_from_to >( boost::make_tuple( alice_id, bob_id ) );
    BOOST_REQUIRE( delegation_deleted == nullptr );

    const rc_account_object& from_rc_account_deleted = db->get< rc_account_object, by_name >( "alice" );
    const rc_account_object& to_rc_account_deleted = db->get< rc_account_object, by_name >( "bob" );

    BOOST_REQUIRE( from_rc_account_deleted.delegated_rc == 0 );
    BOOST_REQUIRE( from_rc_account_deleted.received_delegated_rc == 0 );
    BOOST_REQUIRE( to_rc_account_deleted.delegated_rc == 0 );
    BOOST_REQUIRE( to_rc_account_deleted.received_delegated_rc == 0 );
    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( delegate_rc_operation_apply_many )
{
  try
  {
    db_plugin->debug_update( [=]( database& db )
    {
      db.modify( db.get_witness_schedule_object(), [&]( witness_schedule_object& wso )
      {
        wso.median_props.account_creation_fee = ASSET( "3.000 TESTS" ); //mainnet value
      } );
    } );
    generate_block();

    uint64_t min_delegation = 0;
    {
      const auto& gpo = db->get_dynamic_global_properties();
      const auto& wso = db->get_witness_schedule_object();
      min_delegation = (asset( wso.median_props.account_creation_fee.amount / 3, HIVE_SYMBOL ) * gpo.get_vesting_share_price()).amount.value;
    }

    BOOST_TEST_MESSAGE( "Testing:  delegate_rc_operation_apply_many to many accounts" );
    ACTORS( (alice)(bob)(dave)(dan) )
    vest( HIVE_INIT_MINER_NAME, "alice", ASSET( "10.000 TESTS" ) );
    int64_t alice_vests = alice.vesting_shares.amount.value;

    // Delegating more rc than alice has should fail
    delegate_rc_operation op;
    op.from = "alice";
    op.delegatees = {"bob", "dave"};
    op.max_rc = alice_vests;

    custom_json_operation custom_op;
    custom_op.required_posting_auths.insert( "alice" );
    custom_op.id = HIVE_RC_PLUGIN_NAME;
    custom_op.json = fc::json::to_string( rc_plugin_operation( op ) );
    BOOST_CHECK_THROW( push_transaction(custom_op, alice_private_key), fc::exception );

    // Delegating to a non-existing account should fail
    op.delegatees = {"bob", "eve"};
    op.max_rc = min_delegation;
    custom_op.json = fc::json::to_string( rc_plugin_operation( op ) );
    BOOST_CHECK_THROW( push_transaction(custom_op, alice_private_key), fc::exception );

    // Delegating 0 shouldn't work if there isn't already a delegation that exists (since 0 deletes the delegation)
    op.delegatees = {"dave", "bob"};
    op.max_rc = 0;
    custom_op.json = fc::json::to_string( rc_plugin_operation( op ) );
    BOOST_CHECK_THROW( push_transaction(custom_op, alice_private_key), fc::exception );

    // Successful delegations
    op.delegatees = {"bob", "dave"};
    op.max_rc = min_delegation * 2;
    custom_op.json = fc::json::to_string( rc_plugin_operation( op ) );
    push_transaction(custom_op, alice_private_key);

    generate_block();

    const rc_direct_delegation_object* delegation = db->find< rc_direct_delegation_object, by_from_to >( boost::make_tuple( alice_id, bob_id ) );
    BOOST_REQUIRE( delegation->from == alice_id );
    BOOST_REQUIRE( delegation->to == bob_id );
    BOOST_REQUIRE( delegation->delegated_rc == uint64_t(op.max_rc) );

    const rc_account_object& from_rc_account = db->get< rc_account_object, by_name >( op.from );
    const rc_account_object& to_rc_account = db->get< rc_account_object, by_name >( "bob" );

    BOOST_REQUIRE( from_rc_account.delegated_rc == min_delegation * 4 );
    BOOST_REQUIRE( from_rc_account.received_delegated_rc == 0 );
    BOOST_REQUIRE( to_rc_account.delegated_rc == 0 );
    BOOST_REQUIRE( to_rc_account.received_delegated_rc == min_delegation * 2 );

    // Delegating the same amount shouldn't work
    custom_op.json = fc::json::to_string( rc_plugin_operation( op ) );
    BOOST_CHECK_THROW( push_transaction(custom_op, alice_private_key), fc::exception );

    // Delegating 0 shouldn't work if there isn't already a delegation that exists (since 0 deletes the delegation)
    // dave/bob got a delegation but not dan so it should fail
    op.delegatees = {"dave", "bob", "dan"};
    op.max_rc = 0;
    custom_op.json = fc::json::to_string( rc_plugin_operation( op ) );
    BOOST_CHECK_THROW( push_transaction(custom_op, alice_private_key), fc::exception );

    // Decrease the delegations
    op.from = "alice";
    op.delegatees = {"bob", "dave"};
    op.max_rc = min_delegation;
    custom_op.required_posting_auths.clear();
    custom_op.required_posting_auths.insert( "alice" );
    custom_op.json = fc::json::to_string( rc_plugin_operation( op ) );
    push_transaction(custom_op, alice_private_key);
    generate_block();

    const rc_direct_delegation_object* delegation_decreased_bob = db->find< rc_direct_delegation_object, by_from_to >( boost::make_tuple( alice_id, bob_id ) );
    BOOST_REQUIRE( delegation_decreased_bob->from == alice_id );
    BOOST_REQUIRE( delegation_decreased_bob->to == bob_id );
    BOOST_REQUIRE( delegation_decreased_bob->delegated_rc == uint64_t(op.max_rc) );

    const rc_direct_delegation_object* delegation_decreased_dave = db->find< rc_direct_delegation_object, by_from_to >( boost::make_tuple( alice_id, dave_id ) );
    BOOST_REQUIRE( delegation_decreased_dave->from == alice_id );
    BOOST_REQUIRE( delegation_decreased_dave->to == dave_id );
    BOOST_REQUIRE( delegation_decreased_dave->delegated_rc == uint64_t(op.max_rc) );

    const rc_account_object& from_rc_account_decreased = db->get< rc_account_object, by_name >( op.from );
    const rc_account_object& bob_rc_account_decreased = db->get< rc_account_object, by_name >( "bob" );
    const rc_account_object& dave_rc_account_decreased = db->get< rc_account_object, by_name >( "dave" );

    BOOST_REQUIRE( from_rc_account_decreased.delegated_rc == min_delegation * 2 );
    BOOST_REQUIRE( from_rc_account_decreased.received_delegated_rc == 0 );
    BOOST_REQUIRE( bob_rc_account_decreased.delegated_rc == 0 );
    BOOST_REQUIRE( bob_rc_account_decreased.received_delegated_rc == min_delegation );
    BOOST_REQUIRE( dave_rc_account_decreased.delegated_rc == 0 );
    BOOST_REQUIRE( dave_rc_account_decreased.received_delegated_rc == min_delegation );

    // Increase and create a delegation
    op.from = "alice";
    op.delegatees = {"bob", "dan"};
    op.max_rc = min_delegation * 2;
    custom_op.json = fc::json::to_string( rc_plugin_operation( op ) );
    push_transaction(custom_op, alice_private_key);

    generate_block();

    const rc_direct_delegation_object* bob_delegation_increased = db->find< rc_direct_delegation_object, by_from_to >( boost::make_tuple( alice_id, bob_id ) );
    const rc_direct_delegation_object* dan_delegation_created = db->find< rc_direct_delegation_object, by_from_to >( boost::make_tuple( alice_id, dan_id ) );

    BOOST_REQUIRE( bob_delegation_increased->from == alice_id );
    BOOST_REQUIRE( bob_delegation_increased->to == bob_id );
    BOOST_REQUIRE( bob_delegation_increased->delegated_rc == uint64_t(op.max_rc) );
    BOOST_REQUIRE( dan_delegation_created->from == alice_id );
    BOOST_REQUIRE( dan_delegation_created->to == dan_id );
    BOOST_REQUIRE( dan_delegation_created->delegated_rc == uint64_t(op.max_rc) );

    const rc_account_object& alice_rc_account_increased = db->get< rc_account_object, by_name >( op.from );
    const rc_account_object& dan_rc_account_created = db->get< rc_account_object, by_name >( "dan" );
    const rc_account_object& bob_rc_account_increased = db->get< rc_account_object, by_name >( "bob" );

    BOOST_REQUIRE( alice_rc_account_increased.delegated_rc == min_delegation * 5 );
    BOOST_REQUIRE( alice_rc_account_increased.received_delegated_rc == 0 );

    BOOST_REQUIRE( dan_rc_account_created.delegated_rc == 0 );
    BOOST_REQUIRE( dan_rc_account_created.received_delegated_rc == min_delegation * 2 );
    BOOST_REQUIRE( bob_rc_account_increased.delegated_rc == 0 );
    BOOST_REQUIRE( bob_rc_account_increased.received_delegated_rc == min_delegation * 2 );

    // Delete the delegations
    op.from = "alice";
    op.delegatees = {"bob", "dan", "dave"};
    op.max_rc = 0;
    custom_op.json = fc::json::to_string( rc_plugin_operation( op ) );
    push_transaction(custom_op, alice_private_key);

    generate_block();

    const rc_direct_delegation_object* delegation_bob_deleted = db->find< rc_direct_delegation_object, by_from_to >( boost::make_tuple( alice_id, bob_id ) );
    const rc_direct_delegation_object* delegation_dan_deleted = db->find< rc_direct_delegation_object, by_from_to >( boost::make_tuple( alice_id, dan_id ) );
    const rc_direct_delegation_object* delegation_dave_deleted = db->find< rc_direct_delegation_object, by_from_to >( boost::make_tuple( alice_id, dan_id ) );
    BOOST_REQUIRE( delegation_bob_deleted == nullptr );
    BOOST_REQUIRE( delegation_dan_deleted == nullptr );
    BOOST_REQUIRE( delegation_dave_deleted == nullptr );

    const rc_account_object& from_rc_account_deleted = db->get< rc_account_object, by_name >( "alice" );
    const rc_account_object& bob_rc_account_deleted = db->get< rc_account_object, by_name >( "bob" );
    const rc_account_object& dave_rc_account_deleted = db->get< rc_account_object, by_name >( "dave" );
    const rc_account_object& dan_rc_account_deleted = db->get< rc_account_object, by_name >( "dan" );

    BOOST_REQUIRE( from_rc_account_deleted.delegated_rc == 0 );
    BOOST_REQUIRE( from_rc_account_deleted.received_delegated_rc == 0 );
    BOOST_REQUIRE( bob_rc_account_deleted.delegated_rc == 0 );
    BOOST_REQUIRE( bob_rc_account_deleted.received_delegated_rc == 0 );
    BOOST_REQUIRE( dave_rc_account_deleted.delegated_rc == 0 );
    BOOST_REQUIRE( dave_rc_account_deleted.received_delegated_rc == 0 );
    BOOST_REQUIRE( dan_rc_account_deleted.delegated_rc == 0 );
    BOOST_REQUIRE( dan_rc_account_deleted.received_delegated_rc == 0 );
    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( delegate_rc_operation_apply_many_different )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing:  delegate_rc_operation_apply_many_different to many accounts" );
    ACTORS( (alice)(bob)(dave)(dan)(carol) )
    vest( HIVE_INIT_MINER_NAME, "alice", ASSET( "10.000 TESTS" ) );
    uint64_t alice_vests = alice.vesting_shares.amount.value;

    std::string json = "[";
    delegate_rc_operation op;
    op.from = "alice";
    op.delegatees = { "bob" };
    op.max_rc = alice_vests;
    json += "\"delegate_rc\",";
    json += fc::json::to_string( op );
    op.delegatees = { "dave" };
    op.max_rc = alice_vests - 1; //just to be different than that for bob
    json += ",\"delegate_rc\",";
    json += fc::json::to_string( op );
    op.delegatees = { "nonexistent" };
    op.max_rc = 0;
    json += ",\"delegate_rc\",";
    json += fc::json::to_string( op );
    json += "]";

    custom_json_operation custom_op;
    custom_op.required_posting_auths.insert( "alice" );
    custom_op.id = HIVE_RC_PLUGIN_NAME;
    custom_op.json = json;
    ilog( "${json}", (json) );

    // The following used to be true and is still true if operation is already in block:
    //   Delegating more rc than alice has should fail, but it does not, because of how interpreter
    //   works - only the first pair in outer array is processed when it is formatted this way
    //   see https://hiveblocks.com/tx/65b4772d915dfda66a254e5105c74b5c7667991a
    //   it also means that even "delegating" to nonexistent account will pass since it is ignored
    // Now interpreter is more strict when node is_in_control()
    HIVE_REQUIRE_ASSERT( push_transaction( custom_op, alice_private_key ), "!db.is_in_control()" );
    // Let's put just the correct part into transaction so we don't have to change rest of the test
    json = "[";
    op.delegatees = { "bob" };
    op.max_rc = alice_vests;
    json += "\"delegate_rc\",";
    json += fc::json::to_string( op );
    json += "]";

    custom_op.json = json;
    ilog( "${json}", (json) );

    push_transaction( custom_op, alice_private_key );

    generate_block();

    const rc_direct_delegation_object* delegation = db->find< rc_direct_delegation_object, by_from_to >( boost::make_tuple( alice_id, bob_id ) );
    BOOST_REQUIRE( delegation->from == alice_id );
    BOOST_REQUIRE( delegation->to == bob_id );
    BOOST_REQUIRE( delegation->delegated_rc == alice_vests );
    delegation = db->find< rc_direct_delegation_object, by_from_to >( boost::make_tuple( alice_id, dave_id ) );
    BOOST_REQUIRE( delegation == nullptr ); // second part of ill-formed json was ignored so there is no delegation

    op.delegatees = { "bob" };
    op.max_rc = 0;
    json = "[\"delegate_rc\"," + fc::json::to_string( op ) + "]";
    custom_op.json = json; // use proper legacy format to remove delegation
    ilog( "${json}", (json) );

    push_transaction( custom_op, alice_private_key );

    generate_blocks( db->head_block_time() + fc::seconds( HIVE_RC_REGEN_TIME ) ); //regenerate mana
    generate_block();

    delegation = db->find< rc_direct_delegation_object, by_from_to >( boost::make_tuple( alice_id, bob_id ) );
    BOOST_REQUIRE( delegation == nullptr ); // delegation should be removed properly

    op.max_rc = alice_vests / 3;
    std::string delegate_rc_type = fc::json::to_string( rc_plugin_operation( op ).which() );
    json = "[" + delegate_rc_type + "," + fc::json::to_string( op ) + "]";
    custom_op.json = json; // use alternative (even smaller) legacy format to add delegation
    ilog( "${json}", (json) );

    push_transaction( custom_op, alice_private_key );

    generate_block();
    delegation = db->find< rc_direct_delegation_object, by_from_to >( boost::make_tuple( alice_id, bob_id ) );
    BOOST_REQUIRE( delegation->from == alice_id );
    BOOST_REQUIRE( delegation->to == bob_id );
    BOOST_REQUIRE( delegation->delegated_rc == alice_vests / 3 );

    // now delegate to three accounts using the most compact form (alternative legacy in array)
    // (actually the extension that is constantly added could be dropped since empty is default)
    json = "[";
    op.delegatees = { "bob" };
    op.max_rc = 0;
    json += "[" + delegate_rc_type + "," + fc::json::to_string(op) + "]";
    op.delegatees = { "dave" };
    op.max_rc = alice_vests / 4;
    json += ",[" + delegate_rc_type + "," + fc::json::to_string( op ) + "]";
    op.delegatees = { "dan", "carol" };
    op.max_rc = alice_vests / 5;
    json += ",[" + delegate_rc_type + "," + fc::json::to_string( op ) + "]";
    json += "]";
    // double array is important because it tells the interpreter to think of it as array
    // and not as tuple; note that each part is in fact separate operation (executed in order),
    // therefore it would not be ok if alice could not afford delegation without undelegating of bob
    // since she only gets back regen power, not the RC itself (operations don't balance out when
    // they are not part of single delegation)
    custom_op.json = json;
    ilog( "${json}", (json) );

    push_transaction( custom_op, alice_private_key );

    generate_block();
    delegation = db->find< rc_direct_delegation_object, by_from_to >( boost::make_tuple( alice_id, bob_id ) );
    BOOST_REQUIRE( delegation == nullptr );
    delegation = db->find< rc_direct_delegation_object, by_from_to >( boost::make_tuple( alice_id, dave_id ) );
    BOOST_REQUIRE( delegation->from == alice_id );
    BOOST_REQUIRE( delegation->to == dave_id );
    BOOST_REQUIRE( delegation->delegated_rc == alice_vests / 4 );
    delegation = db->find< rc_direct_delegation_object, by_from_to >( boost::make_tuple( alice_id, dan_id ) );
    BOOST_REQUIRE( delegation->from == alice_id );
    BOOST_REQUIRE( delegation->to == dan_id );
    BOOST_REQUIRE( delegation->delegated_rc == alice_vests / 5 );
    delegation = db->find< rc_direct_delegation_object, by_from_to >( boost::make_tuple( alice_id, carol_id ) );
    BOOST_REQUIRE( delegation->from == alice_id );
    BOOST_REQUIRE( delegation->to == carol_id );
    BOOST_REQUIRE( delegation->delegated_rc == alice_vests / 5 );

    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( update_outdel_overflow )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing:  update_outdel_overflow" );
    generate_block();
    db_plugin->debug_update( [=]( database& db )
    {
      db.modify( db.get_witness_schedule_object(), [&]( witness_schedule_object& wso )
      {
        wso.median_props.account_creation_fee = ASSET( "0.001 TESTS" ); //it effectively turns off minimum HP delegation limit
      });
    });
    generate_block();

    ACTORS_DEFAULT_FEE( (alice)(bob)(dave)(eve)(martin) )
    generate_block();
    vest( HIVE_INIT_MINER_NAME, "alice", ASSET( "0.010 TESTS" ) );
    generate_block();

    const rc_account_object& alice_rc_initial = db->get< rc_account_object, by_name >( "alice" );
    const account_object& alice_account_initial = db->get_account( "alice" );
    int64_t vesting_amount = alice_account_initial.get_vesting().amount.value;
    int64_t creation_rc = alice_rc_initial.max_rc_creation_adjustment.amount.value;

    // Delegate 10 rc to bob, the rest to dave, alice has max_rc_creation_adjustment remaining rc
    delegate_rc_operation op;
    op.from = "alice";
    op.delegatees = {"bob"};
    op.max_rc = 10;
    custom_json_operation custom_op;
    custom_op.required_posting_auths.insert( "alice" );
    custom_op.id = HIVE_RC_PLUGIN_NAME;
    custom_op.json = fc::json::to_string( rc_plugin_operation( op ) );
    push_transaction(custom_op, alice_private_key);
    op.delegatees = {"dave"};
    op.max_rc = vesting_amount - 10 + 1;
    custom_op.json = fc::json::to_string( rc_plugin_operation( op ) );
    //check that it is not possible to dip into creation_rc, nor will it be accepted at the cost of dropping bob
    HIVE_REQUIRE_ASSERT( push_transaction( custom_op, alice_private_key ), "from_delegable_rc >= delta_total" );

    op.max_rc = vesting_amount - 10;
    custom_op.json = fc::json::to_string( rc_plugin_operation( op ) );
    push_transaction(custom_op, alice_private_key);
    generate_block();

    const rc_account_object& bob_rc_account_before = db->get< rc_account_object, by_name >("bob");
    const rc_account_object& dave_rc_account_before = db->get< rc_account_object, by_name >("dave");
    const rc_account_object& alice_rc_before = db->get< rc_account_object, by_name >( "alice" );

    BOOST_REQUIRE( alice_rc_before.delegated_rc == uint64_t(vesting_amount) );
    BOOST_REQUIRE( alice_rc_before.received_delegated_rc == 0 );
    BOOST_REQUIRE( alice_rc_before.last_max_rc == creation_rc );
    BOOST_REQUIRE( alice_rc_before.rc_manabar.current_mana == creation_rc - ( 28756 + 28757 ) ); // cost of the two delegate rc ops (the one to dave costs more because more data is in the op)

    BOOST_REQUIRE( bob_rc_account_before.rc_manabar.current_mana == creation_rc + 10 );
    BOOST_REQUIRE( bob_rc_account_before.last_max_rc == creation_rc + 10 );
    BOOST_REQUIRE( bob_rc_account_before.received_delegated_rc == 10 );
    BOOST_REQUIRE( dave_rc_account_before.rc_manabar.current_mana == creation_rc + vesting_amount - 10 );
    BOOST_REQUIRE( dave_rc_account_before.last_max_rc == creation_rc + vesting_amount - 10 );
    BOOST_REQUIRE( dave_rc_account_before.received_delegated_rc == uint64_t(vesting_amount - 10) );

    generate_blocks(10); //couple blocks to let rc mana regenerate

    // we delegate and it affects one delegation
    // Delegate 5 vests out, alice has 5 remaining rc, it's lower than the max_rc_creation_adjustment which is 10
    // So the first delegation (to bob) is lowered to 5
    delegate_vesting_shares_operation dvso;
    dvso.vesting_shares = ASSET( "0.000005 VESTS");
    dvso.delegator = "alice";
    dvso.delegatee = {"eve"};
    push_transaction(dvso, alice_private_key);

    const rc_account_object& bob_rc_account_after = db->get< rc_account_object, by_name >("bob");
    const rc_account_object& dave_rc_account_after = db->get< rc_account_object, by_name >("dave");
    const rc_account_object& alice_rc_after = db->get< rc_account_object, by_name >( "alice" );

    BOOST_REQUIRE( alice_rc_after.delegated_rc == uint64_t(vesting_amount) - 5 );
    BOOST_REQUIRE( alice_rc_after.received_delegated_rc == 0 );
    BOOST_REQUIRE( alice_rc_after.last_max_rc == creation_rc );
    BOOST_REQUIRE( alice_rc_after.rc_manabar.current_mana == creation_rc - 28943 );

    BOOST_REQUIRE( bob_rc_account_after.rc_manabar.current_mana == creation_rc + 5 );
    BOOST_REQUIRE( bob_rc_account_after.last_max_rc == creation_rc + 5 );
    BOOST_REQUIRE( bob_rc_account_after.received_delegated_rc == 5 );

    BOOST_REQUIRE( dave_rc_account_after.rc_manabar.current_mana == creation_rc + vesting_amount - 10 );
    BOOST_REQUIRE( dave_rc_account_after.last_max_rc == creation_rc + vesting_amount - 10 );
    BOOST_REQUIRE( dave_rc_account_after.received_delegated_rc == uint64_t(vesting_amount - 10) );

    // We delegate and we don't have enough rc to sustain bob's delegation
    dvso.vesting_shares = ASSET( "0.000006 VESTS");
    dvso.delegator = "alice";
    dvso.delegatee = "martin";
    push_transaction(dvso, alice_private_key);

    const rc_account_object& bob_rc_account_after_two = db->get< rc_account_object, by_name >("bob");
    const rc_account_object& dave_rc_account_after_two = db->get< rc_account_object, by_name >("dave");
    const rc_account_object& alice_rc_after_two = db->get< rc_account_object, by_name >( "alice" );

    const rc_direct_delegation_object* delegation_deleted = db->find< rc_direct_delegation_object, by_from_to >( boost::make_tuple( alice_id, bob_id ) );
    BOOST_REQUIRE( delegation_deleted == nullptr );

    BOOST_REQUIRE( alice_rc_after_two.delegated_rc == uint64_t(vesting_amount) - 11 );
    BOOST_REQUIRE( alice_rc_after_two.received_delegated_rc == 0 );
    BOOST_REQUIRE( alice_rc_after_two.last_max_rc == creation_rc );
    BOOST_REQUIRE( alice_rc_after_two.rc_manabar.current_mana == creation_rc - ( 28943 + 28944 ) );

    BOOST_REQUIRE( bob_rc_account_after_two.rc_manabar.current_mana == creation_rc );
    BOOST_REQUIRE( bob_rc_account_after_two.last_max_rc == creation_rc );
    BOOST_REQUIRE( bob_rc_account_after_two.received_delegated_rc == 0 );

    BOOST_REQUIRE( dave_rc_account_after_two.rc_manabar.current_mana == creation_rc + vesting_amount - 11 );
    BOOST_REQUIRE( dave_rc_account_after_two.last_max_rc == creation_rc + vesting_amount - 11 );
    BOOST_REQUIRE( dave_rc_account_after_two.received_delegated_rc == uint64_t(vesting_amount - 11) );

    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( update_outdel_overflow_many_accounts )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing:  update_outdel_overflow with many actors" );
    generate_block();
    const int removal_limit = 200; //current mainnet value - if we increase it (we should, but it requires HF) we need to change it here as well
    db_plugin->debug_update( [=]( database& db )
    {
      db.modify( db.get_witness_schedule_object(), [&]( witness_schedule_object& wso )
      {
        wso.median_props.account_creation_fee = ASSET( "0.001 TESTS" ); //it effectively turns off minimum HP delegation limit
      });
      db.set_remove_threshold( removal_limit );
    });
    generate_block();
    #define NUM_ACTORS 250
    #define CREATE_ACTORS(z, n, text) ACTORS_DEFAULT_FEE( (actor ## n) );
    BOOST_PP_REPEAT(NUM_ACTORS, CREATE_ACTORS, )
    ACTORS_DEFAULT_FEE( (alice)(bob) )
    generate_block();

    vest( HIVE_INIT_MINER_NAME, "alice", ASSET( "0.010 TESTS" ) );
    generate_block();

    const rc_account_object& alice_rc_initial = db->get< rc_account_object, by_name >( "alice" );
    const account_object& alice_account_initial = db->get_account( "alice" );
    uint64_t vesting_amount = uint64_t(alice_account_initial.get_vesting().amount.value);
    int64_t creation_rc = alice_rc_initial.max_rc_creation_adjustment.amount.value;

    delegate_rc_operation op;
    op.from = "alice";
    op.max_rc = 10;
    // Delegate 10 rc to every actor account
    int count = 0;
    for (int i = 0; i < NUM_ACTORS; i++) {
      op.delegatees.insert( "actor" + std::to_string(i) );
      if (count == 50 || i == NUM_ACTORS -1 ) {
        push_custom_json_operation( {}, { "alice" }, HIVE_RC_PLUGIN_NAME, fc::json::to_string( rc_plugin_operation( op ) ), alice_private_key );
        generate_block();
        op.delegatees = {};
        count = 0;
      }
      count++;
    }

    // We delegate to bob last so that his delegation would be the last to be affected
    op.delegatees = {"bob"};
    op.max_rc = vesting_amount - NUM_ACTORS * 10 ;
    push_custom_json_operation( {}, { "alice" }, HIVE_RC_PLUGIN_NAME, fc::json::to_string( rc_plugin_operation( op ) ), alice_private_key );
    generate_block();

    const rc_account_object& actor0_rc_account_before = db->get< rc_account_object, by_name >("actor0");
    const rc_account_object& actor2_rc_account_before = db->get< rc_account_object, by_name >("actor2");
    const rc_account_object& alice_rc_before = db->get< rc_account_object, by_name >( "alice" );
    BOOST_REQUIRE( alice_rc_before.delegated_rc == vesting_amount );
    BOOST_REQUIRE( alice_rc_before.received_delegated_rc == 0 );
    BOOST_REQUIRE( alice_rc_before.last_max_rc == creation_rc );

    BOOST_REQUIRE( actor0_rc_account_before.rc_manabar.current_mana == creation_rc + 10 );
    BOOST_REQUIRE( actor0_rc_account_before.last_max_rc == creation_rc + 10 );
    BOOST_REQUIRE( actor0_rc_account_before.received_delegated_rc == 10 );

    BOOST_REQUIRE( actor2_rc_account_before.rc_manabar.current_mana == creation_rc + 10 );
    BOOST_REQUIRE( actor2_rc_account_before.last_max_rc == creation_rc + 10 );
    BOOST_REQUIRE( actor2_rc_account_before.received_delegated_rc == 10 );

    // we delegate 25 vests and it affects the first three delegations
    // Delegate 25 vests out, alice has rc deficit (she can't dip to creation_rc that results from max_rc_creation_adjustment)
    // So the first two delegations (to actor0 and actor1) are deleted while the delegation to actor2 is halved
    delegate_vesting_shares_operation dvso;
    dvso.vesting_shares = ASSET( "0.000025 VESTS");
    dvso.delegator = "alice";
    dvso.delegatee = "bob";
    push_transaction(dvso, alice_private_key);

    const rc_account_object& actor0_rc_account_after = db->get< rc_account_object, by_name >("actor0");
    const rc_account_object& actor2_rc_account_after = db->get< rc_account_object, by_name >("actor2");
    const rc_account_object& alice_rc_after = db->get< rc_account_object, by_name >( "alice" );

    BOOST_REQUIRE( alice_rc_after.delegated_rc == vesting_amount - 25 ); // total amount minus what was delegated
    BOOST_REQUIRE( alice_rc_after.received_delegated_rc == 0 );
    BOOST_REQUIRE( alice_rc_after.last_max_rc == creation_rc );

    BOOST_REQUIRE( actor0_rc_account_after.rc_manabar.current_mana == creation_rc );
    BOOST_REQUIRE( actor0_rc_account_after.last_max_rc == creation_rc );
    BOOST_REQUIRE( actor0_rc_account_after.received_delegated_rc == 0 );

    BOOST_REQUIRE( actor2_rc_account_after.rc_manabar.current_mana == creation_rc + 5 );
    BOOST_REQUIRE( actor2_rc_account_after.last_max_rc == creation_rc + 5 );
    BOOST_REQUIRE( actor2_rc_account_after.received_delegated_rc == 5 );

    const rc_direct_delegation_object* delegation_actor0_deleted = db->find< rc_direct_delegation_object, by_from_to >( boost::make_tuple( alice_id, actor0_id ) );
    BOOST_REQUIRE( delegation_actor0_deleted == nullptr );
    const rc_direct_delegation_object* delegation_actor1_deleted = db->find< rc_direct_delegation_object, by_from_to >( boost::make_tuple( alice_id, actor1_id ) );
    BOOST_REQUIRE( delegation_actor1_deleted == nullptr );

    // We check that the rest of the delegations weren't affected
    for (int i = 4; i < NUM_ACTORS; i++) {
      const rc_account_object& actor_rc_account = db->get< rc_account_object, by_name >( "actor" + std::to_string(i) );
      BOOST_REQUIRE( actor_rc_account.rc_manabar.current_mana == creation_rc + 10 );
      BOOST_REQUIRE( actor_rc_account.last_max_rc == creation_rc + 10 );
      BOOST_REQUIRE( actor_rc_account.received_delegated_rc == 10 );
    }

    // We delegate all our vests and we don't have enough to sustain any of our remaining delegations
    const account_object& acct = db->get_account( "alice" );
    dvso.vesting_shares = acct.vesting_shares;
    dvso.delegator = "alice";
    dvso.delegatee = "bob";
    push_transaction(dvso, alice_private_key);

    const rc_account_object& alice_rc_end = db->get< rc_account_object, by_name >( "alice" );

    BOOST_REQUIRE( alice_rc_end.delegated_rc == 0 );
    BOOST_REQUIRE( alice_rc_end.received_delegated_rc == 0 );
    BOOST_REQUIRE( alice_rc_end.last_max_rc == creation_rc );

    // we might wait couple of blocks due to limit on delegation removal
    int i = NUM_ACTORS - removal_limit;
    while( i > 0 )
    {
      generate_block();
      i -= removal_limit;
    }
    // We check that every delegation got deleted
    for (int i = 0; i < NUM_ACTORS; i++) {
      const rc_account_object& actor_rc_account = db->get< rc_account_object, by_name >( "actor" + std::to_string(i) );
      BOOST_REQUIRE( actor_rc_account.rc_manabar.current_mana == creation_rc );
      BOOST_REQUIRE( actor_rc_account.last_max_rc == creation_rc );
      BOOST_REQUIRE( actor_rc_account.received_delegated_rc == 0 );
    }

    // Remove vests delegation and check that we don't get the rc back immediately
    dvso.vesting_shares = ASSET( "0.000000 VESTS");
    dvso.delegator = "alice";
    dvso.delegatee = "bob";
    push_transaction(dvso, alice_private_key);

    const rc_account_object& alice_rc_final = db->get< rc_account_object, by_name >( "alice" );
    BOOST_REQUIRE( alice_rc_final.delegated_rc == 0 );
    BOOST_REQUIRE( alice_rc_final.received_delegated_rc == 0 );
    BOOST_REQUIRE( alice_rc_final.last_max_rc == creation_rc );
    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( direct_rc_delegation_vesting_withdrawal )
{
  try
  {
    generate_block();
    db_plugin->debug_update( [=]( database& db )
    {
      db.modify( db.get_witness_schedule_object(), [&]( witness_schedule_object& wso )
      {
        wso.median_props.account_creation_fee = ASSET( "0.001 TESTS" ); //it effectively turns off minimum HP delegation limit
      });
    });
    generate_block();
    ACTORS_DEFAULT_FEE( (alice)(bob)(dave) )
    generate_block();

    vest( HIVE_INIT_MINER_NAME, "alice", ASSET( "0.010 TESTS" ) );
    generate_block();

    const rc_account_object& alice_rc_initial = db->get< rc_account_object, by_name >( "alice" );
    const account_object& alice_account_initial = db->get_account( "alice" );
    int64_t creation_rc = alice_rc_initial.max_rc_creation_adjustment.amount.value;
    int64_t vesting_shares = alice_account_initial.get_vesting().amount.value;
    int64_t delegated_rc = vesting_shares / 2;

    BOOST_TEST_MESSAGE( "Setting up rc delegations" );

    delegate_rc_operation drc_op;
    drc_op.from = "alice";
    drc_op.delegatees = {"bob", "dave"};
    drc_op.max_rc = alice_account_initial.get_vesting().amount.value / 2;
    push_custom_json_operation( {}, { "alice" }, HIVE_RC_PLUGIN_NAME, fc::json::to_string( rc_plugin_operation( drc_op ) ), alice_private_key );
    generate_block();

    BOOST_TEST_MESSAGE( "Setting up withdrawal" );
    const auto& new_alice = db->get_account( "alice" );

    withdraw_vesting_operation op;
    op.account = "alice";
    op.vesting_shares = new_alice.get_vesting();
    push_transaction(op, alice_private_key);

    auto next_withdrawal = db->head_block_time() + HIVE_VESTING_WITHDRAW_INTERVAL_SECONDS;
    asset withdraw_rate = new_alice.vesting_withdraw_rate;

    BOOST_TEST_MESSAGE( "Generating block up to first withdrawal" );
    generate_blocks( next_withdrawal - HIVE_BLOCK_INTERVAL );

    BOOST_REQUIRE( get_vesting( "alice" ).amount.value == vesting_shares );

    {
      const rc_account_object& alice_rc_account = db->get< rc_account_object, by_name >( "alice" );
      const rc_account_object& bob_rc_account = db->get< rc_account_object, by_name >("bob");
      const rc_account_object& dave_rc_account = db->get< rc_account_object, by_name >("dave");

      BOOST_REQUIRE( alice_rc_account.delegated_rc == uint64_t(vesting_shares - withdraw_rate.amount.value) ); // total amount minus what was withdrew
      BOOST_REQUIRE( alice_rc_account.received_delegated_rc == 0 );
      BOOST_REQUIRE( alice_rc_account.last_max_rc == creation_rc );

      // There wasn't enough to sustain the delegation to bob, so it got modified
      BOOST_REQUIRE( bob_rc_account.rc_manabar.current_mana == delegated_rc + creation_rc - withdraw_rate.amount.value );
      BOOST_REQUIRE( bob_rc_account.received_delegated_rc == uint64_t(delegated_rc - withdraw_rate.amount.value) );
      BOOST_REQUIRE( bob_rc_account.last_max_rc == delegated_rc - withdraw_rate.amount.value + creation_rc );

      BOOST_REQUIRE( dave_rc_account.rc_manabar.current_mana == creation_rc + delegated_rc );
      BOOST_REQUIRE( dave_rc_account.last_max_rc == creation_rc + delegated_rc );
      BOOST_REQUIRE( dave_rc_account.received_delegated_rc == uint64_t(delegated_rc) );

      const rc_direct_delegation_object* delegation_bob = db->find< rc_direct_delegation_object, by_from_to >( boost::make_tuple( alice_id, bob_id ) );
      BOOST_REQUIRE( delegation_bob->from == alice_id );
      BOOST_REQUIRE( delegation_bob->to == bob_id );
      BOOST_REQUIRE( delegation_bob->delegated_rc == uint64_t(delegated_rc - withdraw_rate.amount.value) );
      const rc_direct_delegation_object* delegation_dave = db->find< rc_direct_delegation_object, by_from_to >( boost::make_tuple( alice_id, dave_id ) );
      BOOST_REQUIRE( delegation_dave->from == alice_id );
      BOOST_REQUIRE( delegation_dave->to == dave_id );
      BOOST_REQUIRE( delegation_dave->delegated_rc == uint64_t(delegated_rc) );
    }

    BOOST_TEST_MESSAGE( "Generating block to cause withdrawal" );
    generate_block();

    for( int i = 1; i < HIVE_VESTING_WITHDRAW_INTERVALS - 1; i++ )
    {
      // generate up to just before the withdrawal
      generate_blocks( db->head_block_time() + HIVE_VESTING_WITHDRAW_INTERVAL_SECONDS - HIVE_BLOCK_INTERVAL);

      const rc_account_object& alice_rc_account = db->get< rc_account_object, by_name >( "alice" );
      const rc_account_object& bob_rc_account = db->get< rc_account_object, by_name >("bob");
      const rc_account_object& dave_rc_account = db->get< rc_account_object, by_name >("dave");
      const rc_direct_delegation_object* delegation_bob = db->find< rc_direct_delegation_object, by_from_to >( boost::make_tuple( alice_id, bob_id ) );
      const rc_direct_delegation_object* delegation_dave = db->find< rc_direct_delegation_object, by_from_to >( boost::make_tuple( alice_id, dave_id ) );
      auto total_vests_withdrew = withdraw_rate.amount.value * (i + 1);

      BOOST_REQUIRE( alice_rc_account.delegated_rc == uint64_t(vesting_shares - total_vests_withdrew) ); // total amount minus what was withdrew
      BOOST_REQUIRE( alice_rc_account.rc_manabar.current_mana == creation_rc);
      BOOST_REQUIRE( alice_rc_account.received_delegated_rc == 0 );
      BOOST_REQUIRE( alice_rc_account.last_max_rc == creation_rc);

      if (total_vests_withdrew < delegated_rc) {
        // Only the first delegation is affected
        BOOST_REQUIRE( bob_rc_account.rc_manabar.current_mana == delegated_rc + creation_rc - total_vests_withdrew);
        BOOST_REQUIRE( bob_rc_account.received_delegated_rc == uint64_t(delegated_rc - total_vests_withdrew) );
        BOOST_REQUIRE( bob_rc_account.last_max_rc == delegated_rc + creation_rc - total_vests_withdrew);

        BOOST_REQUIRE( dave_rc_account.rc_manabar.current_mana == delegated_rc + creation_rc );
        BOOST_REQUIRE( dave_rc_account.last_max_rc == delegated_rc + creation_rc );
        BOOST_REQUIRE( dave_rc_account.received_delegated_rc == uint64_t(delegated_rc) );

        BOOST_REQUIRE( delegation_bob->from == alice_id );
        BOOST_REQUIRE( delegation_bob->to == bob_id );
        BOOST_REQUIRE( delegation_bob->delegated_rc == uint64_t(delegated_rc - total_vests_withdrew));

        BOOST_REQUIRE( delegation_dave->from == alice_id );
        BOOST_REQUIRE( delegation_dave->to == dave_id );
        BOOST_REQUIRE( delegation_dave->delegated_rc == uint64_t(delegated_rc) );
      } else {
        // the first delegation is deleted and the second delegation starts to be affected
        BOOST_REQUIRE( bob_rc_account.rc_manabar.current_mana == creation_rc);
        BOOST_REQUIRE( bob_rc_account.received_delegated_rc == 0 );
        BOOST_REQUIRE( bob_rc_account.last_max_rc == creation_rc );
        BOOST_REQUIRE( dave_rc_account.rc_manabar.current_mana == creation_rc + delegated_rc - (total_vests_withdrew - delegated_rc));
        BOOST_REQUIRE( dave_rc_account.received_delegated_rc == uint64_t(delegated_rc - (total_vests_withdrew - delegated_rc)));
        BOOST_REQUIRE( dave_rc_account.last_max_rc == creation_rc + delegated_rc - (total_vests_withdrew - delegated_rc));

        BOOST_REQUIRE( delegation_bob == nullptr );

        BOOST_REQUIRE( delegation_dave->from == alice_id );
        BOOST_REQUIRE( delegation_dave->to == dave_id );
        BOOST_REQUIRE( delegation_dave->delegated_rc == uint64_t(vesting_shares - total_vests_withdrew) );
      }

      generate_block();
    }

    BOOST_TEST_MESSAGE( "Generating one more block to finish vesting withdrawal" );
    generate_blocks( db->head_block_time() + HIVE_VESTING_WITHDRAW_INTERVAL_SECONDS, true );

    {
      const rc_account_object& alice_rc_account = db->get< rc_account_object, by_name >( "alice" );
      const rc_account_object& bob_rc_account = db->get< rc_account_object, by_name >("bob");
      const rc_account_object& dave_rc_account = db->get< rc_account_object, by_name >("dave");

      BOOST_REQUIRE( alice_rc_account.delegated_rc == 0 );
      BOOST_REQUIRE( alice_rc_account.received_delegated_rc == 0 );
      BOOST_REQUIRE( alice_rc_account.rc_manabar.current_mana == creation_rc );
      BOOST_REQUIRE( alice_rc_account.last_max_rc == creation_rc );

      BOOST_REQUIRE( bob_rc_account.rc_manabar.current_mana == creation_rc );
      BOOST_REQUIRE( bob_rc_account.last_max_rc == creation_rc );
      BOOST_REQUIRE( bob_rc_account.received_delegated_rc == 0 );

      BOOST_REQUIRE( dave_rc_account.rc_manabar.current_mana == creation_rc );
      BOOST_REQUIRE( dave_rc_account.last_max_rc == creation_rc );
      BOOST_REQUIRE( dave_rc_account.received_delegated_rc == 0 );

      const rc_direct_delegation_object* delegation_bob = db->find< rc_direct_delegation_object, by_from_to >( boost::make_tuple( alice_id, bob_id ) );
      BOOST_REQUIRE( delegation_bob == nullptr );
      const rc_direct_delegation_object* delegation_dave = db->find< rc_direct_delegation_object, by_from_to >( boost::make_tuple( alice_id, dave_id ) );
      BOOST_REQUIRE( delegation_dave == nullptr );
    }
    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( direct_rc_delegation_vesting_withdrawal_routes )
{
  try
  {
    generate_block();
    db_plugin->debug_update( [=]( database& db )
    {
      db.modify( db.get_witness_schedule_object(), [&]( witness_schedule_object& wso )
      {
        wso.median_props.account_creation_fee = ASSET( "0.001 TESTS" ); //it effectively turns off minimum HP delegation limit
      });
    });
    generate_block();
    ACTORS_DEFAULT_FEE( (alice)(bob)(dave) )
    generate_block();

    vest( HIVE_INIT_MINER_NAME, "alice", ASSET( "0.010 TESTS" ) );
    generate_block();

    const rc_account_object& alice_rc_initial = db->get< rc_account_object, by_name >( "alice" );
    const account_object& alice_account_initial = db->get_account( "alice" );
    int64_t creation_rc = alice_rc_initial.max_rc_creation_adjustment.amount.value;
    int64_t vesting_shares = alice_account_initial.get_vesting().amount.value;
    int64_t delegated_rc = vesting_shares / 2;

    BOOST_TEST_MESSAGE( "Setting up vesting routes" );
    set_withdraw_vesting_route( "alice", "bob", HIVE_1_PERCENT * 25, true, alice_private_key);
    set_withdraw_vesting_route( "alice", "alice", HIVE_1_PERCENT * 50, true, alice_private_key);
    set_withdraw_vesting_route( "alice", "dave", HIVE_1_PERCENT * 25, false, alice_private_key);
    
    BOOST_TEST_MESSAGE( "Setting up rc delegations" );

    delegate_rc_operation drc_op;
    drc_op.from = "alice";
    drc_op.delegatees = {"bob", "dave"};
    drc_op.max_rc = delegated_rc;
    push_custom_json_operation( {}, { "alice" }, HIVE_RC_PLUGIN_NAME, fc::json::to_string( rc_plugin_operation( drc_op ) ), alice_private_key );
    generate_block();

    BOOST_TEST_MESSAGE( "Setting up withdrawal" );
    const auto& new_alice = db->get_account( "alice" );

    withdraw_vesting_operation op;
    op.account = "alice";
    op.vesting_shares = asset( new_alice.get_vesting().amount, VESTS_SYMBOL );
    push_transaction(op, alice_private_key);

    auto next_withdrawal = db->head_block_time() + HIVE_VESTING_WITHDRAW_INTERVAL_SECONDS;
    asset withdraw_rate = new_alice.vesting_withdraw_rate;

    BOOST_TEST_MESSAGE( "Generating block up to first withdrawal" );
    generate_blocks( next_withdrawal - HIVE_BLOCK_INTERVAL );

    BOOST_REQUIRE( get_vesting( "alice" ).amount.value == vesting_shares );
    {
      const rc_account_object& alice_rc_account = db->get< rc_account_object, by_name >( "alice" );
      const rc_account_object& bob_rc_account = db->get< rc_account_object, by_name >("bob");
      const rc_account_object& dave_rc_account = db->get< rc_account_object, by_name >("dave");

      BOOST_REQUIRE( alice_rc_account.delegated_rc == uint64_t(vesting_shares - withdraw_rate.amount.value) ); // total amount minus what was withdrew
      BOOST_REQUIRE( alice_rc_account.received_delegated_rc == 0 );
      BOOST_REQUIRE( alice_rc_account.last_max_rc == creation_rc );

      // There wasn't enough to sustain the delegation to bob, so it got modified
      BOOST_REQUIRE( bob_rc_account.rc_manabar.current_mana == delegated_rc + creation_rc - withdraw_rate.amount.value );
      BOOST_REQUIRE( bob_rc_account.received_delegated_rc == uint64_t(delegated_rc - withdraw_rate.amount.value) );
      BOOST_REQUIRE( bob_rc_account.last_max_rc == delegated_rc - withdraw_rate.amount.value + creation_rc );

      BOOST_REQUIRE( dave_rc_account.rc_manabar.current_mana == creation_rc + delegated_rc );
      BOOST_REQUIRE( dave_rc_account.last_max_rc == creation_rc + delegated_rc );
      BOOST_REQUIRE( dave_rc_account.received_delegated_rc == uint64_t(delegated_rc) );

      const rc_direct_delegation_object* delegation_bob = db->find< rc_direct_delegation_object, by_from_to >( boost::make_tuple( alice_id, bob_id ) );
      BOOST_REQUIRE( delegation_bob->from == alice_id );
      BOOST_REQUIRE( delegation_bob->to == bob_id );
      BOOST_REQUIRE( delegation_bob->delegated_rc == uint64_t(delegated_rc - withdraw_rate.amount.value) );
      const rc_direct_delegation_object* delegation_dave = db->find< rc_direct_delegation_object, by_from_to >( boost::make_tuple( alice_id, dave_id ) );
      BOOST_REQUIRE( delegation_dave->from == alice_id );
      BOOST_REQUIRE( delegation_dave->to == dave_id );
      BOOST_REQUIRE( delegation_dave->delegated_rc == uint64_t(delegated_rc) );
    }

    BOOST_TEST_MESSAGE( "Generating block to cause withdrawal" );
    generate_block();

    {
      const rc_account_object& alice_rc_account = db->get< rc_account_object, by_name >( "alice" );
      const rc_account_object& bob_rc_account = db->get< rc_account_object, by_name >("bob");
      const rc_account_object& dave_rc_account = db->get< rc_account_object, by_name >("dave");

      BOOST_REQUIRE( alice_rc_account.delegated_rc == uint64_t(vesting_shares - (withdraw_rate.amount.value * 2 - withdraw_rate.amount.value / 2 )));
      BOOST_REQUIRE( alice_rc_account.received_delegated_rc == 0 );
      BOOST_REQUIRE( alice_rc_account.rc_manabar.current_mana == creation_rc); //  withdraw_rate.amount.value is withdrew, but half is auto vested back
      BOOST_REQUIRE( alice_rc_account.last_max_rc == creation_rc );

      // There wasn't enough to sustain the delegation to bob, so it got modified
      // We remove *1.5 instead of *2 because alice auto vested half, so she only needed to remove 0.5 of the withdraw_rate to reach creation_rc this round
      BOOST_REQUIRE( bob_rc_account.rc_manabar.current_mana == delegated_rc + creation_rc - withdraw_rate.amount.value * 1.5 + withdraw_rate.amount.value / 4);
      BOOST_REQUIRE( bob_rc_account.received_delegated_rc == uint64_t(delegated_rc - withdraw_rate.amount.value * 1.5));
      BOOST_REQUIRE( bob_rc_account.last_max_rc == delegated_rc + creation_rc - withdraw_rate.amount.value * 1.5  + withdraw_rate.amount.value / 4 );

      BOOST_REQUIRE( dave_rc_account.rc_manabar.current_mana == creation_rc + delegated_rc );
      BOOST_REQUIRE( dave_rc_account.last_max_rc == creation_rc + delegated_rc );
      BOOST_REQUIRE( dave_rc_account.received_delegated_rc == uint64_t(delegated_rc) );

      const rc_direct_delegation_object* delegation_bob = db->find< rc_direct_delegation_object, by_from_to >( boost::make_tuple( alice_id, bob_id ) );
      BOOST_REQUIRE( delegation_bob->from == alice_id );
      BOOST_REQUIRE( delegation_bob->to == bob_id );
      BOOST_REQUIRE( delegation_bob->delegated_rc == uint64_t(delegated_rc - withdraw_rate.amount.value * 1.5) );
      const rc_direct_delegation_object* delegation_dave = db->find< rc_direct_delegation_object, by_from_to >( boost::make_tuple( alice_id, dave_id ) );
      BOOST_REQUIRE( delegation_dave->from == alice_id );
      BOOST_REQUIRE( delegation_dave->to == dave_id );
      BOOST_REQUIRE( delegation_dave->delegated_rc == uint64_t(delegated_rc) );
    }

    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( rc_delegation_regeneration )
{
  try
  {
    generate_block();
    db_plugin->debug_update( [=]( database& db )
    {
      db.modify( db.get_witness_schedule_object(), [&]( witness_schedule_object& wso )
      {
        wso.median_props.account_creation_fee = ASSET( "0.001 TESTS" ); //it effectively turns off minimum HP delegation limit
      });
    });
    generate_block();

    ACTORS_DEFAULT_FEE( (alice)(bob) )
    generate_block();

    transfer( HIVE_INIT_MINER_NAME, "bob", ASSET( "100.000 TBD" ));
    vest( HIVE_INIT_MINER_NAME, "alice", ASSET( "1.000 TESTS" ) );
    generate_block();

    BOOST_TEST_MESSAGE( "Fetching the baseLine with no RC delegations" );
    const rc_account_object& bob_rc_account_initial = db->get< rc_account_object, by_name >("bob");
    auto initial_mana = bob_rc_account_initial.rc_manabar.current_mana;
    auto current_mana = initial_mana;

    while (current_mana + 5000000 > initial_mana) {
      transfer( "bob", "bob", ASSET( "1.000 TBD" ));
      const rc_account_object& bob_rc_account_current = db->get< rc_account_object, by_name >("bob");
      current_mana = bob_rc_account_current.rc_manabar.current_mana;
    }

    int64_t delta_no_delegation = 0;
    {
      const rc_account_object& bob_rc_account = db->get< rc_account_object, by_name >("bob");
      hive::chain::util::manabar_params manabar_params(get_maximum_rc( db->get< account_object, by_name >( "bob" ),  bob_rc_account), HIVE_RC_REGEN_TIME);
      auto bob_rc_manabar = bob_rc_account.rc_manabar;
      // Magic number: we renerate based off the future, because otherwise the manabar will already be up to date and won't regenerate
      bob_rc_manabar.regenerate_mana( manabar_params, db->get_dynamic_global_properties().time.sec_since_epoch() + 1);
      delta_no_delegation = bob_rc_manabar.current_mana - current_mana;
    }

    BOOST_TEST_MESSAGE( "Delegating all the RC" );
    delegate_rc_operation drc_op;
    drc_op.from = "alice";
    drc_op.delegatees = {"bob"};
    drc_op.max_rc = db->get_account( "alice" ).get_vesting().amount.value;
    custom_json_operation custom_op;
    custom_op.required_posting_auths.insert( "alice" );
    custom_op.id = HIVE_RC_PLUGIN_NAME;
    custom_op.json = fc::json::to_string( rc_plugin_operation( drc_op ) );
    push_transaction(custom_op, alice_private_key);
    generate_block();

    {
      const rc_account_object& bob_rc_account_initial = db->get< rc_account_object, by_name >("bob");
      auto initial_mana = bob_rc_account_initial.rc_manabar.current_mana;
      current_mana = initial_mana;

      while (current_mana + 5000000 > initial_mana) {
        transfer( "bob", "bob", ASSET( "1.000 TBD" ));
        const rc_account_object& bob_rc_account_current = db->get< rc_account_object, by_name >("bob");
        current_mana = bob_rc_account_current.rc_manabar.current_mana;
      }
    }

    int64_t delta_delegation = 0;
    {
      const rc_account_object& bob_rc_account = db->get< rc_account_object, by_name >("bob");
      hive::chain::util::manabar_params manabar_params(get_maximum_rc( db->get< account_object, by_name >( "bob" ),  bob_rc_account), HIVE_RC_REGEN_TIME);
      auto bob_rc_manabar = bob_rc_account.rc_manabar;
      // Magic number: we renerate based off the future, because otherwise the manabar will already be up to date and won't regenerate
      bob_rc_manabar.regenerate_mana( manabar_params, db->get_dynamic_global_properties().time.sec_since_epoch() + 1);
      delta_delegation = bob_rc_manabar.current_mana - current_mana;
    }

    BOOST_REQUIRE( delta_delegation > delta_no_delegation);

    BOOST_TEST_MESSAGE( "Reducing the RC delegation" );
    drc_op.max_rc = db->get_account( "alice" ).get_vesting().amount.value / 2;
    custom_op.json = fc::json::to_string( rc_plugin_operation( drc_op ) );
    push_transaction(custom_op, alice_private_key);
    generate_block();

    {
      const rc_account_object& bob_rc_account_initial = db->get< rc_account_object, by_name >("bob");
      auto initial_mana = bob_rc_account_initial.rc_manabar.current_mana;
      current_mana = initial_mana;

      while (current_mana + 5000000 > initial_mana) {
        transfer( "bob", "bob", ASSET( "1.000 TBD" ));
        const rc_account_object& bob_rc_account_current = db->get< rc_account_object, by_name >("bob");
        current_mana = bob_rc_account_current.rc_manabar.current_mana;
      }
    }

    int64_t delta_reduced_delegation = 0;
    {
      const rc_account_object& bob_rc_account = db->get< rc_account_object, by_name >("bob");
      hive::chain::util::manabar_params manabar_params(get_maximum_rc( db->get< account_object, by_name >( "bob" ),  bob_rc_account), HIVE_RC_REGEN_TIME);
      auto bob_rc_manabar = bob_rc_account.rc_manabar;
      // Magic number: we renerate based off the future, because otherwise the manabar will already be up to date and won't regenerate
      bob_rc_manabar.regenerate_mana( manabar_params, db->get_dynamic_global_properties().time.sec_since_epoch() + 1);
      delta_reduced_delegation = bob_rc_manabar.current_mana - current_mana;
    }

    BOOST_TEST_MESSAGE( "Removing the RC delegation" );
    drc_op.max_rc = 0;
    custom_op.json = fc::json::to_string( rc_plugin_operation( drc_op ) );
    push_transaction(custom_op, alice_private_key);
    generate_block();

    {
      const rc_account_object& bob_rc_account_initial = db->get< rc_account_object, by_name >("bob");
      auto initial_mana = bob_rc_account_initial.rc_manabar.current_mana;
      current_mana = initial_mana;

      while (current_mana + 5000000 > initial_mana) {
        transfer( "bob", "bob", ASSET( "1.000 TBD" ));
        const rc_account_object& bob_rc_account_current = db->get< rc_account_object, by_name >("bob");
        current_mana = bob_rc_account_current.rc_manabar.current_mana;
      }
    }

    BOOST_REQUIRE( delta_delegation > delta_reduced_delegation);

    int64_t delta_removed_delegation = 0;
    {
      const rc_account_object& bob_rc_account = db->get< rc_account_object, by_name >("bob");
      hive::chain::util::manabar_params manabar_params(get_maximum_rc( db->get< account_object, by_name >( "bob" ),  bob_rc_account), HIVE_RC_REGEN_TIME);
      auto bob_rc_manabar = bob_rc_account.rc_manabar;
      // Magic number: we renerate based off the future, because otherwise the manabar will already be up to date and won't regenerate
      bob_rc_manabar.regenerate_mana( manabar_params, db->get_dynamic_global_properties().time.sec_since_epoch() + 1);
      delta_removed_delegation = bob_rc_manabar.current_mana - current_mana;
    }

    BOOST_REQUIRE(delta_delegation > delta_removed_delegation);
    BOOST_REQUIRE(delta_reduced_delegation > delta_removed_delegation);
    BOOST_REQUIRE(delta_removed_delegation == delta_no_delegation);

    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( rc_delegation_removal_no_rc )
{
  try
  {
    generate_block();
    db_plugin->debug_update( [=]( database& db )
    {
      db.modify( db.get_witness_schedule_object(), [&]( witness_schedule_object& wso )
      {
        wso.median_props.account_creation_fee = ASSET( "0.001 TESTS" ); //it effectively turns off minimum HP delegation limit
      });
    });
    generate_block();

    ACTORS_DEFAULT_FEE( (alice)(bob) )
    generate_block();
    transfer( HIVE_INIT_MINER_NAME, "bob", ASSET( "9000000.000 TESTS" ));
    vest( HIVE_INIT_MINER_NAME, "alice", ASSET( "1.000 TESTS" ) );
    generate_block();

    BOOST_TEST_MESSAGE( "delegating RC to bob" );
    delegate_rc_operation drc_op;
    drc_op.from = "alice";
    drc_op.delegatees = {"bob"};
    drc_op.max_rc = 100000;
    custom_json_operation custom_op;
    custom_op.required_posting_auths.insert( "alice" );
    custom_op.id = HIVE_RC_PLUGIN_NAME;
    custom_op.json = fc::json::to_string( rc_plugin_operation( drc_op ) );
    push_transaction(custom_op, alice_private_key);
    generate_block();

    const rc_account_object& bob_rc_account_initial = db->get< rc_account_object, by_name >("bob");
    auto current_mana = bob_rc_account_initial.rc_manabar.current_mana;

    // Magic number, it's hard to exactly hit 0 RC, but since we delegated 100k rc, removing the delegation would put us to 0
    while (current_mana > 50000) {
      signed_transaction tx;
      transfer_to_vesting_operation op;
      op.from = "bob";
      op.to = "alice";
      op.amount = ASSET( "0.001 TESTS" );
      push_transaction(op, bob_private_key);
      generate_block();
      const rc_account_object& bob_rc_account_current = db->get< rc_account_object, by_name >("bob");
      current_mana = bob_rc_account_current.rc_manabar.current_mana;
    }

    BOOST_TEST_MESSAGE( "Removing the RC delegation" );
    drc_op.max_rc = 0;
    custom_op.json = fc::json::to_string( rc_plugin_operation( drc_op ) );
    push_transaction(custom_op, alice_private_key);
    generate_block();

    const rc_account_object& bob_rc_account = db->get< rc_account_object, by_name >("bob");
    BOOST_REQUIRE(bob_rc_account.rc_manabar.current_mana == 0 );
    BOOST_REQUIRE(get_maximum_rc( db->get< account_object, by_name >( "bob" ),  bob_rc_account) >= 0);

    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( rc_negative_regeneration_bug )
{
  try
  {
    BOOST_TEST_MESSAGE( "Negative RC regeneration bug" );
    auto auto_reset( set_mainnet_cashout_values() ); //time is needed for RC to regenerate - testnet values are too small

    //the problem is very cryptic
    //first thing - it involves negative RC which shouldn't really ever happen (but does, for this
    //test it will be just set artificially)
    //second - it needs to be receiver of RC delegation that loses that delegation due to delegator
    //powering down more than he left undelegated
    //third - the bug only shows when significant time passed between the moment when RC of delegatee
    //went negative and the moment it lost delegation - the bug shows as delegatee having less RC
    //than he would have if RC regeneration was triggered by other means

    ACTORS( (delegator1)(delegator2)(delegator3)(delegatee)(pattern2)(pattern3) )
    generate_block();
    vest( "initminer", "delegator1", ASSET( "1000.000 TESTS" ) );
    vest( "initminer", "delegator2", ASSET( "1000.000 TESTS" ) );
    vest( "initminer", "delegator3", ASSET( "1000.000 TESTS" ) );
    int64_t full_vest = get_vesting( "delegator1" ).amount.value;
    BOOST_REQUIRE_EQUAL( full_vest, get_vesting( "delegator2" ).amount.value );
    BOOST_REQUIRE_EQUAL( full_vest, get_vesting( "delegator3" ).amount.value );

    const auto& delegatee_rc = db->get< rc_account_object, by_name >( "delegatee" );
    const auto& pattern2_rc = db->get< rc_account_object, by_name >( "pattern2" );
    const auto& pattern3_rc = db->get< rc_account_object, by_name >( "pattern3" );
    BOOST_REQUIRE_EQUAL( delegatee_rc.rc_manabar.current_mana, pattern2_rc.rc_manabar.current_mana );
    BOOST_REQUIRE_EQUAL( delegatee_rc.rc_manabar.current_mana, pattern3_rc.rc_manabar.current_mana );

    BOOST_TEST_MESSAGE( "Pattern2 makes a comment and delegator2 votes on it - payout will trigger RC regen" );
    comment_operation comment;
    comment.parent_permlink = "test";
    comment.author = "pattern2";
    comment.permlink = "test";
    comment.title = "test";
    comment.body = "test";
    push_transaction( comment, pattern2_private_key );
    generate_block();
    auto cashout_time = db->find_comment_cashout( db->get_comment( "pattern2", string( "test" ) ) )->get_cashout_time();

    vote_operation vote;
    vote.voter = "delegator2";
    vote.author = "pattern2";
    vote.permlink = "test";
    vote.weight = HIVE_100_PERCENT;
    push_transaction( vote, delegator2_private_key );
    generate_block();

    //wait a bit so the RC used for comment is restored
    generate_blocks( db->head_block_time() + fc::seconds( 60 ) );

    BOOST_TEST_MESSAGE( "All delegators make RC delegations to their delegatees with the same power" );
    auto rc_delegate = [&]( const account_name_type& from, const account_name_type& to, int64_t amount, const fc::ecc::private_key& key )
    {
      delegate_rc_operation delegate;
      delegate.from = from;
      delegate.delegatees = { to };
      delegate.max_rc = amount;
      push_custom_json_operation( {}, { from }, HIVE_RC_PLUGIN_NAME, fc::json::to_string( rc_plugin_operation( delegate ) ), key );
    };
    rc_delegate( "delegator1", "delegatee", full_vest, delegator1_private_key );
    rc_delegate( "delegator2", "pattern2", full_vest, delegator2_private_key );
    rc_delegate( "delegator3", "pattern3", full_vest, delegator3_private_key );
    generate_block();
    BOOST_REQUIRE_EQUAL( delegatee_rc.rc_manabar.current_mana, pattern2_rc.rc_manabar.current_mana );
    BOOST_REQUIRE_EQUAL( delegatee_rc.rc_manabar.current_mana, pattern3_rc.rc_manabar.current_mana );

    BOOST_TEST_MESSAGE( "Artificially going negative with RC of delegatees to expose problem" );
    //NOTE: the problem was fixed and on top of that we can no longer go negative with mana,
    //so we could probably remove the test, but we keep it as testament of previous bugs
    //int64_t full_rc = get_maximum_rc( db->get< account_object, by_name >( "delegatee" ), delegatee_rc );
    //int64_t min_rc = -1 * ( HIVE_RC_MAX_NEGATIVE_PERCENT * full_rc ) / HIVE_100_PERCENT;
    int64_t min_rc = 0;
    auto rc_set_negative_mana = [&]( const rc_account_object& rca )
    {
      db->modify( rca, [&]( rc_account_object& rc_account )
      {
        rc_account.rc_manabar.current_mana = min_rc;
        rc_account.rc_manabar.last_update_time = ( db->head_block_time() - fc::seconds( HIVE_RC_REGEN_TIME ) ).sec_since_epoch();
      } );
    };
    rc_set_negative_mana( delegatee_rc );
    rc_set_negative_mana( pattern2_rc );
    rc_set_negative_mana( pattern3_rc );
    generate_block();

    BOOST_TEST_MESSAGE( "Wait for block when comment payout will trigger regen, power down and take away delegation in the same time" );
    generate_blocks( cashout_time - fc::seconds( HIVE_BLOCK_INTERVAL ) );
    withdraw_vesting_operation power_down;
    power_down.account = "delegator1";
    power_down.vesting_shares = asset( full_vest, VESTS_SYMBOL );
    push_transaction( power_down, delegator1_private_key );
    int64_t undelegated = hive::plugins::rc::detail::get_next_vesting_withdrawal( db->get< account_object, by_name >( "delegator1" ) );
    rc_delegate( "delegator3", "pattern3", full_vest - undelegated, delegator3_private_key );
    generate_block();
    //pattern2 RC regeneration is triggered by author_reward_operation, but it doesn't modify RC because rewards go to separate balance until claimed
    BOOST_REQUIRE_EQUAL( delegatee_rc.rc_manabar.current_mana, pattern2_rc.rc_manabar.current_mana - undelegated );
    //pattern3 undelegated exactly the same amount of RC as was dropped from delegatee by delegator1 powering down
    BOOST_REQUIRE_EQUAL( delegatee_rc.rc_manabar.current_mana, pattern3_rc.rc_manabar.current_mana );

    validate_database();
  }
  FC_LOG_AND_RETHROW()
}


BOOST_AUTO_TEST_CASE( update_outdel_overflow_delegatee )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing:  update_outdel_overflow with a vesting delegation" );
    generate_block();
    db_plugin->debug_update( [=]( database& db )
    {
      db.modify( db.get_witness_schedule_object(), [&]( witness_schedule_object& wso )
      {
        wso.median_props.account_creation_fee = ASSET( "0.001 TESTS" ); //it effectively turns off minimum HP delegation limit
      });
    });
    generate_block();

    ACTORS_DEFAULT_FEE( (alice)(bob)(dave)(eve)(martin) )
    generate_block();
    vest( HIVE_INIT_MINER_NAME, "alice", ASSET( "0.010 TESTS" ) );
    generate_block();

    const rc_account_object& alice_rc_initial = db->get< rc_account_object, by_name >( "alice" );
    const account_object& alice_account_initial = db->get_account( "alice" );
    int64_t vesting_amount = alice_account_initial.get_vesting().amount.value;
    int64_t creation_rc = alice_rc_initial.max_rc_creation_adjustment.amount.value;

    // We delegate all our vesting shares to bob
    delegate_vesting_shares_operation dvso;
    dvso.vesting_shares = alice_account_initial.get_vesting();
    dvso.delegator = "alice";
    dvso.delegatee = {"bob"};
    push_transaction(dvso, alice_private_key);

    // Delegate 10 rc to dave, the rest to eve, bob has max_rc_creation_adjustment remaining rc
    delegate_rc_operation op;
    op.from = "bob";
    op.delegatees = {"dave"};
    op.max_rc = 10;
    custom_json_operation custom_op;
    custom_op.required_posting_auths.insert( "bob" );
    custom_op.id = HIVE_RC_PLUGIN_NAME;
    custom_op.json = fc::json::to_string( rc_plugin_operation( op ) );
    push_transaction(custom_op, bob_private_key);
    op.delegatees = {"eve"};
    op.max_rc = vesting_amount - 10 + 1;
    custom_op.json = fc::json::to_string( rc_plugin_operation( op ) );
    //check that it is not possible to dip into creation_rc, nor will it be accepted at the cost of dropping eve
    HIVE_REQUIRE_ASSERT( push_transaction( custom_op, bob_private_key ), "from_delegable_rc >= delta_total" );

    op.max_rc = vesting_amount - 10;
    custom_op.json = fc::json::to_string( rc_plugin_operation( op ) );
    push_transaction(custom_op, bob_private_key);
    generate_block();

    const rc_account_object& alice_rc_before = db->get< rc_account_object, by_name >( "alice" );
    const rc_account_object& bob_rc_account_before = db->get< rc_account_object, by_name >("bob");
    const rc_account_object& dave_rc_account_before = db->get< rc_account_object, by_name >("dave");
    const rc_account_object& eve_rc_account_before = db->get< rc_account_object, by_name >("eve");

    BOOST_REQUIRE( alice_rc_before.delegated_rc == 0 );
    BOOST_REQUIRE( alice_rc_before.received_delegated_rc == 0 );
    BOOST_REQUIRE( alice_rc_before.last_max_rc == creation_rc );

    BOOST_REQUIRE( bob_rc_account_before.delegated_rc == uint64_t(vesting_amount) );
    BOOST_REQUIRE( bob_rc_account_before.last_max_rc == creation_rc );
    BOOST_REQUIRE( bob_rc_account_before.received_delegated_rc == 0 );
    BOOST_REQUIRE( bob_rc_account_before.rc_manabar.current_mana == creation_rc - ( 28755 + 28756 ) ); // cost of the two delegate rc ops (the one to dave costs more because more data is in the op)

    BOOST_REQUIRE( eve_rc_account_before.rc_manabar.current_mana == creation_rc + vesting_amount - 10 );
    BOOST_REQUIRE( eve_rc_account_before.last_max_rc == creation_rc + vesting_amount - 10 );
    BOOST_REQUIRE( eve_rc_account_before.received_delegated_rc == uint64_t(vesting_amount - 10) );

    BOOST_REQUIRE( dave_rc_account_before.rc_manabar.current_mana == creation_rc + 10 );
    BOOST_REQUIRE( dave_rc_account_before.last_max_rc == creation_rc + 10 );
    BOOST_REQUIRE( dave_rc_account_before.received_delegated_rc == 10 );

    // we remove the vesting delegation and it affects the rc delegations
    dvso.vesting_shares = ASSET( "0.000000 VESTS");
    push_transaction(dvso, alice_private_key);

    const rc_account_object& alice_rc_after = db->get< rc_account_object, by_name >( "alice" );
    const rc_account_object& bob_rc_account_after = db->get< rc_account_object, by_name >("bob");
    const rc_account_object& dave_rc_account_after = db->get< rc_account_object, by_name >("dave");
    const rc_account_object& eve_rc_account_after = db->get< rc_account_object, by_name >("eve");

    BOOST_REQUIRE( alice_rc_after.delegated_rc == 0 );
    BOOST_REQUIRE( alice_rc_after.received_delegated_rc == 0 );
    BOOST_REQUIRE( alice_rc_after.last_max_rc == creation_rc ); // Still creation_rc because the vesting delegation didn't come back

    BOOST_REQUIRE( bob_rc_account_after.delegated_rc == 0 );
    BOOST_REQUIRE( bob_rc_account_after.last_max_rc == creation_rc );
    BOOST_REQUIRE( bob_rc_account_after.received_delegated_rc == 0 );

    BOOST_REQUIRE( eve_rc_account_after.rc_manabar.current_mana == creation_rc );
    BOOST_REQUIRE( eve_rc_account_after.last_max_rc == creation_rc );
    BOOST_REQUIRE( eve_rc_account_after.received_delegated_rc == 0 );

    BOOST_REQUIRE( dave_rc_account_after.rc_manabar.current_mana == creation_rc );
    BOOST_REQUIRE( dave_rc_account_after.last_max_rc == creation_rc );
    BOOST_REQUIRE( dave_rc_account_after.received_delegated_rc == 0 );

    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( update_outdel_overflow_delegatee_performance )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: performance is taken into consideration, when `rc_direct_delegation_object` are removed" );
    generate_block();
    const int removal_limit = 200; //current mainnet value - if we increase it (we should, but it requires HF) we need to change it here as well
    db_plugin->debug_update( [=]( database& db )
    {
      db.modify( db.get_witness_schedule_object(), [&]( witness_schedule_object& wso )
      {
        wso.median_props.account_creation_fee = ASSET( "0.001 TESTS" ); //it effectively turns off minimum HP delegation limit
      });
      db.set_remove_threshold( removal_limit );
    });
    generate_block();

    ACTORS_DEFAULT_FEE( (alice)(bob)(carol) )
    generate_block();
    vest( HIVE_INIT_MINER_NAME, "alice", ASSET( "2000.000 TESTS" ) );
    generate_block();

    const uint32_t nr_accounts = removal_limit * 4;

    const account_object& alice_account_initial = db->get_account( "alice" );

    std::vector< performance::initial_data > accounts = performance::generate_accounts( this, nr_accounts );

    auto time_checker = []( std::function<void()> callable, uint32_t value )
    {
      auto _start = std::chrono::high_resolution_clock::now();
      callable();
      auto _end = std::chrono::high_resolution_clock::now();
      double elapsed_time_ms = std::chrono::duration<double, std::milli>( _end - _start ).count();
      BOOST_TEST_MESSAGE( "time: " + std::to_string( elapsed_time_ms ) );
#ifdef NDEBUG
      BOOST_REQUIRE_GT( value, elapsed_time_ms );
#endif
    };

    // We delegate all our vesting shares to bob and carol
    delegate_vesting_shares_operation dvso;
    dvso.vesting_shares = alice_account_initial.get_vesting();
    dvso.vesting_shares.amount.value /= 2;
    dvso.delegator = "alice";
    dvso.delegatee = "bob";
    push_transaction( dvso, alice_private_key );
    dvso.delegatee = "carol";
    push_transaction( dvso, alice_private_key );
    generate_block();

    delegate_rc_operation op;
    op.max_rc = 5;
    custom_json_operation custom_op;
    custom_op.id = HIVE_RC_PLUGIN_NAME;

    size_t cnt = 0;
    while( cnt < accounts.size() )
    {
      op.delegatees.clear();
      for( size_t i = cnt; i < cnt + HIVE_RC_MAX_ACCOUNTS_PER_DELEGATION_OP; ++i )
        op.delegatees.insert( accounts[i].account );

      cnt += op.delegatees.size();

      BOOST_TEST_MESSAGE( "create delegations" );

      custom_op.required_posting_auths.clear();
      custom_op.required_posting_auths.insert( "bob" );
      op.from = "bob";
      custom_op.json = fc::json::to_string( rc_plugin_operation( op ) );
      push_transaction( custom_op, bob_private_key );

      custom_op.required_posting_auths.clear();
      custom_op.required_posting_auths.insert( "carol" );
      op.from = "carol";
      custom_op.json = fc::json::to_string( rc_plugin_operation( op ) );
      push_transaction( custom_op, carol_private_key );

      generate_block();
    }

    for( auto& account : accounts )
    {
      const rc_account_object& _account = db->get< rc_account_object, by_name >( account.account );
      BOOST_REQUIRE( _account.received_delegated_rc == 10 );
    }

    BOOST_TEST_MESSAGE( "remove delegations" );
    dvso.vesting_shares = asset( 0, VESTS_SYMBOL );
    dvso.delegatee = "bob";
    auto push_10 = [this, &dvso, &alice_private_key] { push_transaction( dvso, alice_private_key ); };
    time_checker( push_10, 100 ); // <- removes `removal_limit` delegations from bob, rest (3 limits) postponed
    // leave a bit at carol - we will test supplementation of delegation removal object
    dvso.vesting_shares = asset( op.max_rc * removal_limit * 2, VESTS_SYMBOL );
    dvso.delegatee = "carol";
    auto push_11 = [this, &dvso, &alice_private_key] { push_transaction( dvso, alice_private_key ); };
    time_checker( push_11, 100 ); // <- removes `removal_limit` delegations from carol, rest (1 limit) postponed

    const auto& rc_del_idx = db->get_index<rc_direct_delegation_index, by_id>();
    BOOST_REQUIRE_EQUAL( rc_del_idx.size(), 2 * nr_accounts - 2 * removal_limit );

    int i = 0;
    for( auto& account : accounts )
    {
      const rc_account_object& _account = db->get< rc_account_object, by_name >( account.account );
      BOOST_REQUIRE( _account.received_delegated_rc == ( i >= removal_limit ? 10 : 0 ) );
      ++i;
    }
    generate_block();
    // since automatic expired delegation removal happens on end of block, now there should be twice as many removed,
    // however only the delegations from bob are processed this way (limit on automatic removal is common for all delegations)
    BOOST_REQUIRE_EQUAL( rc_del_idx.size(), 2 * nr_accounts - 3 * removal_limit );

    i = 0;
    for( auto& account : accounts )
    {
      const rc_account_object& _account = db->get< rc_account_object, by_name >( account.account );
      BOOST_REQUIRE( _account.received_delegated_rc == ( i >= removal_limit * 2 ? 10 : i >= removal_limit ? 5 : 0 ) );
      ++i;
    }

    // not possible to create new delegation while previous are still being removed
    op.delegatees.clear();
    op.delegatees.insert( accounts[0].account );
    custom_op.required_posting_auths.clear();
    custom_op.required_posting_auths.insert( "bob" );
    op.from = "bob";
    custom_op.json = fc::json::to_string( rc_plugin_operation( op ) );
    HIVE_REQUIRE_ASSERT( push_transaction( custom_op, bob_private_key ), "!_db.is_in_control() || !has_expired_delegation( _db, from_account )" );
    custom_op.required_posting_auths.clear();
    custom_op.required_posting_auths.insert( "carol" );
    op.from = "carol";
    custom_op.json = fc::json::to_string( rc_plugin_operation( op ) );
    HIVE_REQUIRE_ASSERT( push_transaction( custom_op, carol_private_key ), "!_db.is_in_control() || !has_expired_delegation( _db, from_account )" );

    // but we can test that dedelegation while previous one did not end just adds to the object
    // (note that execution of dedelegation will in itself stop at removal limit and only excess
    // will be added to the expired delegation object)
    dvso.vesting_shares = asset( 0, VESTS_SYMBOL );
    dvso.delegatee = "carol";
    auto push_12 = [this, &dvso, &alice_private_key] { push_transaction( dvso, alice_private_key ); };
    time_checker( push_12, 100 ); // <- removes `removal_limit` delegations from carol, rest (1 limit) added postponed
    BOOST_REQUIRE_EQUAL( rc_del_idx.size(), 2 * nr_accounts - 4 * removal_limit );

    vest( HIVE_INIT_MINER_NAME, "bob", ASSET( "1000.000 TESTS" ) );
    vest( HIVE_INIT_MINER_NAME, "carol", ASSET( "1000.000 TESTS" ) );
    // even though delegators have new vests that could cover not-yet-removed delegations
    // we still continue to remove them (we could make it differently but that complicates
    // implementation - the mechanism is an safety trigger, so it should be simple)
    custom_op.required_posting_auths.clear();
    custom_op.required_posting_auths.insert( "bob" );
    op.from = "bob";
    custom_op.json = fc::json::to_string( rc_plugin_operation( op ) );
    HIVE_REQUIRE_ASSERT( push_transaction( custom_op, bob_private_key ), "!_db.is_in_control() || !has_expired_delegation( _db, from_account )" );
    custom_op.required_posting_auths.clear();
    custom_op.required_posting_auths.insert( "carol" );
    op.from = "carol";
    custom_op.json = fc::json::to_string( rc_plugin_operation( op ) );
    HIVE_REQUIRE_ASSERT( push_transaction( custom_op, carol_private_key ), "!_db.is_in_control() || !has_expired_delegation( _db, from_account )" );

    generate_block();
    BOOST_REQUIRE_EQUAL( rc_del_idx.size(), 2 * nr_accounts - 5 * removal_limit );
    generate_block();

    BOOST_REQUIRE_EQUAL( rc_del_idx.size(), 2 * nr_accounts - 6 * removal_limit );
    // bob is free to delegate again, since he was first in queue so his delegations were handled first
    custom_op.required_posting_auths.clear();
    custom_op.required_posting_auths.insert( "bob" );
    op.from = "bob";
    custom_op.json = fc::json::to_string( rc_plugin_operation( op ) );
    push_transaction( custom_op, bob_private_key );
    // carol is still 2 blocks behind with delegation removal
    custom_op.required_posting_auths.clear();
    custom_op.required_posting_auths.insert( "carol" );
    op.from = "carol";
    custom_op.json = fc::json::to_string( rc_plugin_operation( op ) );
    HIVE_REQUIRE_ASSERT( push_transaction( custom_op, carol_private_key ), "!_db.is_in_control() || !has_expired_delegation( _db, from_account )" );

    generate_block();
    BOOST_REQUIRE_EQUAL( rc_del_idx.size(), 2 * nr_accounts - 7 * removal_limit + 1 );
    generate_block();
    BOOST_REQUIRE_EQUAL( rc_del_idx.size(), 1 );
    push_transaction( custom_op, carol_private_key );

    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_SUITE_END()

#endif
