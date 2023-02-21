#ifdef IS_TEST_NET

#include <boost/test/unit_test.hpp>

#include <hive/chain/hive_objects.hpp>

#include "../db_fixture/database_fixture.hpp"

using namespace hive::chain;
using namespace hive::chain::util;

BOOST_FIXTURE_TEST_SUITE( hf28_tests, cluster_database_fixture )

BOOST_AUTO_TEST_CASE( declined_voting_rights_basic )
{
  try
  {
    bool is_hf28 = false;

    auto _content = [&is_hf28]( ptr_hardfork_database_fixture& executor )
    {
        BOOST_TEST_MESSAGE( "Testing: 'declined_voting_rights' virtual operation" );
        BOOST_REQUIRE_EQUAL( (bool)executor, true );

        ACTORS_EXT( (*executor), (alice)(bob) );
        executor->fund( "alice", 1000 );
        executor->fund( "bob", 1000 );
        executor->vest( "alice", 1000 );
        executor->vest( "bob", 1000 );

        BOOST_TEST_MESSAGE( "--- Generating 'declined_voting_rights' virtual operation" );

        account_witness_proxy_operation op;
        op.account = "alice";
        op.proxy = "bob";

        decline_voting_rights_operation op2;
        op2.account = "alice";
        op2.decline = true;

        signed_transaction tx;
        tx.operations.push_back( op );
        tx.operations.push_back( op2 );
        tx.set_expiration( executor->db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );

        executor->push_transaction( tx, alice_private_key );

        const auto& request_idx = executor->db->get_index< decline_voting_rights_request_index >().indices().get< by_account >();

        auto itr = request_idx.find( executor->db->get_account( "alice" ).name );
        BOOST_REQUIRE( itr != request_idx.end() );
        BOOST_REQUIRE( itr->effective_date == executor->db->head_block_time() + HIVE_OWNER_AUTH_RECOVERY_PERIOD );

        executor->generate_block();

        executor->generate_blocks( executor->db->head_block_time() + HIVE_OWNER_AUTH_RECOVERY_PERIOD - fc::seconds( HIVE_BLOCK_INTERVAL ) - fc::seconds( HIVE_BLOCK_INTERVAL ), false );

        itr = request_idx.find( executor->db->get_account( "alice" ).name );
        BOOST_REQUIRE( itr != request_idx.end() );

        executor->generate_blocks( executor->db->head_block_time() + fc::seconds( HIVE_BLOCK_INTERVAL ) );

        itr = request_idx.find( executor->db->get_account( "alice" ).name );
        BOOST_REQUIRE( itr == request_idx.end() );

        auto recent_ops = executor->get_last_operations( 1 );

        /*
            It doesn't matter which HF is - since vop `declined_voting_rights` was introduced, it should be always generated.
            Before HF28, a last vop was `proxy_cleared_operation`.
        */
        auto recent_op = recent_ops.back().get< declined_voting_rights >();
        BOOST_REQUIRE( recent_op.account == "alice" );
    };

    BOOST_TEST_MESSAGE( "*****HF-27*****" );
    execute_hardfork<27>( _content );

    is_hf28 = true;

    BOOST_TEST_MESSAGE( "*****HF-28*****" );
    execute_hardfork<28>( _content );
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( declined_voting_rights_basic_2 )
{
  try
  {
    bool is_hf28 = false;

    auto _content = [ &is_hf28 ]( ptr_hardfork_database_fixture& executor )
    {
        BOOST_TEST_MESSAGE( "Testing: 'declined_voting_rights' is created twice" );
        BOOST_REQUIRE_EQUAL( (bool)executor, true );

        ACTORS_EXT( (*executor), (alice) );
        executor->fund( "alice", 1000 );
        executor->vest( "alice", 1000 );

        auto _create_decline_voting_rights_operation = []( ptr_hardfork_database_fixture& executor, const fc::ecc::private_key& key )
        {
          decline_voting_rights_operation op;
          op.account = "alice";
          op.decline = true;

          BOOST_TEST_MESSAGE( "Create 'decline_voting_rights'" );
          signed_transaction tx;
          tx.operations.push_back( op );
          tx.set_expiration( executor->db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );

          executor->push_transaction( tx, key );
        };

        _create_decline_voting_rights_operation( executor, alice_private_key );

        executor->generate_blocks( executor->db->head_block_time() + HIVE_OWNER_AUTH_RECOVERY_PERIOD, false );

        if( is_hf28 )
        {
          HIVE_REQUIRE_THROW( _create_decline_voting_rights_operation( executor, alice_private_key ), fc::assert_exception );
        }
        else
        {
          _create_decline_voting_rights_operation( executor, alice_private_key );
        }
    };

    BOOST_TEST_MESSAGE( "*****HF-27*****" );
    execute_hardfork<27>( _content );

    is_hf28 = true;

    BOOST_TEST_MESSAGE( "*****HF-28*****" );
    execute_hardfork<28>( _content );
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( declined_voting_rights_proposal_votes )
{
  try
  {
    bool is_hf28 = false;

    auto _content = [ &is_hf28 ]( ptr_hardfork_database_fixture& executor )
    {
      BOOST_TEST_MESSAGE( "Testing: create 'decline_voting_rights' before an user casts votes on proposal" );
      BOOST_REQUIRE_EQUAL( (bool)executor, true );

      ACTORS_EXT( (*executor), (alice)(bob) );

      executor->generate_block();

      executor->fund( "alice", 100000 );
      executor->fund( "bob", 100000 );
      executor->fund( "alice", ASSET( "200.000 TBD" ) );
      executor->fund( "bob", ASSET( "200.000 TBD" ) );
      executor->vest( "alice", 100000 );
      executor->vest( "bob", 100000 );

      executor->generate_block();

      BOOST_TEST_MESSAGE( "Create 'decline_voting_rights'" );
      decline_voting_rights_operation op;
      op.account = "alice";
      op.decline = true;

      signed_transaction tx;
      tx.operations.push_back( op );
      tx.set_expiration( executor->db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );

      executor->push_transaction( tx, alice_private_key );

      executor->generate_block();

      dhf_database dhf_db( *executor.get() );
      dhf_database::create_proposal_data cpd( executor->db->head_block_time() );

      BOOST_TEST_MESSAGE( "Create 'create_proposal_operation'" );
      int64_t _id_proposal = dhf_db.create_proposal( cpd.creator, cpd.receiver, cpd.start_date, cpd.end_date, cpd.daily_pay, alice_private_key, false/*with_block_generation*/ );

      BOOST_TEST_MESSAGE( "Create 'update_proposal_votes_operation'" );
      dhf_db.vote_proposal( "alice", { _id_proposal }, true, alice_private_key );
      BOOST_REQUIRE( dhf_db.find_vote_for_proposal( "alice", _id_proposal ) == true );

      executor->generate_block();

      executor->generate_blocks( executor->db->head_block_time() + HIVE_OWNER_AUTH_RECOVERY_PERIOD, false );

      if( is_hf28 )
        BOOST_REQUIRE( dhf_db.find_vote_for_proposal( "alice", _id_proposal ) == false );//Proposal votes should be removed.
      else
        BOOST_REQUIRE( dhf_db.find_vote_for_proposal( "alice", _id_proposal ) == true );
    };

    BOOST_TEST_MESSAGE( "*****HF-27*****" );
    execute_hardfork<27>( _content );

    is_hf28 = true;

    BOOST_TEST_MESSAGE( "*****HF-28*****" );
    execute_hardfork<28>( _content );
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( declined_voting_rights_proposal_votes_2 )
{
  try
  {
    bool is_hf28 = false;

    auto _content = [ &is_hf28 ]( ptr_hardfork_database_fixture& executor )
    {
      BOOST_TEST_MESSAGE( "Testing: create 'decline_voting_rights'. When 'declined_voting_rights' is generated, an user tries to cast a vote on proposal" );
      BOOST_REQUIRE_EQUAL( (bool)executor, true );

      ACTORS_EXT( (*executor), (alice)(bob) );

      executor->generate_block();

      executor->fund( "alice", 100000 );
      executor->fund( "bob", 100000 );
      executor->fund( "alice", ASSET( "200.000 TBD" ) );
      executor->fund( "bob", ASSET( "200.000 TBD" ) );
      executor->vest( "alice", 100000 );
      executor->vest( "bob", 100000 );

      executor->generate_block();

      BOOST_TEST_MESSAGE( "Create 'decline_voting_rights'" );
      decline_voting_rights_operation op;
      op.account = "alice";
      op.decline = true;

      signed_transaction tx;
      tx.operations.push_back( op );
      tx.set_expiration( executor->db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );

      executor->push_transaction( tx, alice_private_key );

      executor->generate_block();

      executor->generate_blocks( executor->db->head_block_time() + HIVE_OWNER_AUTH_RECOVERY_PERIOD, false );

      dhf_database dhf_db( *executor.get() );
      dhf_database::create_proposal_data cpd( executor->db->head_block_time() );

      BOOST_TEST_MESSAGE( "Create 'create_proposal_operation'" );
      int64_t _id_proposal = dhf_db.create_proposal( cpd.creator, cpd.receiver, cpd.start_date, cpd.end_date, cpd.daily_pay, alice_private_key, false/*with_block_generation*/ );

      BOOST_TEST_MESSAGE( "Create 'update_proposal_votes_operation'" );
      if( is_hf28 )
      {
        HIVE_REQUIRE_THROW( dhf_db.vote_proposal( "alice", { _id_proposal }, true, alice_private_key ), fc::assert_exception );
      }
      else
      {
        dhf_db.vote_proposal( "alice", { _id_proposal }, true, alice_private_key );
        BOOST_REQUIRE( dhf_db.find_vote_for_proposal( "alice", _id_proposal ) == true );
      }
    };

    BOOST_TEST_MESSAGE( "*****HF-27*****" );
    execute_hardfork<27>( _content );

    is_hf28 = true;

    BOOST_TEST_MESSAGE( "*****HF-28*****" );
    execute_hardfork<28>( _content );
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_SUITE_END()

#endif
