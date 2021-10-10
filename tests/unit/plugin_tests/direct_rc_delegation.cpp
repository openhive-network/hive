#ifdef IS_TEST_NET
#include <boost/test/unit_test.hpp>

#include <hive/chain/account_object.hpp>
#include <hive/chain/comment_object.hpp>
#include <hive/chain/database_exceptions.hpp>
#include <hive/chain/smt_objects.hpp>
#include <hive/protocol/hive_operations.hpp>

#include <hive/plugins/rc/rc_objects.hpp>
#include <hive/plugins/rc/rc_operations.hpp>
#include <hive/plugins/rc/rc_plugin.hpp>

#include "../db_fixture/database_fixture.hpp"

using namespace hive::chain;
using namespace hive::protocol;
using namespace hive::plugins::rc;

BOOST_FIXTURE_TEST_SUITE( rc_direct_delegation, clean_database_fixture )

BOOST_AUTO_TEST_CASE( delegate_rc_operation_validate )
{
  try{
    delegate_rc_operation op;
    op.from = "alice";
    op.to = "bob";
    op.max_rc = 10;
    op.validate();

    op.from = "alice-";
    BOOST_REQUIRE_THROW( op.validate(), fc::assert_exception );
    op.from = "alice";

    op.to = "bob-";
    BOOST_REQUIRE_THROW( op.validate(), fc::assert_exception );
    op.to = "bob";

    op.to = "alice";
    BOOST_REQUIRE_THROW( op.validate(), fc::assert_exception );
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( delegate_rc_operation_apply )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing:  delegate_rc_operation_apply" );
    ACTORS( (alice)(bob)(dave) )
    vest( HIVE_INIT_MINER_NAME, "alice", ASSET( "10.000 TESTS" ) );
    int64_t alice_vests = alice.vesting_shares.amount.value;

    // Delegating more rc than I have should fail
    delegate_rc_operation op;
    op.from = "alice";
    op.to = "bob";
    op.max_rc = alice_vests + 1;

    custom_json_operation custom_op;
    custom_op.required_posting_auths.insert( "alice" );
    custom_op.id = HIVE_RC_PLUGIN_NAME;
    custom_op.json = fc::json::to_string( rc_plugin_operation( op ) );
    BOOST_CHECK_THROW( push_transaction(custom_op, alice_private_key);, fc::exception );

    // Delegating to a non-existing account should fail
    op.to = "eve";
    op.max_rc = 10;
    custom_op.json = fc::json::to_string( rc_plugin_operation( op ) );
    BOOST_CHECK_THROW( push_transaction(custom_op, alice_private_key);, fc::exception );

    // Delegating 0 shouldn't work if there isn't already a delegation that exists (since 0 deletes the delegation)
    op.to = "bob";
    op.max_rc = 0;
    custom_op.json = fc::json::to_string( rc_plugin_operation( op ) );
    BOOST_CHECK_THROW( push_transaction(custom_op, alice_private_key);, fc::exception );

    // Delegating 0 shouldn't work if there isn't already a delegation that exists (since 0 deletes the delegation)
    op.to = "bob";
    op.max_rc = 0;
    custom_op.json = fc::json::to_string( rc_plugin_operation( op ) );
    BOOST_CHECK_THROW( push_transaction(custom_op, alice_private_key);, fc::exception );

    // Successful delegation
    op.to = "bob";
    op.max_rc = 10;
    custom_op.json = fc::json::to_string( rc_plugin_operation( op ) );
    push_transaction(custom_op, alice_private_key);

    generate_block();

    const rc_direct_delegation_object* delegation = db->find< rc_direct_delegation_object, by_from_to >( boost::make_tuple( alice_id, bob_id ) );
    BOOST_REQUIRE( delegation->from == alice_id );
    BOOST_REQUIRE( delegation->to == bob_id );
    BOOST_REQUIRE( delegation->delegated_rc == op.max_rc );

    const rc_account_object& from_rc_account = db->get< rc_account_object, by_name >( op.from );
    const rc_account_object& to_rc_account = db->get< rc_account_object, by_name >( op.to );

    BOOST_REQUIRE( from_rc_account.delegated_rc == 10 );
    BOOST_REQUIRE( from_rc_account.received_delegated_rc == 0 );
    BOOST_REQUIRE( to_rc_account.delegated_rc == 0 );
    BOOST_REQUIRE( to_rc_account.received_delegated_rc == 10 );


    // Delegating the same amount shouldn't work
    custom_op.json = fc::json::to_string( rc_plugin_operation( op ) );
    BOOST_CHECK_THROW( push_transaction(custom_op, alice_private_key);, fc::exception );
    
    // Decrease the delegation
    op.from = "alice";
    op.to = "bob";
    op.max_rc = 5;
    custom_op.required_posting_auths.clear();
    custom_op.required_posting_auths.insert( "alice" );
    custom_op.json = fc::json::to_string( rc_plugin_operation( op ) );
    push_transaction(custom_op, alice_private_key);

    generate_block();

    const rc_direct_delegation_object* delegation_decreased = db->find< rc_direct_delegation_object, by_from_to >( boost::make_tuple( alice_id, bob_id ) );
    BOOST_REQUIRE( delegation_decreased->from == alice_id );
    BOOST_REQUIRE( delegation_decreased->to == bob_id );
    BOOST_REQUIRE( delegation_decreased->delegated_rc == op.max_rc );

    const rc_account_object& from_rc_account_decreased = db->get< rc_account_object, by_name >( op.from );
    const rc_account_object& to_rc_account_decreased = db->get< rc_account_object, by_name >( op.to );

    BOOST_REQUIRE( from_rc_account_decreased.delegated_rc == 5 );
    BOOST_REQUIRE( from_rc_account_decreased.received_delegated_rc == 0 );
    BOOST_REQUIRE( to_rc_account_decreased.delegated_rc == 0 );
    BOOST_REQUIRE( to_rc_account_decreased.received_delegated_rc == 5 );

    // Increase the delegation
    op.from = "alice";
    op.to = "bob";
    op.max_rc = 50;
    custom_op.json = fc::json::to_string( rc_plugin_operation( op ) );
    push_transaction(custom_op, alice_private_key);

    generate_block();

    const rc_direct_delegation_object* delegation_increased = db->find< rc_direct_delegation_object, by_from_to >( boost::make_tuple( alice_id, bob_id ) );
    BOOST_REQUIRE( delegation_increased->from == alice_id );
    BOOST_REQUIRE( delegation_increased->to == bob_id );
    BOOST_REQUIRE( delegation_increased->delegated_rc == op.max_rc );

    const rc_account_object& from_rc_account_increased = db->get< rc_account_object, by_name >( op.from );
    const rc_account_object& to_rc_account_increased = db->get< rc_account_object, by_name >( op.to );

    BOOST_REQUIRE( from_rc_account_increased.delegated_rc == 50 );
    BOOST_REQUIRE( from_rc_account_increased.received_delegated_rc == 0 );
    BOOST_REQUIRE( to_rc_account_increased.delegated_rc == 0 );
    BOOST_REQUIRE( to_rc_account_increased.received_delegated_rc == 50 );

    // Delete the delegation
    op.from = "alice";
    op.to = "bob";
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
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( update_outdel_overflow )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing:  update_outdel_overflow" );
    ACTORS( (alice)(bob)(dave)(eve)(martin) )
    generate_block();

    // We are forced to do this because vests and rc values are bugged when executing tests
    db_plugin->debug_update( [=]( database& db )
    {
      db.modify( db.get_account( "alice" ), [&]( account_object& a )
      {
        a.vesting_shares = asset( 90, VESTS_SYMBOL );
      });

      db.modify( db.get_account( "bob" ), [&]( account_object& a )
      {
        a.vesting_shares = asset( 0, VESTS_SYMBOL );
      });

      db.modify( db.get_account( "dave" ), [&]( account_object& a )
      {
        a.vesting_shares = asset( 0, VESTS_SYMBOL );
      });

      db.modify( db.get< rc_account_object, by_name >( "alice" ), [&]( rc_account_object& rca )
      {
        rca.max_rc_creation_adjustment.amount.value = 10;
        rca.rc_manabar.current_mana = 100;
        rca.rc_manabar.last_update_time = db.head_block_time().sec_since_epoch();
        rca.last_max_rc = 100;
      });

      db.modify( db.get< rc_account_object, by_name >( "bob" ), [&]( rc_account_object& rca )
      {
        rca.max_rc_creation_adjustment.amount.value = 10;
        rca.rc_manabar.current_mana = 10;
        rca.rc_manabar.last_update_time = db.head_block_time().sec_since_epoch();
        rca.max_rc_creation_adjustment.amount.value = 10;
        rca.last_max_rc = 10;
      });
      db.modify( db.get< rc_account_object, by_name >( "dave" ), [&]( rc_account_object& rca )
      {
        rca.max_rc_creation_adjustment.amount.value = 10;
        rca.rc_manabar.current_mana = 10;
        rca.rc_manabar.last_update_time = db.head_block_time().sec_since_epoch();
        rca.max_rc_creation_adjustment.amount.value = 10;
        rca.last_max_rc = 10;
      });
    });


    // Delegate 10 rc to bob, 80 to dave, alice has 10 remaining rc
    delegate_rc_operation op;
    op.from = "alice";
    op.to = "bob";
    op.max_rc = 10;
    custom_json_operation custom_op;
    custom_op.required_posting_auths.insert( "alice" );
    custom_op.id = HIVE_RC_PLUGIN_NAME;
    custom_op.json = fc::json::to_string( rc_plugin_operation( op ) );
    push_transaction(custom_op, alice_private_key);

    op.to = "dave";
    op.max_rc = 80;
    custom_op.json = fc::json::to_string( rc_plugin_operation( op ) );
    push_transaction(custom_op, alice_private_key);

    generate_block();

    const rc_account_object& bob_rc_account_before = db->get< rc_account_object, by_name >("bob");
    const rc_account_object& dave_rc_account_before = db->get< rc_account_object, by_name >("dave");
    const rc_account_object& alice_rc_before = db->get< rc_account_object, by_name >( "alice" );

    BOOST_REQUIRE( alice_rc_before.delegated_rc == 90 );
    BOOST_REQUIRE( alice_rc_before.received_delegated_rc == 0 );
    BOOST_REQUIRE( alice_rc_before.rc_manabar.current_mana == 10 );
    BOOST_REQUIRE( alice_rc_before.last_max_rc == 10 );

    BOOST_REQUIRE( bob_rc_account_before.rc_manabar.current_mana == 20 );
    BOOST_REQUIRE( bob_rc_account_before.last_max_rc == 20 );
    BOOST_REQUIRE( bob_rc_account_before.received_delegated_rc == 10 );

    BOOST_REQUIRE( dave_rc_account_before.rc_manabar.current_mana == 90 );
    BOOST_REQUIRE( dave_rc_account_before.last_max_rc == 90 );
    BOOST_REQUIRE( dave_rc_account_before.received_delegated_rc == 80 );

    // we delegate and it affects one delegation
    // Delegate 5 vests out, alice has 5 remaining rc, it's lower than the max_rc_creation_adjustment which is 10
    // So the first delegation (to bob) is lowered to 5
    delegate_vesting_shares_operation dvso;
    dvso.vesting_shares = ASSET( "0.000005 VESTS");
    dvso.delegator = "alice";
    dvso.delegatee = "eve";
    push_transaction(dvso, alice_private_key);

    const rc_account_object& bob_rc_account_after = db->get< rc_account_object, by_name >("bob");
    const rc_account_object& dave_rc_account_after = db->get< rc_account_object, by_name >("dave");
    const rc_account_object& alice_rc_after = db->get< rc_account_object, by_name >( "alice" );

    BOOST_REQUIRE( alice_rc_after.delegated_rc == 85 );
    BOOST_REQUIRE( alice_rc_after.received_delegated_rc == 0 );
    BOOST_REQUIRE( alice_rc_after.rc_manabar.current_mana == 10 );
    BOOST_REQUIRE( alice_rc_after.last_max_rc == 10 );

    BOOST_REQUIRE( bob_rc_account_after.rc_manabar.current_mana == 15 );
    BOOST_REQUIRE( bob_rc_account_after.last_max_rc == 15 );
    BOOST_REQUIRE( bob_rc_account_after.received_delegated_rc == 5 );

    BOOST_REQUIRE( dave_rc_account_after.rc_manabar.current_mana == 90 );
    BOOST_REQUIRE( dave_rc_account_after.last_max_rc == 90 );
    BOOST_REQUIRE( dave_rc_account_after.received_delegated_rc == 80 );
    
    // We delegate and we don't have enough rc to sustain bob's delegation
    dvso.vesting_shares = ASSET( "0.000045 VESTS");
    dvso.delegator = "alice";
    dvso.delegatee = "martin";
    push_transaction(dvso, alice_private_key);

    const rc_account_object& bob_rc_account_after_two = db->get< rc_account_object, by_name >("bob");
    const rc_account_object& dave_rc_account_after_two = db->get< rc_account_object, by_name >("dave");
    const rc_account_object& alice_rc_after_two = db->get< rc_account_object, by_name >( "alice" );

   
    BOOST_REQUIRE( bob_rc_account_after_two.rc_manabar.current_mana == 10 );
    BOOST_REQUIRE( bob_rc_account_after_two.last_max_rc == 10 );
    BOOST_REQUIRE( bob_rc_account_after_two.received_delegated_rc == 0 );

    BOOST_REQUIRE( alice_rc_after_two.rc_manabar.current_mana == 10 );
    BOOST_REQUIRE( alice_rc_after_two.delegated_rc == 40 );
    BOOST_REQUIRE( alice_rc_after_two.received_delegated_rc == 0 );
    BOOST_REQUIRE( alice_rc_after_two.last_max_rc == 10 );

    BOOST_REQUIRE( dave_rc_account_after_two.rc_manabar.current_mana == 50 );
    BOOST_REQUIRE( dave_rc_account_after_two.last_max_rc == 50 );
    BOOST_REQUIRE( dave_rc_account_after_two.received_delegated_rc == 40 );
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( update_outdel_overflow_many_accounts )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing:  update_outdel_overflow with many actors" );
    #define NUM_ACTORS 250
    #define CREATE_ACTORS(z, n, text) ACTORS( (actor ## n) );
    BOOST_PP_REPEAT(NUM_ACTORS, CREATE_ACTORS, )
    ACTORS( (alice)(bob) )
    generate_block();

    // We are forced to do this because vests and rc values are bugged when executing tests
    db_plugin->debug_update( [=]( database& db )
    {
      db.modify( db.get_account( "alice" ), [&]( account_object& a )
      {
        a.vesting_shares = asset( NUM_ACTORS * 10, VESTS_SYMBOL );
      });

      db.modify( db.get< rc_account_object, by_name >( "alice" ), [&]( rc_account_object& rca )
      {
        rca.max_rc_creation_adjustment.amount.value = 10;
        rca.rc_manabar.current_mana = NUM_ACTORS * 10 + 10; // vests + max_rc_creation_adjustment
        rca.rc_manabar.last_update_time = db.head_block_time().sec_since_epoch();
        rca.last_max_rc = NUM_ACTORS * 10 + 10; // vests + max_rc_creation_adjustment
      });


      // Set the values for every actor
      for (int i = 0; i < NUM_ACTORS; i++) {
        db.modify( db.get_account( "actor" + std::to_string(i) ), [&]( account_object& a )
        {
          a.vesting_shares = asset( 0, VESTS_SYMBOL );
        });

        db.modify( db.get< rc_account_object, by_name >( "actor" + std::to_string(i) ), [&]( rc_account_object& rca )
        {
          rca.max_rc_creation_adjustment.amount.value = 10;
          rca.rc_manabar.current_mana = 10;
          rca.rc_manabar.last_update_time = db.head_block_time().sec_since_epoch();
          rca.max_rc_creation_adjustment.amount.value = 10;
          rca.last_max_rc = 10;
        });
      }
    });

    // Delegate 10 rc to every actor account
    for (int i = 0; i < NUM_ACTORS; i++) {
      delegate_rc_operation op;
      op.from = "alice";
      op.max_rc = 10;
      op.to = "actor" + std::to_string(i);
      custom_json_operation custom_op;
      custom_op.required_posting_auths.insert( "alice" );
      custom_op.id = HIVE_RC_PLUGIN_NAME;
      custom_op.json = fc::json::to_string( rc_plugin_operation( op ) );
      push_transaction(custom_op, alice_private_key);
      generate_block(); // TODO optimize this loop where we push the max allowed custom ops per block
    }

    const rc_account_object& actor0_rc_account_before = db->get< rc_account_object, by_name >("actor0");
    const rc_account_object& actor2_rc_account_before = db->get< rc_account_object, by_name >("actor2");
    const rc_account_object& alice_rc_before = db->get< rc_account_object, by_name >( "alice" );

    BOOST_REQUIRE( alice_rc_before.delegated_rc == NUM_ACTORS * 10 );
    BOOST_REQUIRE( alice_rc_before.received_delegated_rc == 0 );
    BOOST_REQUIRE( alice_rc_before.rc_manabar.current_mana == 10 );
    BOOST_REQUIRE( alice_rc_before.last_max_rc == 10 );

    BOOST_REQUIRE( actor0_rc_account_before.rc_manabar.current_mana == 20 );
    BOOST_REQUIRE( actor0_rc_account_before.last_max_rc == 20 );
    BOOST_REQUIRE( actor0_rc_account_before.received_delegated_rc == 10 );

    BOOST_REQUIRE( actor2_rc_account_before.rc_manabar.current_mana == 20 );
    BOOST_REQUIRE( actor2_rc_account_before.last_max_rc == 20 );
    BOOST_REQUIRE( actor2_rc_account_before.received_delegated_rc == 10 );

    // we delegate 25 vests and it affects the first three delegations
    // Delegate 25 vests out, alice has -15 remaining rc, it's lower than the max_rc_creation_adjustment which is 10
    // So the first two delegations (to actor0 and actor2) are deleted while the delegation to actor2 is deleted
    delegate_vesting_shares_operation dvso;
    dvso.vesting_shares = ASSET( "0.000025 VESTS");
    dvso.delegator = "alice";
    dvso.delegatee = "bob";
    push_transaction(dvso, alice_private_key);

    const rc_account_object& actor0_rc_account_after = db->get< rc_account_object, by_name >("actor0");
    const rc_account_object& actor2_rc_account_after = db->get< rc_account_object, by_name >("actor2");
    const rc_account_object& alice_rc_after = db->get< rc_account_object, by_name >( "alice" );

    BOOST_REQUIRE( alice_rc_after.delegated_rc == NUM_ACTORS * 10 - 25 ); // total amount minus what was delegated
    BOOST_REQUIRE( alice_rc_after.received_delegated_rc == 0 );
    BOOST_REQUIRE( alice_rc_after.rc_manabar.current_mana == 10 );
    BOOST_REQUIRE( alice_rc_after.last_max_rc == 10 );

    BOOST_REQUIRE( actor0_rc_account_after.rc_manabar.current_mana == 10 );
    BOOST_REQUIRE( actor0_rc_account_after.last_max_rc == 10 );
    BOOST_REQUIRE( actor0_rc_account_after.received_delegated_rc == 0 );

    BOOST_REQUIRE( actor2_rc_account_after.rc_manabar.current_mana == 15 );
    BOOST_REQUIRE( actor2_rc_account_after.last_max_rc == 15 );
    BOOST_REQUIRE( actor2_rc_account_after.received_delegated_rc == 5 );

    // We check that the rest of the delegations weren't affected
    for (int i = 4; i < NUM_ACTORS; i++) {
      const rc_account_object& actor_rc_account = db->get< rc_account_object, by_name >( "actor" + std::to_string(i) );
      BOOST_REQUIRE( actor_rc_account.rc_manabar.current_mana == 20 );
      BOOST_REQUIRE( actor_rc_account.last_max_rc == 20 );
      BOOST_REQUIRE( actor_rc_account.received_delegated_rc == 10 );
    }

    // We delegate all our vests and we don't have enough to sustain any of our remaining delegations
    const account_object& acct = db->get_account( "alice" );

    dvso.vesting_shares = acct.vesting_shares;
    dvso.delegator = "alice";
    dvso.delegatee = "bob";
    push_transaction(dvso, alice_private_key);

    const rc_account_object& alice_rc_end = db->get< rc_account_object, by_name >( "alice" );

    BOOST_REQUIRE( alice_rc_end.delegated_rc == 0 ); // total amount minus what was delegated
    BOOST_REQUIRE( alice_rc_end.received_delegated_rc == 0 );
    BOOST_REQUIRE( alice_rc_end.rc_manabar.current_mana == 10 );
    BOOST_REQUIRE( alice_rc_end.last_max_rc == 10 );

    // We check that every delegation got deleted
    for (int i = 0; i < NUM_ACTORS; i++) {
      const rc_account_object& actor_rc_account = db->get< rc_account_object, by_name >( "actor" + std::to_string(i) );
      BOOST_REQUIRE( actor_rc_account.rc_manabar.current_mana == 10 );
      BOOST_REQUIRE( actor_rc_account.last_max_rc == 10 );
      BOOST_REQUIRE( actor_rc_account.received_delegated_rc == 0 );
    }

    // Remove vests delegation and check that we got the rc back
    dvso.vesting_shares = ASSET( "0.000000 VESTS");;
    dvso.delegator = "alice";
    dvso.delegatee = "bob";
    push_transaction(dvso, alice_private_key);

    const rc_account_object& alice_rc_final = db->get< rc_account_object, by_name >( "alice" );

    BOOST_REQUIRE( alice_rc_final.delegated_rc == 0 ); // total amount minus what was delegated
    BOOST_REQUIRE( alice_rc_final.received_delegated_rc == 0 );
    BOOST_REQUIRE( alice_rc_final.rc_manabar.current_mana == 10 );
    BOOST_REQUIRE( alice_rc_final.last_max_rc == 10 );
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_SUITE_END()

#endif
