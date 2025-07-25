#ifdef IS_TEST_NET

#include <boost/test/unit_test.hpp>

#include <hive/chain/hive_objects.hpp>
#include <hive/chain/dhf_objects.hpp>
#include <hive/chain/account_object.hpp>
#include <hive/chain/database_exceptions.hpp>

#include <hive/protocol/transaction_util.hpp>

#include "../db_fixture/clean_database_fixture.hpp"

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
        BOOST_TEST_MESSAGE( "Testing: 'declined_voting_rights_operation'" );
        BOOST_REQUIRE_EQUAL( (bool)executor, true );

        ACTORS_EXT( (*executor), (alice)(bob) );
        executor->vest( "alice", ASSET( "1.000 TESTS" ) );
        executor->vest( "bob", ASSET( "1.000 TESTS" ) );

        BOOST_TEST_MESSAGE( "--- Generating 'declined_voting_rights_operation'" );

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

        auto itr = request_idx.find( "alice" );
        BOOST_REQUIRE( itr != request_idx.end() );
        BOOST_REQUIRE( itr->effective_date == executor->db->head_block_time() + HIVE_OWNER_AUTH_RECOVERY_PERIOD );

        executor->generate_block();

        executor->generate_blocks( executor->db->head_block_time() + HIVE_OWNER_AUTH_RECOVERY_PERIOD - fc::seconds( HIVE_BLOCK_INTERVAL ) - fc::seconds( HIVE_BLOCK_INTERVAL ), false );

        itr = request_idx.find( "alice" );
        BOOST_REQUIRE( itr != request_idx.end() );

        executor->generate_blocks( executor->db->head_block_time() + fc::seconds( HIVE_BLOCK_INTERVAL ) );

        itr = request_idx.find( "alice" );
        BOOST_REQUIRE( itr == request_idx.end() );

        auto recent_ops = executor->get_last_operations( 1 );

        /*
            It doesn't matter which HF is - since vop `declined_voting_rights_operation` was introduced, it should be always generated.
            Before HF28, a last vop was `proxy_cleared_operation`.
        */
        auto recent_op = recent_ops.back().get< declined_voting_rights_operation >();
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
        BOOST_TEST_MESSAGE( "Testing: 'declined_voting_rights_operation' is created twice" );
        BOOST_REQUIRE_EQUAL( (bool)executor, true );

        ACTORS_EXT( (*executor), (alice) );
        executor->vest( "alice", ASSET( "1.000 TESTS" ) );

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
          //An exception must be thrown because in HF28 is blocked by a condition `_db.is_in_control()` or `_db.has_hardfork( HIVE_HARDFORK_1_28 )`
          HIVE_REQUIRE_THROW( _create_decline_voting_rights_operation( executor, alice_private_key ), fc::assert_exception );
        }
        else
        {
          //An exception must be thrown because in HF27 is blocked by a condition `_db.is_in_control()`
          HIVE_REQUIRE_THROW( _create_decline_voting_rights_operation( executor, alice_private_key ), fc::assert_exception );
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

      executor->vest( "alice", ASSET( "100.000 TESTS" ) );
      executor->vest( "bob", ASSET( "100.000 TESTS" ) );
      executor->issue_funds( "alice", ASSET( "200.000 TBD" ) );
      executor->issue_funds( "bob", ASSET( "200.000 TBD" ) );

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

      dhf_database::create_proposal_data cpd( executor->db->head_block_time() );

      BOOST_TEST_MESSAGE( "Create 'create_proposal_operation'" );
      int64_t _id_proposal = executor->create_proposal( cpd.creator, cpd.receiver, cpd.start_date, cpd.end_date, cpd.daily_pay, alice_private_key, alice_post_key, false/*with_block_generation*/ );

      BOOST_TEST_MESSAGE( "Create 'update_proposal_votes_operation'" );
      executor->vote_proposal( "alice", { _id_proposal }, true, alice_private_key );
      BOOST_REQUIRE( executor->find_vote_for_proposal( "alice", _id_proposal ) != nullptr );

      executor->generate_block();

      executor->generate_blocks( executor->db->head_block_time() + HIVE_OWNER_AUTH_RECOVERY_PERIOD, false );

      if( is_hf28 )
        BOOST_REQUIRE( executor->find_vote_for_proposal( "alice", _id_proposal ) == nullptr );//Proposal votes should be removed.
      else
        BOOST_REQUIRE( executor->find_vote_for_proposal( "alice", _id_proposal ) != nullptr );
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
      BOOST_TEST_MESSAGE( "Testing: create 'decline_voting_rights'. When 'declined_voting_rights_operation' is generated, an user tries to cast a vote on proposal" );
      BOOST_REQUIRE_EQUAL( (bool)executor, true );

      ACTORS_EXT( (*executor), (alice)(bob) );

      executor->generate_block();

      executor->vest( "alice", ASSET( "100.000 TESTS" ) );
      executor->vest( "bob", ASSET( "100.000 TESTS" ) );
      executor->issue_funds( "alice", ASSET( "200.000 TBD" ) );
      executor->issue_funds( "bob", ASSET( "200.000 TBD" ) );

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

      dhf_database::create_proposal_data cpd( executor->db->head_block_time() );

      BOOST_TEST_MESSAGE( "Create 'create_proposal_operation'" );
      int64_t _id_proposal = executor->create_proposal( cpd.creator, cpd.receiver, cpd.start_date, cpd.end_date, cpd.daily_pay, alice_private_key, alice_post_key, false/*with_block_generation*/ );

      BOOST_TEST_MESSAGE( "Create 'update_proposal_votes_operation'" );
      if( is_hf28 )
      {
        //An exception must be thrown because in HF28 is blocked by a condition `_db.is_in_control()` or `_db.has_hardfork( HIVE_HARDFORK_1_28 )`
        HIVE_REQUIRE_THROW( executor->vote_proposal( "alice", { _id_proposal }, true, alice_private_key ), fc::assert_exception );
      }
      else
      {
        //An exception must be thrown because in HF27 is blocked by a condition `_db.is_in_control()`
        HIVE_REQUIRE_THROW( executor->vote_proposal( "alice", { _id_proposal }, true, alice_private_key ), fc::assert_exception );
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

struct request
{
  account_name_type   name;
  fc::time_point_sec  effective_date;

  bool operator==( const request& obj ) const
  {
    return name == obj.name && effective_date == obj.effective_date;
  }

  bool operator<( const request& obj ) const
  {
    return name < obj.name;
  }
};

bool check_decline_voting_rights_requests( database* db, const std::set<request>& requests )
{
  std::set<request> _requests;

  const auto& _request_idx = db->get_index< decline_voting_rights_request_index, by_account >();
  auto _itr = _request_idx.begin();

  while( _itr != _request_idx.end() )
  {
    _requests.insert( { _itr->account, _itr->effective_date } );
    ++_itr;
  }

  if( _requests.size() != requests.size() )
    return false;

  bool _result = std::equal( _requests.begin(), _requests.end(), requests.begin() );
  return _result;
}

BOOST_AUTO_TEST_CASE( declined_voting_rights_between_hf27_and_hf28 )
{
  try
  {
    auto _content = []( ptr_hardfork_database_fixture& executor, uint32_t level )
    {
      BOOST_TEST_MESSAGE( "Testing: when HF28 occurs then it's necessary to remove proposal votes for accounts that declined voting rights" );
      BOOST_REQUIRE_EQUAL( (bool)executor, true );

      auto _ht = executor->db->head_block_time();

      {
        if( level == 0 )
        {
          executor->db->set_hardfork( HIVE_HARDFORK_1_28 );
          BOOST_REQUIRE( check_decline_voting_rights_requests( executor->db, {} ) );

          return;
        }
      }
      {
        std::vector<account_name_type> _accounts{ "alice0", "alice1", "alice2", "alice3", "alice4", "alice5" };

        BOOST_TEST_MESSAGE("Create accounts.");
        for( auto& account : _accounts )
          executor->db->create< account_object >( account, _ht );
      }
      {
        if( level == 1 )
        {
          executor->db->set_hardfork( HIVE_HARDFORK_1_28 );
          BOOST_REQUIRE( check_decline_voting_rights_requests( executor->db, {} ) );
          BOOST_REQUIRE( !check_decline_voting_rights_requests( executor->db, { { "alice0", _ht } } ) );
          BOOST_REQUIRE( !check_decline_voting_rights_requests( executor->db, { { "alice1", _ht } } ) );
          BOOST_REQUIRE( !check_decline_voting_rights_requests( executor->db, { { "alice2", _ht } } ) );
          BOOST_REQUIRE( !check_decline_voting_rights_requests( executor->db, { { "alice3", _ht } } ) );
          BOOST_REQUIRE( !check_decline_voting_rights_requests( executor->db, { { "alice4", _ht } } ) );
          BOOST_REQUIRE( !check_decline_voting_rights_requests( executor->db, { { "alice5", _ht } } ) );

          return;
        }
      }
      {
        executor->db->create< proposal_vote_object >( [&]( proposal_vote_object& proposal_vote )
        {
          proposal_vote.voter = "alice0";
        } );
        executor->db->create< proposal_vote_object >( [&]( proposal_vote_object& proposal_vote )
        {
          proposal_vote.voter = "alice1";
        } );
        executor->db->create< proposal_vote_object >( [&]( proposal_vote_object& proposal_vote )
        {
          proposal_vote.voter = "alice3";
        } );
      }
      {
        if( level == 2 )
        {
          executor->db->set_hardfork( HIVE_HARDFORK_1_28 );
          BOOST_REQUIRE( check_decline_voting_rights_requests( executor->db, {} ) );
          BOOST_REQUIRE( !check_decline_voting_rights_requests( executor->db, { { "alice0", _ht } } ) );
          BOOST_REQUIRE( !check_decline_voting_rights_requests( executor->db, { { "alice1", _ht } } ) );
          BOOST_REQUIRE( !check_decline_voting_rights_requests( executor->db, { { "alice2", _ht } } ) );
          BOOST_REQUIRE( !check_decline_voting_rights_requests( executor->db, { { "alice3", _ht } } ) );
          BOOST_REQUIRE( !check_decline_voting_rights_requests( executor->db, { { "alice4", _ht } } ) );
          BOOST_REQUIRE( !check_decline_voting_rights_requests( executor->db, { { "alice5", _ht } } ) );

          return;
        }
      }
      {
        auto& _account = executor->db->get_account( "alice0" );
        executor->db->modify( _account, [&]( account_object& account )
        {
          account.can_vote = false;
        } );
      }
      {
        if( level == 3 )
        {
          executor->db->set_hardfork( HIVE_HARDFORK_1_28 );
          BOOST_REQUIRE( check_decline_voting_rights_requests( executor->db, { { "alice0", _ht } } ) );
          BOOST_REQUIRE( !check_decline_voting_rights_requests( executor->db, { { "alice0", _ht + 1 } } ) );
          BOOST_REQUIRE( !check_decline_voting_rights_requests( executor->db, { { "alice0", _ht }, { "alice1", _ht } } ) );
          BOOST_REQUIRE( !check_decline_voting_rights_requests( executor->db, { { "alice0", _ht }, { "alice2", _ht } } ) );
          BOOST_REQUIRE( !check_decline_voting_rights_requests( executor->db, { { "alice0", _ht }, { "alice3", _ht } } ) );
          BOOST_REQUIRE( !check_decline_voting_rights_requests( executor->db, { { "alice0", _ht }, { "alice4", _ht } } ) );
          BOOST_REQUIRE( !check_decline_voting_rights_requests( executor->db, { { "alice0", _ht }, { "alice5", _ht } } ) );

          return;
        }
      }
      {
        auto& _account = executor->db->get_account( "alice1" );
        executor->db->modify( _account, [&]( account_object& account )
        {
          account.can_vote = false;
        } );
      }
      {
        auto& _account = executor->db->get_account( "alice2" );
        executor->db->modify( _account, [&]( account_object& account )
        {
          account.can_vote = false;
        } );
      }
      {
        if( level == 4 )
        {
          executor->db->set_hardfork( HIVE_HARDFORK_1_28 );
          BOOST_REQUIRE( check_decline_voting_rights_requests( executor->db, { { "alice0", _ht }, { "alice1", _ht } } ) );
          BOOST_REQUIRE( !check_decline_voting_rights_requests( executor->db, { { "alice0", _ht + 1 }, { "alice1", _ht + 1 } } ) );
          BOOST_REQUIRE( !check_decline_voting_rights_requests( executor->db, { { "alice0", _ht }, { "alice1", _ht }, { "alice2", _ht } } ) );
          BOOST_REQUIRE( !check_decline_voting_rights_requests( executor->db, { { "alice0", _ht }, { "alice1", _ht }, { "alice3", _ht } } ) );
          BOOST_REQUIRE( !check_decline_voting_rights_requests( executor->db, { { "alice0", _ht }, { "alice1", _ht }, { "alice4", _ht } } ) );
          BOOST_REQUIRE( !check_decline_voting_rights_requests( executor->db, { { "alice0", _ht }, { "alice1", _ht }, { "alice5", _ht } } ) );

          return;
        }
      }
      {
        executor->db->create< proposal_vote_object >( [&]( proposal_vote_object& proposal_vote )
        {
          proposal_vote.voter = "alice2";
        } );
        executor->db->create< proposal_vote_object >( [&]( proposal_vote_object& proposal_vote )
        {
          proposal_vote.voter = "alice4";
        } );
        executor->db->create< proposal_vote_object >( [&]( proposal_vote_object& proposal_vote )
        {
          proposal_vote.voter = "alice5";
        } );
      }
      {
        if( level == 5 )
        {
          executor->db->set_hardfork( HIVE_HARDFORK_1_28 );
          BOOST_REQUIRE( check_decline_voting_rights_requests( executor->db, { { "alice2", _ht }, { "alice0", _ht }, { "alice1", _ht } } ) );
          BOOST_REQUIRE( !check_decline_voting_rights_requests( executor->db, { { "alice2", _ht }, { "alice0", _ht + 1 }, { "alice1", _ht + 1 } } ) );
          BOOST_REQUIRE( !check_decline_voting_rights_requests( executor->db, { { "alice0", _ht }, { "alice1", _ht }, { "alice2", _ht }, { "alice3", _ht } } ) );
          BOOST_REQUIRE( !check_decline_voting_rights_requests( executor->db, { { "alice0", _ht }, { "alice1", _ht }, { "alice2", _ht }, { "alice4", _ht } } ) );
          BOOST_REQUIRE( !check_decline_voting_rights_requests( executor->db, { { "alice0", _ht }, { "alice1", _ht }, { "alice2", _ht }, { "alice5", _ht } } ) );

          return;
        }
      }
      {
        auto& _account = executor->db->get_account( "alice4" );
        executor->db->modify( _account, [&]( account_object& account )
        {
          account.can_vote = false;
        } );
      }
      {
        auto& _account = executor->db->get_account( "alice5" );
        executor->db->modify( _account, [&]( account_object& account )
        {
          account.can_vote = false;
        } );
      }
      {
        if( level == 6 )
        {
          executor->db->set_hardfork( HIVE_HARDFORK_1_28 );
          BOOST_REQUIRE( check_decline_voting_rights_requests( executor->db, { { "alice2", _ht }, { "alice0", _ht }, { "alice1", _ht }, { "alice5", _ht }, { "alice4", _ht } } ) );
          BOOST_REQUIRE( !check_decline_voting_rights_requests( executor->db, { { "alice2", _ht }, { "alice0", _ht }, { "alice1", _ht + 1 }, { "alice5", _ht }, { "alice4", _ht } } ) );

          return;
        }
      }
    };

    BOOST_TEST_MESSAGE( "*****HF-27*****" );
    const uint32_t max_testing_levels = 7;
    for( uint32_t current_level = 0; current_level < max_testing_levels; ++current_level )
      execute_hardfork<27>( std::bind( _content, std::placeholders::_1, current_level ) );
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( declined_voting_rights_between_hf27_and_hf28_2 )
{
  try
  {
    /*
      Following test has stages:
      a) create some `update_proposal_votes_operation`
      b) create some `decline_voting_rights`
      c) change MANUALLY(!) `can_vote` in some `decline_voting_rights_request_object` objects
      c) call `db.remove_proposal_votes_for_accounts_without_voting_rights` by calling set_hardfork(28)
      d) wait for removing proposal votes in the same block
    */
    auto _content = []( ptr_hardfork_database_fixture& executor )
    {
      BOOST_TEST_MESSAGE( "Testing: It's necessary to remove proposal votes for accounts that declined voting rights" );
      BOOST_REQUIRE_EQUAL( (bool)executor, true );

      ACTORS_EXT( (*executor), (alice)(bob)(carol)(diana) );
      executor->generate_block();

      struct account_data
      {
        std::string           name;
        fc::ecc::private_key  key;
      };
      std::vector<account_data> _actors = { { "alice", alice_private_key }, { "bob", bob_private_key }, { "carol", carol_private_key }, { "diana", diana_private_key } };
      for( auto& actor : _actors )
      {
        executor->vest( actor.name, ASSET( "100.000 TESTS" ) );
        executor->issue_funds( actor.name, ASSET( "200.000 TBD" ) );
      }
      executor->generate_block();

      dhf_database::create_proposal_data cpd( executor->db->head_block_time() );

      BOOST_TEST_MESSAGE( "Create 'create_proposal_operation'" );
      int64_t _id_proposal = executor->create_proposal( cpd.creator, cpd.receiver, cpd.start_date, cpd.end_date, cpd.daily_pay, alice_private_key, alice_post_key, false/*with_block_generation*/ );
      executor->generate_block();

      BOOST_TEST_MESSAGE( "Create some `update_proposal_votes_operation`" );
      for( auto& actor : _actors )
      {
        executor->vote_proposal( actor.name, { _id_proposal }, true, actor.key );
        executor->generate_block();
      }

      BOOST_TEST_MESSAGE( "Create some 'decline_voting_rights'" );
      for( auto& actor : _actors )
      {
        decline_voting_rights_operation op;
        op.account = actor.name;
        op.decline = true;

        signed_transaction tx;
        tx.operations.push_back( op );
        tx.set_expiration( executor->db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );

        executor->push_transaction( tx, actor.key );

        executor->generate_blocks( 2 );
      }
      BOOST_TEST_MESSAGE( "Change `can_vote` in some `decline_voting_rights_request_object` objects" );
      {
        auto& _account = executor->db->get_account( _actors[0].name );
        executor->db->modify( _account, [&]( account_object& account )
        {
          account.can_vote = false;
        } );
      }
      {
        auto& _account = executor->db->get_account( _actors[2].name );
        executor->db->modify( _account, [&]( account_object& account )
        {
          account.can_vote = false;
        } );
      }

      BOOST_TEST_MESSAGE( "Call `db.remove_proposal_votes_for_accounts_without_voting_rights` during HF28 execution" );
      executor->db->set_hardfork( HIVE_HARDFORK_1_28 );

      BOOST_TEST_MESSAGE( "All actors must have proposal votes" );
      for( auto& actor : _actors )
        BOOST_REQUIRE( executor->find_vote_for_proposal( actor.name, _id_proposal ) != nullptr );

      executor->generate_block();

      BOOST_TEST_MESSAGE( "Some actors have proposal votes some don't" );
      BOOST_REQUIRE( executor->find_vote_for_proposal( _actors[0].name, _id_proposal ) == nullptr );
      BOOST_REQUIRE( executor->find_vote_for_proposal( _actors[1].name, _id_proposal ) != nullptr );
      BOOST_REQUIRE( executor->find_vote_for_proposal( _actors[2].name, _id_proposal ) == nullptr );
      BOOST_REQUIRE( executor->find_vote_for_proposal( _actors[3].name, _id_proposal ) != nullptr );
    };

    execute_hardfork<27>( _content );
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( basic_expiration_test )
{
  try
  {
    std::vector<bool> _statuses;
    std::vector<bool> _additional_statuses;

    auto _content = [&_statuses, &_additional_statuses]( ptr_hardfork_database_fixture& executor )
    {
      BOOST_TEST_MESSAGE( "Testing: transactions with different expiration time" );

      ACTORS_EXT( (*executor), (alice)(bob) );
      executor->vest( "alice", ASSET( "100.000 TESTS" ) );
      executor->vest( "bob", ASSET( "100.000 TESTS" ) );
      executor->issue_funds( "alice", ASSET( "200.000 TESTS" ) );
      executor->issue_funds( "bob", ASSET( "200.000 TESTS" ) );

      executor->generate_blocks( 60 / HIVE_BLOCK_INTERVAL );

      size_t _test_cnt = 0;

      transfer_operation _op;
      _op.from = "alice";
      _op.to = "bob";
      _op.amount = asset(1,HIVE_SYMBOL);
      _op.memo = "";

      {
        BOOST_TEST_MESSAGE( "0) Test with expiration: `HIVE_MAX_TIME_UNTIL_SIGNATURE_EXPIRATION`" );

        signed_transaction _trx;
        _trx.operations.push_back( _op );
        _trx.set_expiration( executor->db->head_block_time() + HIVE_MAX_TIME_UNTIL_SIGNATURE_EXPIRATION );

        if( _statuses[_test_cnt] )
          executor->push_transaction( _trx, alice_private_key );
        else
          HIVE_REQUIRE_THROW( executor->push_transaction( _trx, alice_private_key ), hive::chain::transaction_expiration_exception );
        ++_test_cnt;
      }
      {
        BOOST_TEST_MESSAGE( "1) Test with expiration: `HIVE_MAX_TIME_UNTIL_SIGNATURE_EXPIRATION` - 1" );

        signed_transaction _trx;
        _trx.operations.push_back( _op );
        _trx.set_expiration( executor->db->head_block_time() + HIVE_MAX_TIME_UNTIL_SIGNATURE_EXPIRATION - 1 );

        if( _statuses[_test_cnt] )
          executor->push_transaction( _trx, alice_private_key );
        else
          HIVE_REQUIRE_THROW( executor->push_transaction( _trx, alice_private_key ), hive::chain::transaction_expiration_exception );
        ++_test_cnt;
      }
      {
        BOOST_TEST_MESSAGE( "2) Test with expiration: `HIVE_MAX_TIME_UNTIL_SIGNATURE_EXPIRATION` + 1" );

        signed_transaction _trx;
        _trx.operations.push_back( _op );
        _trx.set_expiration( executor->db->head_block_time() + HIVE_MAX_TIME_UNTIL_SIGNATURE_EXPIRATION + 1 );

        if( _statuses[_test_cnt] )
          executor->push_transaction( _trx, alice_private_key );
        else
          HIVE_REQUIRE_THROW( executor->push_transaction( _trx, alice_private_key ), hive::chain::transaction_expiration_exception );
        ++_test_cnt;
      }
      {
        BOOST_TEST_MESSAGE( "3) Test with expiration: `HIVE_MAX_TIME_UNTIL_EXPIRATION` + earlier generate blocks for 5 hours" );

        signed_transaction _trx;
        _trx.operations.push_back( _op );
        _trx.set_expiration( executor->db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
        ilog("db->head_block_time() ${a} _trx.expiration: ${b}", ("a", executor->db->head_block_time())("b", _trx.expiration));

        executor->generate_blocks( executor->db->head_block_time() + fc::hours( 5 ) );

        if( _statuses[_test_cnt] )
          executor->push_transaction( _trx, alice_private_key );
        else
          HIVE_REQUIRE_THROW( executor->push_transaction( _trx, alice_private_key ), hive::chain::transaction_expiration_exception );
        ++_test_cnt;
      }
      {
        BOOST_TEST_MESSAGE( "4) Test with expiration: `HIVE_MAX_TIME_UNTIL_EXPIRATION` + earlier generate blocks for 30 minutes" );

        signed_transaction _trx;
        _trx.operations.push_back( _op );
        _trx.set_expiration( executor->db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );

        executor->generate_blocks( executor->db->head_block_time() + fc::minutes( 30 ) );

        if( _statuses[_test_cnt] )
          executor->push_transaction( _trx, alice_private_key );
        else
          HIVE_REQUIRE_THROW( executor->push_transaction( _trx, alice_private_key ), hive::chain::transaction_expiration_exception );
        ++_test_cnt;
      }
      {
        BOOST_TEST_MESSAGE( "5) Test with expiration: `HIVE_MAX_TIME_UNTIL_EXPIRATION` + earlier generate blocks for ( `HIVE_MAX_TIME_UNTIL_EXPIRATION` - `HIVE_BLOCK_INTERVAL` ) seconds" );

        signed_transaction _trx;
        _trx.operations.push_back( _op );
        _trx.set_expiration( executor->db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );

        executor->generate_blocks( executor->db->head_block_time() + fc::seconds( HIVE_MAX_TIME_UNTIL_EXPIRATION - HIVE_BLOCK_INTERVAL ) );

        if( _statuses[_test_cnt] )
          executor->push_transaction( _trx, alice_private_key );
        else
          HIVE_REQUIRE_THROW( executor->push_transaction( _trx, alice_private_key ), hive::chain::transaction_expiration_exception );
        ++_test_cnt;
      }
      {
        BOOST_TEST_MESSAGE( "6) Test with expiration: `HIVE_MAX_TIME_UNTIL_EXPIRATION` + earlier generate blocks for `HIVE_MAX_TIME_UNTIL_EXPIRATION` seconds" );

        signed_transaction _trx;
        _trx.operations.push_back( _op );
        _trx.set_expiration( executor->db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );

        executor->generate_blocks( executor->db->head_block_time() + fc::seconds( HIVE_MAX_TIME_UNTIL_EXPIRATION ) );

        if( _statuses[_test_cnt] )
          executor->push_transaction( _trx, alice_private_key );
        else
          HIVE_REQUIRE_THROW( executor->push_transaction( _trx, alice_private_key ), hive::chain::transaction_expiration_exception );
        ++_test_cnt;
      }
      {
        BOOST_TEST_MESSAGE( "7) Test with expiration: `HIVE_MAX_TIME_UNTIL_EXPIRATION`. Try to add duplicated transactions." );

        signed_transaction _trx;
        _trx.operations.push_back( _op );
        _trx.set_expiration( executor->db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );

        executor->push_transaction( _trx, alice_private_key );

        executor->generate_blocks( executor->db->head_block_time() + fc::seconds( HIVE_BLOCK_INTERVAL ) );

        if( _statuses[_test_cnt] )
          executor->push_transaction( _trx, alice_private_key );
        else
        {
          bool _passed = true;
          try
          {
            executor->push_transaction( _trx, alice_private_key );
          }
          catch( const fc::assert_exception& ex )
          {
            _passed = false;
            BOOST_TEST_MESSAGE("Caught assert exception: " + ex.to_string() );
            BOOST_REQUIRE( ex.to_string().find( "Duplicate transaction check failed" )  != std::string::npos );
          }
          BOOST_REQUIRE( !_passed );
        }
        ++_test_cnt;
      }
      {
        BOOST_TEST_MESSAGE( "8) Test with expiration: 2 * `HIVE_MAX_TIME_UNTIL_EXPIRATION`. Try to add duplicated transactions." );

        signed_transaction _trx;
        _trx.operations.push_back( _op );
        _trx.set_expiration( executor->db->head_block_time() + 2 * HIVE_MAX_TIME_UNTIL_EXPIRATION );

        if( _additional_statuses[_test_cnt] )
          executor->push_transaction( _trx, alice_private_key );
        else
          HIVE_REQUIRE_THROW( executor->push_transaction( _trx, alice_private_key ), hive::chain::transaction_expiration_exception );

        executor->generate_blocks( executor->db->head_block_time() + fc::seconds( HIVE_BLOCK_INTERVAL ) );

        if( _statuses[_test_cnt] )
          executor->push_transaction( _trx, alice_private_key );
        else
        {
          bool _passed = true;
          try
          {
            executor->push_transaction( _trx, alice_private_key );
          }
          catch( const fc::assert_exception& ex )
          {
            _passed = false;

            BOOST_TEST_MESSAGE("Caught assert exception: " + ex.to_string() );
            if( _additional_statuses[_test_cnt] )
              BOOST_REQUIRE( ex.to_string().find( "Duplicate transaction check failed" )  != std::string::npos );
            else
              BOOST_REQUIRE( ex.to_string().find( "trx.expiration <= now" )  != std::string::npos );
          }
          BOOST_REQUIRE( !_passed );
        }
        ++_test_cnt;
      }
    };

    const size_t _max = 9;

    BOOST_TEST_MESSAGE( "*****HF-27*****" );

    _statuses.resize( _max, false );
    _statuses[4] = _statuses[5] = true;

    _additional_statuses.resize( _max, false );

    execute_hardfork<27>( _content );

    BOOST_TEST_MESSAGE( "*****HF-28*****" );

    _statuses.resize( _max, false );
    _statuses[0] = _statuses[1] = _statuses[4] = _statuses[5] = true;

    _additional_statuses.resize( _max, false );
    _additional_statuses[8] = true;

    execute_hardfork<28>( _content );
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( mixed_authorities_in_one_transaction )
{
  try
  {
    bool is_hf28 = false;

    auto _content = [&is_hf28]( ptr_hardfork_database_fixture& executor )
    {
        BOOST_TEST_MESSAGE( "Testing: 'mixed_authorities_in_one_transaction'" );
        BOOST_REQUIRE_EQUAL( (bool)executor, true );

        ACTORS_EXT( (*executor), (alice)(bob) );
        executor->vest( "alice", ASSET( "1.000 TESTS" ) );
        executor->vest( "bob", ASSET( "1.000 TESTS" ) );
        executor->issue_funds( "alice", ASSET( "100.000 TESTS" ) );

        account_witness_proxy_operation _proxy_op;
        _proxy_op.account = "alice";
        _proxy_op.proxy = "bob";

        comment_operation _comment_op;
        _comment_op.parent_permlink = "category";
        _comment_op.author = "alice";
        _comment_op.permlink = "test";
        _comment_op.title = "test";
        _comment_op.body = "test";

        transfer_operation _transfer_op;
        _transfer_op.from = "alice";
        _transfer_op.to = "bob";
        _transfer_op.amount = ASSET( "5.000 TESTS" );

        signed_transaction _tx;
        _tx.operations.push_back( _proxy_op );
        _tx.operations.push_back( _comment_op );
        _tx.set_expiration( executor->db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );

        signed_transaction _tx_02;
        _tx_02.operations.push_back( _transfer_op );
        _tx_02.set_expiration( executor->db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );

        if( is_hf28 )
        {
          HIVE_REQUIRE_THROW( executor->push_transaction( _tx, alice_private_key ), tx_missing_posting_auth );
          HIVE_REQUIRE_THROW( executor->push_transaction( _tx, alice_post_key ), tx_missing_active_auth );
          executor->push_transaction( _tx, {alice_private_key, alice_post_key} );
          executor->push_transaction( _tx_02, {alice_private_key, bob_private_key} );
        }
        else
        {
          HIVE_REQUIRE_THROW( executor->push_transaction( _tx, alice_post_key ), tx_combined_auths_with_posting );
          HIVE_REQUIRE_THROW( executor->push_transaction( _tx, alice_private_key ), tx_combined_auths_with_posting );
          HIVE_REQUIRE_THROW( executor->push_transaction( _tx, { alice_private_key, alice_post_key } ), tx_combined_auths_with_posting );
          HIVE_REQUIRE_THROW( executor->push_transaction( _tx_02, {alice_private_key, bob_private_key} ), tx_irrelevant_sig );
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

BOOST_AUTO_TEST_CASE( mixed_authorities_in_one_transaction_2 )
{
  try
  {
    bool is_hf28 = false;

    auto _content = [&is_hf28]( ptr_hardfork_database_fixture& executor )
    {
        BOOST_TEST_MESSAGE( "Testing: 'mixed_authorities_in_one_transaction'" );
        BOOST_REQUIRE_EQUAL( (bool)executor, true );

        ACTORS_EXT( (*executor), (alice)(bob) );
        executor->vest( "alice", ASSET( "1.000 TESTS" ) );
        executor->vest( "bob", ASSET( "1.000 TESTS" ) );
        executor->issue_funds( "alice", ASSET( "100.000 TESTS" ) );
        executor->issue_funds( "bob", ASSET( "100.000 TESTS" ) );

        account_update_operation _acc_update_op;
        _acc_update_op.account = "alice";
        _acc_update_op.active = authority( 1, "bob", 1 );
        _acc_update_op.posting = authority( 1, "bob", 1 );

        comment_operation _comment_op;
        _comment_op.parent_permlink = "category";
        _comment_op.author = "alice";
        _comment_op.permlink = "test";
        _comment_op.title = "test";
        _comment_op.body = "test";

        transfer_operation _transfer_op;
        _transfer_op.from = "alice";
        _transfer_op.to = "bob";
        _transfer_op.amount = ASSET( "5.000 TESTS" );

        signed_transaction _tx_00;
        _tx_00.operations.push_back( _acc_update_op );
        _tx_00.set_expiration( executor->db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );

        signed_transaction _tx_01;
        _tx_01.operations.push_back( _comment_op );
        _tx_01.operations.push_back( _transfer_op );
        _tx_01.set_expiration( executor->db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );

        executor->push_transaction( _tx_00, alice_private_key );

        executor->generate_block();

        if( is_hf28 )
        {
          BOOST_TEST_MESSAGE("Failure: a mixed transaction has only active key");
          HIVE_REQUIRE_THROW( executor->push_transaction( _tx_01, { bob_private_key } ), tx_missing_posting_auth );

          BOOST_TEST_MESSAGE("Failure: a mixed transaction has only posting key");
          HIVE_REQUIRE_THROW( executor->push_transaction( _tx_01, { bob_post_key } ), tx_missing_active_auth );

          BOOST_TEST_MESSAGE("Success: a mixed transaction has sufficient number of keys");
          executor->push_transaction( _tx_01, { bob_private_key, bob_post_key } );

          executor->generate_blocks( executor->db->head_block_time() + HIVE_MIN_COMMENT_EDIT_INTERVAL );

          BOOST_TEST_MESSAGE("Success: a mixed transaction has sufficient number of keys");
          _tx_01.set_expiration( executor->db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION - HIVE_BLOCK_INTERVAL );
          executor->push_transaction( _tx_01, { bob_post_key, bob_private_key } );
        }
        else
        {
          BOOST_TEST_MESSAGE("Failure: a mixed transaction is not allowed");
          HIVE_REQUIRE_THROW( executor->push_transaction( _tx_01, { bob_private_key } ), tx_combined_auths_with_posting );

          BOOST_TEST_MESSAGE("Failure: a mixed transaction is not allowed");
          HIVE_REQUIRE_THROW( executor->push_transaction( _tx_01, { bob_post_key } ), tx_combined_auths_with_posting );

          BOOST_TEST_MESSAGE("Failure: a mixed transaction is not allowed");
          HIVE_REQUIRE_THROW( executor->push_transaction( _tx_01, { bob_private_key, bob_post_key } ), tx_combined_auths_with_posting );

          BOOST_TEST_MESSAGE("Failure: a mixed transaction is not allowed");
          HIVE_REQUIRE_THROW( executor->push_transaction( _tx_01, { bob_post_key, bob_private_key } ), tx_combined_auths_with_posting );
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

BOOST_AUTO_TEST_CASE( verify_authority_limits )
{
  try
  {
    bool is_hf28 = false;

    auto _content = [&is_hf28]( ptr_hardfork_database_fixture& executor )
    {
      BOOST_TEST_MESSAGE( "Testing limits in authorities verification" );

      authority _empty_auth(  1/*threshold*/, executor->generate_private_key( "empty" ).get_public_key(), 1 );

      BOOST_TEST_MESSAGE( "Creating set of keys" );
      std::vector< public_key_type > _keys;
      const size_t _nr_keys = 10;
      for( size_t i = 0; i < _nr_keys; ++i )
        _keys.emplace_back( executor->generate_private_key( std::to_string( i ) ).get_public_key() );

      BOOST_TEST_MESSAGE( "Creating set of accounts" );
      std::vector< std::string > _accs;
      const size_t _nr_accs = 10;
      for( size_t i = 0; i < _nr_accs; ++i )
        _accs.emplace_back( std::to_string( i ) );

      bool _allow_strict_and_mixed_authorities  = executor->db->has_hardfork( HIVE_HARDFORK_1_28_ALLOW_STRICT_AND_MIXED_AUTHORITIES );
      bool _allow_redundant_signatures          = executor->db->has_hardfork( HIVE_HARDFORK_1_28_ALLOW_REDUNDANT_SIGNATURES );

      {
        uint32_t _hive_max_sig_check_depth       = 2;
        uint32_t _hive_max_authority_membership  = 3;
        uint32_t _hive_max_sig_check_accounts    = 4;

        auto _get_active = [&]( const std::string& name ) { return _empty_auth; };
        auto _get_owner = [&]( const std::string& name ) { return _empty_auth; };
        auto _get_posting = [&]( const std::string& name ) { return _empty_auth; };
        auto _get_witness_key = [&]( const std::string& name ) { return public_key_type(); };

        required_authorities_type _required_authorities;
        _required_authorities.required_posting.insert( "xyz" );

        flat_set<public_key_type> _signatures;
        _signatures.insert( _keys[0] );

        BOOST_TEST_MESSAGE( "Failure. Posting authority: required 1 signature, all authorities are empty." );

        HIVE_REQUIRE_THROW( hive::protocol::verify_authority
        ( _allow_strict_and_mixed_authorities, _allow_redundant_signatures,
          _required_authorities, _signatures,
          _get_active, _get_owner, _get_posting, _get_witness_key,
          _hive_max_sig_check_depth, _hive_max_authority_membership, _hive_max_sig_check_accounts
        ), tx_missing_posting_auth );
      }
      {
        uint32_t _hive_max_sig_check_depth       = 10;
        uint32_t _hive_max_authority_membership  = 3;
        uint32_t _hive_max_sig_check_accounts    = 10;

        authority _posting_auth;
        _posting_auth.weight_threshold = _hive_max_authority_membership;

        for( size_t i = 0; i < _hive_max_authority_membership - 1; ++i )
          _posting_auth.add_authority( _keys[i], 1 );

        auto _get_active = [&]( const std::string& name ) { return _empty_auth; };
        auto _get_owner = [&]( const std::string& name ) { return _empty_auth; };
        auto _get_posting = [&]( const std::string& name ) { return _posting_auth; };
        auto _get_witness_key = [&]( const std::string& name ) { return public_key_type(); };

        required_authorities_type _required_authorities;
        _required_authorities.required_posting.insert( "xyz" );

        flat_set<public_key_type> _signatures;
        for( size_t i = 0; i < _hive_max_authority_membership; ++i )
          _signatures.insert( _keys[i] );

        BOOST_TEST_MESSAGE( "Failure. Posting authority: required 3 signatures, but only 2 keys are given." );

        HIVE_REQUIRE_THROW( hive::protocol::verify_authority
        (
          executor->db->has_hardfork( HIVE_HARDFORK_1_28_ALLOW_STRICT_AND_MIXED_AUTHORITIES ),
          executor->db->has_hardfork( HIVE_HARDFORK_1_28_ALLOW_REDUNDANT_SIGNATURES ),
          _required_authorities, _signatures, _get_active, _get_owner, _get_posting, _get_witness_key,
          _hive_max_sig_check_depth,
          _hive_max_authority_membership,
          _hive_max_sig_check_accounts
        ), tx_missing_posting_auth );

        BOOST_TEST_MESSAGE( "Success. Posting authority: required 3 signatures and 3 keys are given." );
        _posting_auth.add_authority( _keys[ _hive_max_authority_membership - 1 ], 1 );

        hive::protocol::verify_authority
        (
          executor->db->has_hardfork( HIVE_HARDFORK_1_28_ALLOW_STRICT_AND_MIXED_AUTHORITIES ),
          executor->db->has_hardfork( HIVE_HARDFORK_1_28_ALLOW_REDUNDANT_SIGNATURES ),
          _required_authorities, _signatures, _get_active, _get_owner, _get_posting, _get_witness_key,
          _hive_max_sig_check_depth,
          _hive_max_authority_membership,
          _hive_max_sig_check_accounts
        );

        BOOST_TEST_MESSAGE( "Failure. Posting authority: required 3 signatures and 3 keys are given, but `_hive_max_authority_membership` == 2." );
        _hive_max_authority_membership = 2;

        HIVE_REQUIRE_THROW( hive::protocol::verify_authority
        (
          executor->db->has_hardfork( HIVE_HARDFORK_1_28_ALLOW_STRICT_AND_MIXED_AUTHORITIES ),
          executor->db->has_hardfork( HIVE_HARDFORK_1_28_ALLOW_REDUNDANT_SIGNATURES ),
          _required_authorities, _signatures, _get_active, _get_owner, _get_posting, _get_witness_key,
          _hive_max_sig_check_depth,
          _hive_max_authority_membership,
          _hive_max_sig_check_accounts
        ), tx_missing_posting_auth );
      }
      {
        uint32_t _hive_max_sig_check_depth       = 10;
        uint32_t _hive_max_authority_membership  = 10;
        uint32_t _hive_max_sig_check_accounts    = 3;

        size_t _keys_iter = 0;

        authority _posting_auth;
        _posting_auth.weight_threshold = 3;
        _posting_auth.add_authority( _keys[_keys_iter++], 1 );
        _posting_auth.add_authority( "00", 2 );

        authority _posting_auth_00;
        _posting_auth_00.weight_threshold = 2;
        _posting_auth_00.add_authority( _keys[_keys_iter++], 1 );
        _posting_auth_00.add_authority( "01", 1 );

        authority _posting_auth_01;
        _posting_auth_01.weight_threshold = 1;
        _posting_auth_01.add_authority( _keys[_keys_iter++], 1 );

        std::map<std::string, authority> accs_auths;
        accs_auths["start"] = _posting_auth;
        accs_auths["00"] = _posting_auth_00;
        accs_auths["01"] = _posting_auth_01;

        auto _get_active = [&]( const std::string& name ) { return _empty_auth; };
        auto _get_owner = [&]( const std::string& name ) { return _empty_auth; };
        auto _get_posting = [&]( const std::string& name ) { return accs_auths[ name ]; };
        auto _get_witness_key = [&]( const std::string& name ) { return public_key_type(); };

        required_authorities_type _required_authorities;
        _required_authorities.required_posting.insert( "start" );

        flat_set<public_key_type> _signatures;
        for( size_t i = 0; i < _hive_max_sig_check_accounts; ++i )
          _signatures.insert( _keys[i] );

        BOOST_TEST_MESSAGE( "Success. Posting authority: required 3 signatures and 3 keys are given. Every user on every level has exactly 1 key." );

        hive::protocol::verify_authority
        (
          executor->db->has_hardfork( HIVE_HARDFORK_1_28_ALLOW_STRICT_AND_MIXED_AUTHORITIES ),
          executor->db->has_hardfork( HIVE_HARDFORK_1_28_ALLOW_REDUNDANT_SIGNATURES ),
          _required_authorities, _signatures, _get_active, _get_owner, _get_posting, _get_witness_key,
          _hive_max_sig_check_depth,
          _hive_max_authority_membership,
          _hive_max_sig_check_accounts
        );

        BOOST_TEST_MESSAGE( "Failure. Posting authority: required 3 signatures and 3 keys are given. Every user on every level has exactly 1 key, but `_hive_max_sig_check_accounts` == 2." );
        _hive_max_sig_check_accounts = 2;
        HIVE_REQUIRE_THROW( hive::protocol::verify_authority
        (
          executor->db->has_hardfork( HIVE_HARDFORK_1_28_ALLOW_STRICT_AND_MIXED_AUTHORITIES ),
          executor->db->has_hardfork( HIVE_HARDFORK_1_28_ALLOW_REDUNDANT_SIGNATURES ),
          _required_authorities, _signatures, _get_active, _get_owner, _get_posting, _get_witness_key,
          _hive_max_sig_check_depth,
          _hive_max_authority_membership,
          _hive_max_sig_check_accounts
        ), tx_missing_posting_auth );

        BOOST_TEST_MESSAGE( "Failure. Posting authority: required 3 signatures and 3 keys are given. Every user on every level has exactly 1 key, but `_hive_max_sig_check_depth` == 1." );
        _hive_max_sig_check_accounts = 3;
        _hive_max_sig_check_depth = 1;
        HIVE_REQUIRE_THROW( hive::protocol::verify_authority
        (
          executor->db->has_hardfork( HIVE_HARDFORK_1_28_ALLOW_STRICT_AND_MIXED_AUTHORITIES ),
          executor->db->has_hardfork( HIVE_HARDFORK_1_28_ALLOW_REDUNDANT_SIGNATURES ),
          _required_authorities, _signatures, _get_active, _get_owner, _get_posting, _get_witness_key,
          _hive_max_sig_check_depth,
          _hive_max_authority_membership,
          _hive_max_sig_check_accounts
        ), tx_missing_posting_auth );
      }
      {
        uint32_t _hive_max_sig_check_depth       = 10;
        uint32_t _hive_max_authority_membership  = 2;
        uint32_t _hive_max_sig_check_accounts    = 10;

        authority _active_auth;
        _active_auth.weight_threshold = 1;
        _active_auth.add_authority( _keys[8], 1 );
        _active_auth.add_authority( "00", 1 );

        authority _owner_auth;
        _owner_auth.weight_threshold = 1;
        _owner_auth.add_authority( _keys[9], 1 );
        _owner_auth.add_authority( "01", 1 );

        size_t _keys_iter = 0;

        authority _active_auth_00;
        _active_auth_00.weight_threshold = 1;
        _active_auth_00.add_authority( _keys[_keys_iter++], 1 );

        authority _owner_auth_01;
        _owner_auth_01.weight_threshold = 1;
        _owner_auth_01.add_authority( _keys[_keys_iter++], 1 );

        std::map<std::string, authority> _accs_auths_active;
        _accs_auths_active["start"] = _active_auth;
        _accs_auths_active["00"]    = _active_auth_00;
        _accs_auths_active["01"]    = _owner_auth_01;

        std::map<std::string, authority> _accs_auths_owner;
        _accs_auths_owner["start"]  = _owner_auth;
        _accs_auths_owner["01"]     = _owner_auth_01;

        auto _get_active = [&]( const std::string& name ) { return _accs_auths_active[ name ]; };
        auto _get_owner = [&]( const std::string& name ) { return _accs_auths_owner[ name ]; };
        auto _get_posting = [&]( const std::string& name ) { return _empty_auth; };
        auto _get_witness_key = [&]( const std::string& name ) { return public_key_type(); };

        required_authorities_type _required_authorities;
        _required_authorities.required_active.insert( "start" );
        _required_authorities.required_owner.insert( "start" );

        flat_set<public_key_type> _signatures;
        for( size_t i = 0; i < 2; ++i )
          _signatures.insert( _keys[i] );

        BOOST_TEST_MESSAGE( "Success. Incrementing of `membership` is reset when every `check_authority` is called. Such situation occurs twice for `active`/`owner` authorities calculation.");
        hive::protocol::verify_authority
        (
          executor->db->has_hardfork( HIVE_HARDFORK_1_28_ALLOW_STRICT_AND_MIXED_AUTHORITIES ),
          executor->db->has_hardfork( HIVE_HARDFORK_1_28_ALLOW_REDUNDANT_SIGNATURES ),
          _required_authorities, _signatures, _get_active, _get_owner, _get_posting, _get_witness_key,
          _hive_max_sig_check_depth,
          _hive_max_authority_membership,
          _hive_max_sig_check_accounts
        );

        _hive_max_sig_check_depth       = 10;
        _hive_max_authority_membership  = 10;
        _hive_max_sig_check_accounts    = 2;

        if( is_hf28 )
        {
          BOOST_TEST_MESSAGE( "Failure. In HF28 `account_auth_count` variable is held in a class like a member. Calling `check_authority` doesn't reset a counter. The limit is reached too early, because `_hive_max_sig_check_accounts == 2`.");
          HIVE_REQUIRE_THROW( hive::protocol::verify_authority
          (
            executor->db->has_hardfork( HIVE_HARDFORK_1_28_ALLOW_STRICT_AND_MIXED_AUTHORITIES ),
            executor->db->has_hardfork( HIVE_HARDFORK_1_28_ALLOW_REDUNDANT_SIGNATURES ),
            _required_authorities, _signatures, _get_active, _get_owner, _get_posting, _get_witness_key,
            _hive_max_sig_check_depth,
            _hive_max_authority_membership,
            _hive_max_sig_check_accounts
          ), tx_missing_owner_auth );
        }
        else
        {
          BOOST_TEST_MESSAGE( "Success. In HF27 incrementing of `account_auth_count` is reset when every `check_authority` is called. Such situation occurs twice for `active`/`owner` authorities calculation.");
          hive::protocol::verify_authority
          (
            executor->db->has_hardfork( HIVE_HARDFORK_1_28_ALLOW_STRICT_AND_MIXED_AUTHORITIES ),
            executor->db->has_hardfork( HIVE_HARDFORK_1_28_ALLOW_REDUNDANT_SIGNATURES ),
            _required_authorities, _signatures, _get_active, _get_owner, _get_posting, _get_witness_key,
            _hive_max_sig_check_depth,
            _hive_max_authority_membership,
            _hive_max_sig_check_accounts
          );
        }
      }
      {
        uint32_t _hive_max_sig_check_depth       = 10;
        uint32_t _hive_max_authority_membership  = 2;
        uint32_t _hive_max_sig_check_accounts    = 10;

        size_t _keys_iter = 0;

        authority _active_auth;
        _active_auth.weight_threshold = 1;
        _active_auth.add_authority( _keys[8], 1 );
        _active_auth.add_authority( _keys[_keys_iter++], 1 );

        authority _owner_auth;
        _owner_auth.weight_threshold = 1;
        _owner_auth.add_authority( _keys[9], 1 );
        _owner_auth.add_authority( _keys[_keys_iter++], 1 );

        auto _get_active = [&]( const std::string& name ) { return _active_auth; };
        auto _get_owner = [&]( const std::string& name ) { return _owner_auth; };
        auto _get_posting = [&]( const std::string& name ) { return _empty_auth; };
        auto _get_witness_key = [&]( const std::string& name ) { return public_key_type(); };

        required_authorities_type _required_authorities;
        _required_authorities.required_active.insert( "start" );
        _required_authorities.required_owner.insert( "start" );

        flat_set<public_key_type> _signatures;
        for( size_t i = 0; i < 2; ++i )
          _signatures.insert( _keys[i] );

        BOOST_TEST_MESSAGE( "Success. Incrementing of `membership` is reset when every `check_authority` is called. Such situation occurs twice for `active`/`owner` authorities calculation.");
        hive::protocol::verify_authority
        (
          executor->db->has_hardfork( HIVE_HARDFORK_1_28_ALLOW_STRICT_AND_MIXED_AUTHORITIES ),
          executor->db->has_hardfork( HIVE_HARDFORK_1_28_ALLOW_REDUNDANT_SIGNATURES ),
          _required_authorities, _signatures, _get_active, _get_owner, _get_posting, _get_witness_key,
          _hive_max_sig_check_depth,
          _hive_max_authority_membership,
          _hive_max_sig_check_accounts
        );
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

BOOST_AUTO_TEST_CASE( verify_authority_limits_for_temp_account )
{
  try
  {
    bool is_hf28 = false;

    auto _content = [&is_hf28]( ptr_hardfork_database_fixture& executor )
    {
      BOOST_TEST_MESSAGE( "Testing limits in authorities verification for `temp` account" );

      authority _empty_auth_0;
      _empty_auth_0.weight_threshold = 0;

      authority _empty_auth_1;
      _empty_auth_1.weight_threshold = 1;

      bool _allow_strict_and_mixed_authorities  = executor->db->has_hardfork( HIVE_HARDFORK_1_28_ALLOW_STRICT_AND_MIXED_AUTHORITIES );
      bool _allow_redundant_signatures          = executor->db->has_hardfork( HIVE_HARDFORK_1_28_ALLOW_REDUNDANT_SIGNATURES );

      {
        uint32_t _hive_max_sig_check_depth       = 2;
        uint32_t _hive_max_authority_membership  = 3;
        uint32_t _hive_max_sig_check_accounts    = 4;

        auto _get_active = [&]( const std::string& name ) { return authority( executor->db->get< account_authority_object, by_account >( name ).active ); };
        auto _get_owner = [&]( const std::string& name ) { return authority( executor->db->get< account_authority_object, by_account >( name ).owner ); };
        auto _get_posting = [&]( const std::string& name ) { return authority( executor->db->get< account_authority_object, by_account >( name ).posting ); };
        auto _get_witness_key = [&]( const std::string& name ) { try { return executor->db->get_witness( name ).signing_key; } FC_CAPTURE_AND_RETHROW( ( name ) ) };

        {
          required_authorities_type _required_authorities;
          _required_authorities.required_posting.insert( "temp" );

          flat_set<public_key_type> _signatures;

          BOOST_TEST_MESSAGE( "Success. Posting authority for `temp`: required 0 signatures, all authorities are empty, `threshold` == 1( HF27) `threshold` == 0( HF28)." );

          hive::protocol::verify_authority
          ( _allow_strict_and_mixed_authorities, _allow_redundant_signatures,
            _required_authorities, _signatures,
            _get_active, _get_owner, _get_posting, _get_witness_key,
            _hive_max_sig_check_depth, _hive_max_authority_membership, _hive_max_sig_check_accounts
          );
        }
        {
          required_authorities_type _required_authorities;
          _required_authorities.required_active.insert( "temp" );

          flat_set<public_key_type> _signatures;

          BOOST_TEST_MESSAGE( "Success. Active authority for `temp`: required 0 signatures, all authorities are empty, `threshold` == 0." );

          hive::protocol::verify_authority
          ( _allow_strict_and_mixed_authorities, _allow_redundant_signatures,
            _required_authorities, _signatures,
            _get_active, _get_owner, _get_posting, _get_witness_key,
            _hive_max_sig_check_depth, _hive_max_authority_membership, _hive_max_sig_check_accounts
          );
        }
        {
          required_authorities_type _required_authorities;
          _required_authorities.required_owner.insert( "temp" );

          flat_set<public_key_type> _signatures;

          BOOST_TEST_MESSAGE( "Success. Owner authority for `temp`: required 0 signatures, all authorities are empty, `threshold` == 0." );

          hive::protocol::verify_authority
          ( _allow_strict_and_mixed_authorities, _allow_redundant_signatures,
            _required_authorities, _signatures,
            _get_active, _get_owner, _get_posting, _get_witness_key,
            _hive_max_sig_check_depth, _hive_max_authority_membership, _hive_max_sig_check_accounts
          );
        }
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

BOOST_AUTO_TEST_CASE( different_behaviour_for_nonexistent_proposals )
{
  try
  {
    bool is_hf28 = false;

    auto _content = [ &is_hf28 ]( ptr_hardfork_database_fixture& executor )
    {
      BOOST_TEST_MESSAGE( "Testing: different_behaviour for not existing proposals" );
      BOOST_REQUIRE_EQUAL( (bool)executor, true );

      ACTORS_EXT( (*executor), (alice)(bob) );

      executor->generate_block();

      executor->vest( "alice", ASSET( "100.000 TESTS" ) );
      executor->vest( "bob", ASSET( "100.000 TESTS" ) );
      executor->issue_funds( "alice", ASSET( "200.000 TBD" ) );
      executor->issue_funds( "bob", ASSET( "200.000 TBD" ) );

      executor->generate_block();

      dhf_database::create_proposal_data cpd( executor->db->head_block_time() );

      BOOST_TEST_MESSAGE( "Create 'create_proposal_operation'" );
      int64_t _id_proposal = executor->create_proposal( cpd.creator, cpd.receiver, cpd.start_date, cpd.end_date, cpd.daily_pay, alice_private_key, alice_post_key, false/*with_block_generation*/ );

      BOOST_TEST_MESSAGE( "Create 'update_proposal_votes_operation'" );

      //An exception must be thrown because there is a condition `_db.is_in_control()` or `_db.has_hardfork( HIVE_HARDFORK_1_28 )`
      if( is_hf28 )
        HIVE_REQUIRE_ASSERT( executor->vote_proposal("alice", { 666 }, true, alice_private_key), "false && \"proposal doesn't exist\"" );
      else
        HIVE_REQUIRE_ASSERT( executor->vote_proposal("alice", { 666 }, true, alice_private_key), "false && \"proposal doesn't exist\"" );

      //An exception must be thrown because there is a condition `_db.is_in_control()` or `_db.has_hardfork( HIVE_HARDFORK_1_28 )`
      if( is_hf28 )
        HIVE_REQUIRE_ASSERT( executor->vote_proposal("alice", { _id_proposal, 666 }, true, alice_private_key), "false && \"proposal doesn't exist\"" );
      else
        HIVE_REQUIRE_ASSERT( executor->vote_proposal("alice", { _id_proposal, 666 }, true, alice_private_key), "false && \"proposal doesn't exist\"" );

      BOOST_TEST_MESSAGE( "Create 'remove_proposal_operation'" );
      //An exception must be thrown because there is a condition `_db.is_in_control()` or `_db.has_hardfork( HIVE_HARDFORK_1_28 )`
      if( is_hf28 )
        HIVE_REQUIRE_ASSERT( executor->remove_proposal("alice", { 666 }, alice_private_key), "false && \"proposal doesn't exist\"" );
      else
        HIVE_REQUIRE_ASSERT( executor->remove_proposal("alice", { 666 }, alice_private_key), "false && \"proposal doesn't exist\"" );

      //An exception must be thrown because there is a condition `_db.is_in_control()` or `_db.has_hardfork( HIVE_HARDFORK_1_28 )`
      if( is_hf28 )
        HIVE_REQUIRE_ASSERT( executor->remove_proposal("alice", { _id_proposal, 666 }, alice_private_key), "false && \"proposal doesn't exist\"" );
      else
        HIVE_REQUIRE_ASSERT( executor->remove_proposal("alice", { _id_proposal, 666 }, alice_private_key), "false && \"proposal doesn't exist\"" );
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

BOOST_FIXTURE_TEST_SUITE( hf28_tests2, genesis_database_fixture )

BOOST_AUTO_TEST_CASE( disturbed_power_down )
{
  try
  {
    autoscope reset( []() { configuration_data.reset_cashout_values(); } );
    configuration_data.set_cashout_related_values( 0, 3, 6, 21, 3 );

    // the scenario of this test replicates problem encountered by 'gil' account:
    // - it received VESTS and started power down before HF1
    // - it was supposed to last 104 weeks - normally it would happen over 104 weeks plus one
    //   more step releasing amount remaining due to truncation
    // - some power down was already executed when HF1 was activated (2 steps)
    // - this is when the bug hit the account - vesting_withdraw_rate was recalculated instead
    //   of being split by 1M like other VESTS related values
    // - the recalculation caused final step of power down to only release tiny amount of VESTS
    //   instead of all the rest
    // - for that reason power down continued up to the time of HF28, when fix was introduced
    // Note that change in HF16 to 13 weeks down from 104 didn't affect power downs already in
    // progress.
    // The test checks that the fix of HF28 actually works.
    // Note that 'weeks' in above is expressed in cashout windows, which is 1hr by default
    // in testnet (down to 7 blocks in above setting)
    const int week_blocks = HIVE_VESTING_WITHDRAW_INTERVAL_SECONDS / HIVE_BLOCK_INTERVAL; // number of blocks per "week" of power down
    const int split = 1000000;

    // amount of stake for 'bob' is divisible by 104 which is corner case due to remainder
    // being 0; it is also not affected by split
    const int64_t bob_amount = 1040000;
    int64_t bob_rate = bob_amount / 104;
    int64_t bob_vests = bob_amount;
    // amount of stake for 'gil' reflects actual amount withdrown by that account on mainnet
    const int64_t gil_amount = 1000000;
    int64_t gil_rate = gil_amount / 104;
    int64_t gil_vests = gil_amount;
    // check to make sure mainnet values are used
    static_assert( ( gil_amount / 104 == 9615 ) && ( gil_amount * split / 104 == 9615384615 ) );
    BOOST_REQUIRE( db->get_dynamic_global_properties().get_vesting_share_price() ==
      price( asset( 1000000, VESTS_SYMBOL ), asset( 1000, HIVE_SYMBOL ) ) );

    ACTORS_DEFAULT_FEE( (bob)(gil) );
    vest( "bob", asset( bob_amount / 1000, HIVE_SYMBOL ) );
    vest( "gil", asset( gil_amount / 1000, HIVE_SYMBOL ) );
    generate_block();

    BOOST_CHECK_EQUAL( get_vesting( "bob" ).amount.value, bob_amount );
    BOOST_CHECK_EQUAL( get_vesting( "gil" ).amount.value, gil_amount );

    BOOST_TEST_MESSAGE( "Start power down" );
    int start_block = db->head_block_num();
    withdraw_vesting( "bob", get_vesting( "bob" ), bob_private_key );
    withdraw_vesting( "gil", get_vesting( "gil" ), gil_private_key );
    BOOST_CHECK_EQUAL( db->get_account( "bob" ).get_next_vesting_withdrawal().value, bob_rate );
    BOOST_CHECK_EQUAL( db->get_account( "gil" ).get_next_vesting_withdrawal().value, gil_rate );

    generate_blocks( week_blocks );
    BOOST_CHECK_EQUAL( start_block + week_blocks, db->head_block_num() );

    BOOST_TEST_MESSAGE( "Check after first week of power down" );
    bob_vests -= bob_rate;
    gil_vests -= gil_rate;

    BOOST_CHECK_EQUAL( get_vesting( "bob" ).amount.value, bob_vests );
    BOOST_CHECK_EQUAL( get_vesting( "gil" ).amount.value, gil_vests );

    generate_blocks( week_blocks );
    BOOST_CHECK_EQUAL( start_block + 2 * week_blocks, db->head_block_num() );

    BOOST_TEST_MESSAGE( "Check after second week of power down" );
    bob_vests -= bob_rate;
    gil_vests -= gil_rate;

    BOOST_CHECK_EQUAL( get_vesting( "bob" ).amount.value, bob_vests );
    BOOST_CHECK_EQUAL( get_vesting( "gil" ).amount.value, gil_vests );

    BOOST_TEST_MESSAGE( "Activate split" );
    inject_hardfork( HIVE_HARDFORK_0_1 );
    bob_vests *= split;
    gil_vests *= split;

    BOOST_CHECK_EQUAL( get_vesting( "bob" ).amount.value, bob_vests );
    BOOST_CHECK_EQUAL( get_vesting( "gil" ).amount.value, gil_vests );
    // split multiplied remaining stake by 1M (actually added 6 fractional decimal digits, but
    // from the perspective of raw value is the sama as multiplication); unfortunately the power
    // down rate was not multiplied but recalculated, disturbing the flow
    bob_rate = bob_amount * split / 104;
    gil_rate = gil_amount * split / 104;
    BOOST_CHECK_EQUAL( db->get_account( "bob" ).get_next_vesting_withdrawal().value, bob_rate );
    BOOST_CHECK_EQUAL( db->get_account( "gil" ).get_next_vesting_withdrawal().value, gil_rate );

    BOOST_CHECK_EQUAL( bob_vests % bob_rate, 0 );
    BOOST_CHECK_EQUAL( gil_vests - 102 * gil_rate, 769270 );
    BOOST_CHECK_EQUAL( gil_amount * split % gil_rate, 40 );
    // we can see that after 769270 / 40 = 19231 additional weeks of power down (over 368 years)
    // gil would be left with final step trying to take 40 from balance of 769270 % 40 = 30
    // triggering error condition with NOTIFYALERT!
    // we are not going to run the test that long (we'd run into "Problem of year 2037" much earlier)

    BOOST_TEST_MESSAGE( "Run remaining power down steps without remainder" );
    for( int i = 3; i <= 104; ++i )
    {
      generate_blocks( week_blocks ); //we've run two extra blocks during HF1 injection, but it doesn't matter
      bob_vests -= bob_rate;
      gil_vests -= gil_rate;

      BOOST_TEST_MESSAGE( "Check after week " << i << " of power down");
      BOOST_CHECK_EQUAL( get_vesting( "bob" ).amount.value, bob_vests );
      BOOST_CHECK_EQUAL( get_vesting( "gil" ).amount.value, gil_vests );
    }
    BOOST_CHECK_EQUAL( get_vesting( "bob" ).amount.value, 0 );
    BOOST_CHECK_EQUAL( get_vesting( "gil" ).amount.value, 769270 );
    BOOST_CHECK_EQUAL( db->get_account( "bob" ).get_next_vesting_withdrawal().value, 0 );
    // officially whole remainder of gil's power down is for next week, but that's not the case prior HF28
    BOOST_CHECK_EQUAL( db->get_account( "gil" ).get_next_vesting_withdrawal().value, 769270 );

    BOOST_TEST_MESSAGE( "Bob finished power down, but not gil" );
    gil_rate = 40;
    for( int i = 105; i < 135; ++i )
    {
      if( i == 115 )
      {
        BOOST_TEST_MESSAGE( "Activate change from 104 weeks down to 13 weeks" );
        inject_hardfork( HIVE_HARDFORK_0_16 );
      }
      generate_blocks( week_blocks );
      gil_vests -= gil_rate;
      BOOST_TEST_MESSAGE( "Check after week " << i << " of power down" );
      BOOST_CHECK_EQUAL( get_vesting( "gil" ).amount.value, gil_vests );
    }
    BOOST_CHECK_EQUAL( get_vesting( "gil" ).amount.value, 769270 - 30*40 );
    BOOST_CHECK_EQUAL( db->get_account( "gil" ).get_next_vesting_withdrawal().value, 769270 - 30*40 );

    BOOST_TEST_MESSAGE( "Activate fix" );
    inject_hardfork( HIVE_HARDFORK_1_28 );

    BOOST_TEST_MESSAGE( "Check innediately after HF28 - nothing changed yet for gil" );
    BOOST_CHECK_EQUAL( get_vesting( "gil" ).amount.value, 769270 - 30 * 40 );
    BOOST_CHECK_EQUAL( db->get_account( "gil" ).get_next_vesting_withdrawal().value, 769270 - 30 * 40 );

    generate_blocks( week_blocks );
    BOOST_TEST_MESSAGE( "Check week after activation of HF28 - should fix power down on gil" );
    BOOST_CHECK_EQUAL( get_vesting( "gil" ).amount.value, 0 );
    BOOST_CHECK_EQUAL( db->get_account( "gil" ).get_next_vesting_withdrawal().value, 0 );

    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( vote_edit_limit )
{
  try
  {
    autoscope reset( set_mainnet_cashout_values() );

    inject_hardfork( HIVE_HARDFORK_0_20 ); // we don't care about old voting rules

    ACTORS_DEFAULT_FEE( (alice)(bob)(carol) );
    vest( "bob", ASSET( "1.000 TESTS" ) );
    vest( "carol", ASSET( "1.000 TESTS" ) );
    generate_block();

    post_comment( "alice", "test", "test", "test", "category", alice_post_key );
    generate_block();

    vote( "alice", "test", "bob", HIVE_100_PERCENT, bob_post_key );
    generate_block();
    vote( "alice", "test", "bob", 90 * HIVE_1_PERCENT, bob_post_key ); // edit 1
    generate_block();
    vote( "alice", "test", "bob", 80 * HIVE_1_PERCENT, bob_post_key ); // edit 2
    generate_block();
    vote( "alice", "test", "bob", 70 * HIVE_1_PERCENT, bob_post_key ); // edit 3
    generate_block();
    vote( "alice", "test", "bob", 60 * HIVE_1_PERCENT, bob_post_key ); // edit 4
    generate_block();
    vote( "alice", "test", "bob", 50 * HIVE_1_PERCENT, bob_post_key ); // edit 5
    generate_block();
    HIVE_REQUIRE_ASSERT( vote( "alice", "test", "bob", 40 * HIVE_1_PERCENT, bob_post_key ), "itr->get_number_of_changes() < HIVE_MAX_VOTE_CHANGES"); // edit 6 - fails

    BOOST_TEST_MESSAGE( "Activate HF28" );
    inject_hardfork( HIVE_HARDFORK_1_28 );
    BOOST_TEST_MESSAGE( "Now there is no limit on vote edits (other than RC)" );

    vote( "alice", "test", "bob", 40 * HIVE_1_PERCENT, bob_post_key ); // edit 6
    generate_block();
    vote( "alice", "test", "bob", 30 * HIVE_1_PERCENT, bob_post_key ); // edit 7
    generate_block();
    vote( "alice", "test", "bob", 20 * HIVE_1_PERCENT, bob_post_key ); // edit 8
    generate_block();

    // wait a day for voting power of 'bob' to regenerate
    generate_blocks( HIVE_BLOCKS_PER_DAY );
    vote( "alice", "test", "bob", 75 * HIVE_1_PERCENT, bob_post_key ); // edit 9
    vote( "alice", "test", "carol", 75 * HIVE_1_PERCENT, carol_post_key ); // fresh vote of the same power

    const auto& comment = db->get_comment( "alice", std::string( "test" ) );
    const auto& comment_vote_idx = db->get_index< comment_vote_index, by_comment_voter >();
    const auto& vote_bob = *comment_vote_idx.find( boost::make_tuple( comment.get_id(), bob_id ) );
    const auto& vote_carol = *comment_vote_idx.find( boost::make_tuple( comment.get_id(), carol_id ) );
    BOOST_REQUIRE_EQUAL( vote_bob.get_rshares(), vote_carol.get_rshares() );

    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( artificial_1_on_power_down )
{
  try
  {
    //ABW: once problem with artificial 1 on vesting withdraw rate (with no power down) is fixed
    //this test won't make sense anymore and should be removed; before that happens comment out checks
    //after HF1 and make sure the rest of the test works without changes; note that it is possible to
    //run replay to detect safe time to activate "no op" checks before HF28, but I'm not sure it is
    //worth the time

    ACTORS_DEFAULT_FEE( (alice)(bob)(carol)(dave)(eric) );
    vest( "alice", ASSET( "1.000 TESTS" ) );
    vest( "bob", ASSET( "1.000 TESTS" ) );
    vest( "carol", ASSET( "1.000 TESTS" ) );
    vest( "dave", ASSET( "1.000 TESTS" ) );
    vest( "eric", ASSET( "1.000 TESTS" ) );
    generate_block();

    // vesting shares split puts artificial 1 on vesting withdraw rate on accounts with no power down
    inject_hardfork( HIVE_HARDFORK_0_1 );
    BOOST_REQUIRE_EQUAL( db->get_account( "alice" ).get_vesting_withdraw_rate().amount.value, 1 );
    BOOST_REQUIRE_EQUAL( db->get_account( "bob" ).get_vesting_withdraw_rate().amount.value, 1 );
    BOOST_REQUIRE_EQUAL( db->get_account( "carol" ).get_vesting_withdraw_rate().amount.value, 1 );
    BOOST_REQUIRE_EQUAL( db->get_account( "dave" ).get_vesting_withdraw_rate().amount.value, 1 );
    BOOST_REQUIRE_EQUAL( db->get_account( "eric" ).get_vesting_withdraw_rate().amount.value, 1 );

    // HF5 activates "no op" check during power down and cancel power down
    inject_hardfork( HIVE_HARDFORK_0_5 );

    // activate actual power down on 'alice' - it will clear effect of artificial 1 putting account into normal state
    withdraw_vesting( "alice", get_vesting( "alice" ), alice_private_key );
    generate_block();

    // 'alice' can cancel power down since she is no longer affected by the bug and actually is powering down
    withdraw_vesting( "alice", asset( 0, VESTS_SYMBOL ), alice_private_key );
    // 'bob' can cancel nonexisting power down due to bug
    withdraw_vesting( "bob", asset( 0, VESTS_SYMBOL ), bob_private_key );
    // 'eric' cannot change power down rate to 1 due to bug
    HIVE_REQUIRE_ASSERT( withdraw_vesting( "eric", asset( 1, VESTS_SYMBOL ), eric_private_key ), "account.vesting_withdraw_rate != new_vesting_withdraw_rate" );
    generate_block();

    // since 'alice' and 'bob' are now no longer affected by a bug, we can use them to check the power down with natural 1 withdraw rate
    BOOST_REQUIRE_EQUAL( db->get_account( "alice" ).get_vesting_withdraw_rate().amount.value, 0 );
    BOOST_REQUIRE_EQUAL( db->get_account( "bob" ).get_vesting_withdraw_rate().amount.value, 0 );

    withdraw_vesting( "alice", asset( HIVE_VESTING_WITHDRAW_INTERVALS_PRE_HF_16 - 1, VESTS_SYMBOL ), alice_private_key);
    // above power down truncates down to zero, that is corrected to 1
    BOOST_REQUIRE_EQUAL( db->get_account( "alice" ).get_vesting_withdraw_rate().amount.value, 1 );

    // HF16 switches from 104 weeks to 13 weeks of power down
    inject_hardfork( HIVE_HARDFORK_0_16 );

    withdraw_vesting( "bob", asset( HIVE_VESTING_WITHDRAW_INTERVALS * 2 - 1, VESTS_SYMBOL ), bob_private_key );
    BOOST_REQUIRE_EQUAL( db->get_account( "bob" ).get_vesting_withdraw_rate().amount.value, 1 );

    // make sure code behaves "properly" (I mean in unchanged way) right before HF28 too
    inject_hardfork( HIVE_HARDFORK_1_27 );
    // 'carol' can cancel nonexisting power down due to bug
    withdraw_vesting( "carol", asset( 0, VESTS_SYMBOL ), carol_private_key );
    // 'eric' still can't change rate to 1
    HIVE_REQUIRE_ASSERT( withdraw_vesting( "eric", asset( 1, VESTS_SYMBOL ), eric_private_key ), "account.vesting_withdraw_rate != new_vesting_withdraw_rate" );
    generate_block();

    // also since HF21 there is different rate correction mechanism, so the same operation as for 'bob'
    // before results in different rate
    withdraw_vesting( "carol", asset( HIVE_VESTING_WITHDRAW_INTERVALS * 2 - 1, VESTS_SYMBOL ), carol_private_key );
    BOOST_REQUIRE_EQUAL( db->get_account( "carol" ).get_vesting_withdraw_rate().amount.value, 2 );

    // HF28 activates code that prevents bug from affecting new power down cancels
    inject_hardfork( HIVE_HARDFORK_1_28 );

    // 'alice' and 'bob' have 1 in vesting withdraw rate, but they are actually performing power down
    // it means they can't change rate to 1, because it is already 1
    HIVE_REQUIRE_ASSERT( withdraw_vesting( "alice", asset( 1, VESTS_SYMBOL ), alice_private_key ),
      "account.vesting_withdraw_rate != new_vesting_withdraw_rate || !account.has_active_power_down()" );
    HIVE_REQUIRE_ASSERT( withdraw_vesting( "bob", asset( 1, VESTS_SYMBOL ), bob_private_key ),
      "account.vesting_withdraw_rate != new_vesting_withdraw_rate || !account.has_active_power_down()" );
    withdraw_vesting( "alice", asset( 0, VESTS_SYMBOL ), alice_private_key );
    withdraw_vesting( "bob", asset( 0, VESTS_SYMBOL ), bob_private_key );
    // 'carol' does not have 1 as power down rate, so she can change it to 1 or cancel power down
    withdraw_vesting( "carol", asset( 1, VESTS_SYMBOL ), carol_private_key );
    withdraw_vesting( "carol", asset( 0, VESTS_SYMBOL ), carol_private_key );
    // only 'dave' does not have power down despite having 1 as power down rate
    HIVE_REQUIRE_ASSERT( withdraw_vesting( "dave", asset( 0, VESTS_SYMBOL ), dave_private_key ), "account.has_active_power_down()" );
    // 'eric' can finally change rate to 1
    withdraw_vesting( "eric", asset( 1, VESTS_SYMBOL ), eric_private_key );

    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( vote_stabilization )
{
  try
  {
    autoscope reset( set_mainnet_cashout_values() );

    BOOST_TEST_MESSAGE( "Testing consecutive single block votes and downvotes before HF28" );

    inject_hardfork( HIVE_HARDFORK_1_27 );

    ACTORS_DEFAULT_FEE( (alice)(bob)(carol)(antibob)(anticarol) );
    vest( "alice", ASSET( "1000.000 TESTS" ) );
    vest( "bob", ASSET( "1000.000 TESTS" ) );
    vest( "carol", ASSET( "1000.000 TESTS" ) );
    vest( "antibob", ASSET( "1000.000 TESTS" ) );
    vest( "anticarol", ASSET( "1000.000 TESTS" ) );
    generate_block();

    // create root post
    post_comment( "alice", "test", "test", "test", "category", alice_post_key );
    generate_block();

    int i;
    // create replies (there is a cooldown on root posts, replies can be posted block by block)
    for( i = 0; i < 200; ++i )
    {
      auto permlink = "reply" + std::to_string( i );
      post_comment_to_comment( "alice", permlink, "reply", permlink, "alice", "test", alice_post_key );
      generate_block();
    }

    const auto& vote_idx = db->get_index< comment_vote_index, by_comment_voter >();

    auto vote_reply = [&]( int i, int16_t weight, const std::string& voter, const fc::ecc::private_key& key ) -> int64_t
    {
      auto permlink = "reply" + std::to_string( i );
      vote( "alice", permlink, voter, weight, key );
      auto reply_id = db->get_comment( "alice", permlink ).get_id();
      const auto& vote_obj = *vote_idx.find( boost::make_tuple( reply_id, get_account_id( voter ) ) );
      return vote_obj.get_rshares();
    };

    // upvote all replies with 'bob'
    int64_t full_power = 0, last_power = 0;
    for( i = 0; i < 200; ++i )
    {
      auto rshares = vote_reply( i, HIVE_100_PERCENT, "bob", bob_post_key );
      if( !full_power )
        full_power = rshares;
      else
        BOOST_REQUIRE_LT( rshares, last_power ); // power drops with every vote, but never reaches zero (at least not within 200 votes)
      last_power = rshares;
      ilog( "${i}: upvote with ${p} BP of full power", ( i )( "p", last_power * HIVE_100_PERCENT / full_power ) );
    }
    generate_block();

    // downvote all replies with 'antibob'
    for( i = 0; i < 13; ++i )
    {
      auto rshares = vote_reply( i, - HIVE_100_PERCENT, "antibob", antibob_post_key );
      BOOST_REQUIRE_EQUAL( -rshares, full_power ); // since antibob has full upvote mana, downvotes will be of equal power until he starts consuming upvote mana
    }
    last_power = full_power;
    for( ; i < 200; ++i )
    {
      auto rshares = vote_reply( i, -HIVE_100_PERCENT, "antibob", antibob_post_key );
      BOOST_REQUIRE_LT( -rshares, last_power ); // now power drops with every downvote, because all downvote mana was consumed
      last_power = -rshares;
      ilog( "${i}: downvote with ${p} BP of full power", ( i )( "p", last_power * HIVE_100_PERCENT / full_power ) );
    }
    generate_block();

    BOOST_TEST_MESSAGE( "Testing consecutive single block votes and downvotes after HF28" );

    inject_hardfork( HIVE_BLOCKCHAIN_VERSION.minor_v() );

    // upvote replies with 'carol' - she can only cast 49 full powered votes before her mana
    // is gone
    // NOTE: due to "round-up" code a tiny bit more mana is used, therefore when all votes are
    // in single block, 50th one will be a tiny bit short on mana
    for( i = 0; i < 49; ++i )
    {
      auto rshares = vote_reply( i, HIVE_100_PERCENT, "carol", carol_post_key );
      BOOST_REQUIRE_EQUAL( rshares, full_power ); // power stays the same on all votes
    }
    HIVE_REQUIRE_ASSERT( vote_reply( i, HIVE_100_PERCENT, "carol", carol_post_key ),
      "voter.get_voting_manabar().has_mana( fc::uint128_to_int64( used_mana ) )" );

    generate_block();
    // after single block 'carol' should regenerate enough mana to counter "round-up" mentioned above
    vote_reply( i, HIVE_100_PERCENT, "carol", carol_post_key );
    // but further voting is not possible, unless she waits or lowers weight significantly
    ++i;
    HIVE_REQUIRE_ASSERT( vote_reply( i, HIVE_100_PERCENT, "carol", carol_post_key ),
      "voter.get_voting_manabar().has_mana( fc::uint128_to_int64( used_mana ) )" );

    generate_blocks( HIVE_BLOCKS_PER_DAY / 10 - 1 );
    HIVE_REQUIRE_ASSERT( vote_reply( i, HIVE_100_PERCENT, "carol", carol_post_key ),
      "voter.get_voting_manabar().has_mana( fc::uint128_to_int64( used_mana ) )" );
    generate_block();
    vote_reply( i, HIVE_100_PERCENT, "carol", carol_post_key );
    // only one full vote is possible after 1/10 of day of mana regen
    ++i;
    HIVE_REQUIRE_ASSERT( vote_reply( i, HIVE_100_PERCENT, "carol", carol_post_key ),
      "voter.get_voting_manabar().has_mana( fc::uint128_to_int64( used_mana ) )" );

    // downvote all replies with 'anticarol'
    for( i = 0; i < 12+1+49; ++i )
    {
      auto rshares = vote_reply( i, -HIVE_100_PERCENT, "anticarol", anticarol_post_key );
      BOOST_REQUIRE_EQUAL( -rshares, full_power ); // downvote power no longer relies on upvote manabar
    }
    // 13'th downvote started to eat upvote mana, eating half-vote worth, however due to "round-up" code
    // mentioned earlier, it will come couple points short
    HIVE_REQUIRE_ASSERT( vote_reply( i, -50 * HIVE_1_PERCENT, "anticarol", anticarol_post_key ),
      "voter.get_voting_manabar().current_mana + voter.get_downvote_manabar().current_mana > fc::uint128_to_int64( used_mana )" );
    generate_block();
    // after single block 'anticarol' should regenerate enough mana to counter "round-up" mentioned above
    vote_reply( i, -50 * HIVE_1_PERCENT, "anticarol", anticarol_post_key );
    // but further downvoting is not possible, unless she waits or lowers weight significantly
    ++i;
    HIVE_REQUIRE_ASSERT( vote_reply( i, -HIVE_100_PERCENT, "anticarol", anticarol_post_key ),
      "voter.get_voting_manabar().current_mana + voter.get_downvote_manabar().current_mana > fc::uint128_to_int64( used_mana )" );

    generate_blocks( HIVE_BLOCKS_PER_DAY * 10 / 125 - 1 );
    // since downvote can burn both downvote and upvote mana at the same time and they regenerate concurrently,
    // with downvote manabar being 1/4 of upvote, we need to wait less to be able to cast next downvote
    HIVE_REQUIRE_ASSERT( vote_reply( i, -HIVE_100_PERCENT, "anticarol", anticarol_post_key ),
      "voter.get_voting_manabar().current_mana + voter.get_downvote_manabar().current_mana > fc::uint128_to_int64( used_mana )" );
    generate_block();
    vote_reply( i, -HIVE_100_PERCENT, "anticarol", anticarol_post_key );
    // only one full downvote is possible after (1/10 of day / 125%) of mana regen
    ++i;
    HIVE_REQUIRE_ASSERT( vote_reply( i, -HIVE_100_PERCENT, "anticarol", anticarol_post_key ),
      "voter.get_voting_manabar().current_mana + voter.get_downvote_manabar().current_mana > fc::uint128_to_int64( used_mana )" );

    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( empty_voting )
{
  try
  {
    autoscope reset( set_mainnet_cashout_values() );

    BOOST_TEST_MESSAGE( "Testing voting and downvoting with no mana before HF28" );

    inject_hardfork( HIVE_HARDFORK_1_27 );

    ACTORS_DEFAULT_FEE( (alice)(bob)(carol)(antibob)(anticarol) );
    vest( "alice", ASSET( "1000.000 TESTS" ) );
    generate_block();

    // create post
    post_comment( "alice", "test", "test", "test", "category", alice_post_key );
    generate_block();

    // voting requires exactly as much mana as is used - zero in this case since bob has no stake
    vote( "alice", "test", "bob", HIVE_100_PERCENT, bob_post_key );
    // downvoting requires more than is used - it is probably a bug
    HIVE_REQUIRE_ASSERT( vote( "alice", "test", "antibob", -HIVE_100_PERCENT, antibob_post_key ),
      "voter.get_voting_manabar().current_mana + voter.get_downvote_manabar().current_mana > fc::uint128_to_int64( used_mana )" );
    generate_block();

    BOOST_TEST_MESSAGE( "Testing voting and downvoting with no mana after HF28" );

    inject_hardfork( HIVE_BLOCKCHAIN_VERSION.minor_v() );

    vote( "alice", "test", "carol", HIVE_100_PERCENT, carol_post_key );
    // even though it is probably a bug and it was actually fixed briefly, final decision was to not change it
    HIVE_REQUIRE_ASSERT( vote( "alice", "test", "anticarol", -HIVE_100_PERCENT, anticarol_post_key ),
      "voter.get_voting_manabar().current_mana + voter.get_downvote_manabar().current_mana > fc::uint128_to_int64( used_mana )" );
    generate_block();

    auto post_id = db->get_comment( "alice", std::string( "test" ) ).get_id();
    const auto& vote_idx = db->get_index< comment_vote_index, by_comment_voter >();
    auto voteI = vote_idx.find( boost::make_tuple( post_id, bob_id ) );
    BOOST_REQUIRE( voteI != vote_idx.end() );
    BOOST_REQUIRE_EQUAL( voteI->get_rshares(), 0 );
    BOOST_REQUIRE_EQUAL( voteI->get_vote_percent(), HIVE_100_PERCENT );
    voteI = vote_idx.find( boost::make_tuple( post_id, antibob_id ) );
    BOOST_REQUIRE( voteI == vote_idx.end() );
    voteI = vote_idx.find( boost::make_tuple( post_id, carol_id ) );
    BOOST_REQUIRE( voteI != vote_idx.end() );
    BOOST_REQUIRE_EQUAL( voteI->get_rshares(), 0 );
    BOOST_REQUIRE_EQUAL( voteI->get_vote_percent(), HIVE_100_PERCENT );
    voteI = vote_idx.find( boost::make_tuple( post_id, anticarol_id ) );
    BOOST_REQUIRE( voteI == vote_idx.end() );

    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( genesis_account_authorities )
{
  try
  {
    // tests fix for missing authorities on steem.dao and hive.fund accounts in testnet;
    // before fix they would fail on access to authority object, but only until HF21/HF24
    HIVE_REQUIRE_ASSERT( transfer( OBSOLETE_TREASURY_ACCOUNT, NEW_HIVE_TREASURY_ACCOUNT, ASSET( "0.001 TBD" ), "", init_account_priv_key ),
      "s.check_authority( id ) || check_with_role_upgrade()" );
    HIVE_REQUIRE_ASSERT( transfer( NEW_HIVE_TREASURY_ACCOUNT, OBSOLETE_TREASURY_ACCOUNT, ASSET( "0.001 TBD" ), "", init_account_priv_key ),
      "s.check_authority( id ) || check_with_role_upgrade()" );

    inject_hardfork( HIVE_HARDFORK_0_21 );

    HIVE_REQUIRE_ASSERT( transfer( OBSOLETE_TREASURY_ACCOUNT, NEW_HIVE_TREASURY_ACCOUNT, ASSET( "0.001 TBD" ), "", init_account_priv_key ),
      "s.check_authority( id ) || check_with_role_upgrade()" );

    inject_hardfork( HIVE_HARDFORK_1_24 );

    HIVE_REQUIRE_ASSERT( transfer( NEW_HIVE_TREASURY_ACCOUNT, OBSOLETE_TREASURY_ACCOUNT, ASSET( "0.001 TBD" ), "", init_account_priv_key ),
      "s.check_authority( id ) || check_with_role_upgrade()" );

    inject_hardfork( HIVE_BLOCKCHAIN_VERSION.minor_v() );

    HIVE_REQUIRE_ASSERT( transfer( OBSOLETE_TREASURY_ACCOUNT, NEW_HIVE_TREASURY_ACCOUNT, ASSET( "0.001 TBD" ), "", init_account_priv_key ),
      "s.check_authority( id )" );
    HIVE_REQUIRE_ASSERT( transfer( NEW_HIVE_TREASURY_ACCOUNT, OBSOLETE_TREASURY_ACCOUNT, ASSET( "0.001 TBD" ), "", init_account_priv_key ),
      "s.check_authority( id )" );
  }
  FC_LOG_AND_RETHROW()
}

void set_witness_properties( database_fixture& fixture, uint32_t block_size, uint16_t hbd_apr, const asset& fee )
{
  decltype( witness_set_properties_operation::props ) props;
  props[ "key" ] = fc::raw::pack_to_vector( fixture.init_account_pub_key );
  props[ "maximum_block_size" ] = fc::raw::pack_to_vector( block_size );
  props[ "hbd_interest_rate" ] = fc::raw::pack_to_vector( hbd_apr );
  props[ "account_creation_fee" ] = fc::raw::pack_to_vector( fee );
  fixture.set_witness_props( props, false );
  ilog( "#${b} setting new witness values: maximum_block_size = ${s}, hbd_interest_rate = ${h}, account_creation_fee = ${f}",
    ( "b", fixture.db->head_block_num() )( "s", block_size )( "h", hbd_apr )( "f", legacy_asset( fee ).to_string() ) );
};

const uint32_t default_block_size = HIVE_MIN_BLOCK_SIZE_LIMIT * 2;
static_assert( default_block_size != HIVE_MAX_BLOCK_SIZE );
const uint16_t default_hbd_apr = HIVE_DEFAULT_HBD_INTEREST_RATE;
static_assert( default_hbd_apr != 0 );
const asset default_fee = HIVE_asset( HIVE_MIN_ACCOUNT_CREATION_FEE );

BOOST_AUTO_TEST_CASE( global_witness_props_basic_test )
{
  try
  {
    inject_hardfork( HIVE_HARDFORK_1_27 );

    const auto& wso = db->get_witness_schedule_object();
    const auto& future_wso = db->get_future_witness_schedule_object();
    const auto& dgpo = db->get_dynamic_global_properties();
    const auto& witness = db->get_witness( HIVE_INIT_MINER_NAME );

    // default global block size and interest rate are initially different than default wso value and witness value
    BOOST_REQUIRE_EQUAL( dgpo.maximum_block_size, HIVE_MAX_BLOCK_SIZE );
    BOOST_REQUIRE_EQUAL( wso.median_props.maximum_block_size, default_block_size );
    BOOST_REQUIRE_EQUAL( future_wso.median_props.maximum_block_size, default_block_size );
    BOOST_REQUIRE_EQUAL( witness.props.maximum_block_size, default_block_size );

    BOOST_REQUIRE_EQUAL( dgpo.hbd_interest_rate, 0 );
    BOOST_REQUIRE_EQUAL( wso.median_props.hbd_interest_rate, default_hbd_apr );
    BOOST_REQUIRE_EQUAL( future_wso.median_props.hbd_interest_rate, default_hbd_apr );
    BOOST_REQUIRE_EQUAL( witness.props.hbd_interest_rate, default_hbd_apr );

    BOOST_REQUIRE_EQUAL( wso.median_props.account_creation_fee.amount.value, default_fee.amount.value );
    BOOST_REQUIRE_EQUAL( future_wso.median_props.account_creation_fee.amount.value, default_fee.amount.value );
    BOOST_REQUIRE_EQUAL( witness.props.account_creation_fee.amount.value, default_fee.amount.value );

    // generate three schedules worth of blocks so we know all the mechanisms had a chance to activate
    // (that is, default global properties that are used in first schedule, default witness schedule
    // which values are used in second schedule and future witness schedule which values are built from
    // from default witness values)
    generate_blocks( db->head_block_time() + 63 * HIVE_BLOCK_INTERVAL, false );

    // the global values should propagate all the way from witness(es)
    BOOST_REQUIRE_EQUAL( witness.props.maximum_block_size, dgpo.maximum_block_size );
    BOOST_REQUIRE_EQUAL( witness.props.hbd_interest_rate, dgpo.hbd_interest_rate );

    // change witness properties
    uint32_t new_block_size1 = HIVE_MIN_BLOCK_SIZE_LIMIT * 3;
    uint16_t new_hbd_apr1 = HIVE_DEFAULT_HBD_INTEREST_RATE * 2;
    asset new_fee1 = ASSET( "1.500 TESTS" );
    set_witness_properties( *this, new_block_size1, new_hbd_apr1, new_fee1 );

    // run one schedule so changed properties are included in future wso
    generate_blocks( db->head_block_time() + 21 * HIVE_BLOCK_INTERVAL, false );

    // witness has new values
    BOOST_REQUIRE_EQUAL( witness.props.maximum_block_size, new_block_size1 );
    BOOST_REQUIRE_EQUAL( witness.props.hbd_interest_rate, new_hbd_apr1 );
    BOOST_REQUIRE_EQUAL( witness.props.account_creation_fee.amount.value, new_fee1.amount.value );

    // future wso has new values
    BOOST_REQUIRE_EQUAL( future_wso.median_props.maximum_block_size, new_block_size1 );
    BOOST_REQUIRE_EQUAL( future_wso.median_props.hbd_interest_rate, new_hbd_apr1 );
    BOOST_REQUIRE_EQUAL( future_wso.median_props.account_creation_fee.amount.value, new_fee1.amount.value );

    // wso still has old values
    BOOST_REQUIRE_EQUAL( wso.median_props.maximum_block_size, default_block_size );
    BOOST_REQUIRE_EQUAL( wso.median_props.hbd_interest_rate, default_hbd_apr );
    BOOST_REQUIRE_EQUAL( wso.median_props.account_creation_fee.amount.value, default_fee.amount.value );

    // dgpo incorrectly has new values, because they were activated when future wso was filled
    BOOST_REQUIRE_EQUAL( dgpo.maximum_block_size, new_block_size1 );
    BOOST_REQUIRE_EQUAL( dgpo.hbd_interest_rate, new_hbd_apr1 );

    // run one schedule to make the state consistent (future wso activated)
    generate_blocks( db->head_block_time() + 21 * HIVE_BLOCK_INTERVAL, false );

    inject_hardfork( HIVE_BLOCKCHAIN_VERSION.minor_v() );

    // all objects have new values
    BOOST_REQUIRE_EQUAL( witness.props.maximum_block_size, new_block_size1 );
    BOOST_REQUIRE_EQUAL( witness.props.hbd_interest_rate, new_hbd_apr1 );
    BOOST_REQUIRE_EQUAL( witness.props.account_creation_fee.amount.value, new_fee1.amount.value );
    BOOST_REQUIRE_EQUAL( future_wso.median_props.maximum_block_size, new_block_size1 );
    BOOST_REQUIRE_EQUAL( future_wso.median_props.hbd_interest_rate, new_hbd_apr1 );
    BOOST_REQUIRE_EQUAL( future_wso.median_props.account_creation_fee.amount.value, new_fee1.amount.value );
    BOOST_REQUIRE_EQUAL( wso.median_props.maximum_block_size, new_block_size1 );
    BOOST_REQUIRE_EQUAL( wso.median_props.hbd_interest_rate, new_hbd_apr1 );
    BOOST_REQUIRE_EQUAL( wso.median_props.account_creation_fee.amount.value, new_fee1.amount.value );
    BOOST_REQUIRE_EQUAL( dgpo.maximum_block_size, new_block_size1 );
    BOOST_REQUIRE_EQUAL( dgpo.hbd_interest_rate, new_hbd_apr1 );

    // change witness properties again
    uint32_t new_block_size2 = HIVE_MIN_BLOCK_SIZE_LIMIT * 5;
    uint16_t new_hbd_apr2 = HIVE_DEFAULT_HBD_INTEREST_RATE * 4;
    asset new_fee2 = ASSET( "2.500 TESTS" );
    set_witness_properties( *this, new_block_size2, new_hbd_apr2, new_fee2 );

    // run one schedule so changed properties are included in future wso
    generate_blocks( db->head_block_time() + 21 * HIVE_BLOCK_INTERVAL, false );

    // witness has new values
    BOOST_REQUIRE_EQUAL( witness.props.maximum_block_size, new_block_size2 );
    BOOST_REQUIRE_EQUAL( witness.props.hbd_interest_rate, new_hbd_apr2 );
    BOOST_REQUIRE_EQUAL( witness.props.account_creation_fee.amount.value, new_fee2.amount.value );

    // future wso has new values
    BOOST_REQUIRE_EQUAL( future_wso.median_props.maximum_block_size, new_block_size2 );
    BOOST_REQUIRE_EQUAL( future_wso.median_props.hbd_interest_rate, new_hbd_apr2 );
    BOOST_REQUIRE_EQUAL( future_wso.median_props.account_creation_fee.amount.value, new_fee2.amount.value );

    // wso still has previous values
    BOOST_REQUIRE_EQUAL( wso.median_props.maximum_block_size, new_block_size1 );
    BOOST_REQUIRE_EQUAL( wso.median_props.hbd_interest_rate, new_hbd_apr1 );
    BOOST_REQUIRE_EQUAL( wso.median_props.account_creation_fee.amount.value, new_fee1.amount.value );

    // dgpo also has previous values
    BOOST_REQUIRE_EQUAL( dgpo.maximum_block_size, new_block_size1 );
    BOOST_REQUIRE_EQUAL( dgpo.hbd_interest_rate, new_hbd_apr1 );

    // run one schedule so changed properties are propagated to wso and activated
    generate_blocks( db->head_block_time() + 21 * HIVE_BLOCK_INTERVAL, false );

    // all objects have new values
    BOOST_REQUIRE_EQUAL( witness.props.maximum_block_size, new_block_size2 );
    BOOST_REQUIRE_EQUAL( witness.props.hbd_interest_rate, new_hbd_apr2 );
    BOOST_REQUIRE_EQUAL( witness.props.account_creation_fee.amount.value, new_fee2.amount.value );
    BOOST_REQUIRE_EQUAL( future_wso.median_props.maximum_block_size, new_block_size2 );
    BOOST_REQUIRE_EQUAL( future_wso.median_props.hbd_interest_rate, new_hbd_apr2 );
    BOOST_REQUIRE_EQUAL( future_wso.median_props.account_creation_fee.amount.value, new_fee2.amount.value );
    BOOST_REQUIRE_EQUAL( wso.median_props.maximum_block_size, new_block_size2 );
    BOOST_REQUIRE_EQUAL( wso.median_props.hbd_interest_rate, new_hbd_apr2 );
    BOOST_REQUIRE_EQUAL( wso.median_props.account_creation_fee.amount.value, new_fee2.amount.value );
    BOOST_REQUIRE_EQUAL( dgpo.maximum_block_size, new_block_size2 );
    BOOST_REQUIRE_EQUAL( dgpo.hbd_interest_rate, new_hbd_apr2 );
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( global_witness_props_change_noticed_after_hf_test )
{
  try
  {
    // this test changes witness values before hardfork, but first schedule update happens after

    inject_hardfork( HIVE_HARDFORK_1_27 );

    const auto& wso = db->get_witness_schedule_object();
    const auto& future_wso = db->get_future_witness_schedule_object();
    const auto& dgpo = db->get_dynamic_global_properties();
    const auto& witness = db->get_witness( HIVE_INIT_MINER_NAME );

    generate_blocks( db->head_block_time() + 63 * HIVE_BLOCK_INTERVAL, false );

    // run to the start of nearest schedule plus couple blocks
    uint32_t block_num = db->head_block_num();
    uint32_t block_num_of_change = ( block_num - 5 + 20 ) / 21 * 21 + 5;
    while( block_num < block_num_of_change )
    {
      generate_block();
      ++block_num;
    }

    // change witness properties
    uint32_t new_block_size = HIVE_MIN_BLOCK_SIZE_LIMIT * 3;
    uint16_t new_hbd_apr = HIVE_DEFAULT_HBD_INTEREST_RATE * 2;
    asset new_fee = ASSET( "1.500 TESTS" );
    set_witness_properties( *this, new_block_size, new_hbd_apr, new_fee );

    generate_block();

    // values only changed on witness
    BOOST_REQUIRE_EQUAL( witness.props.maximum_block_size, new_block_size );
    BOOST_REQUIRE_EQUAL( witness.props.hbd_interest_rate, new_hbd_apr );
    BOOST_REQUIRE_EQUAL( witness.props.account_creation_fee.amount.value, new_fee.amount.value );

    BOOST_REQUIRE_EQUAL( future_wso.median_props.maximum_block_size, default_block_size );
    BOOST_REQUIRE_EQUAL( future_wso.median_props.hbd_interest_rate, default_hbd_apr );
    BOOST_REQUIRE_EQUAL( future_wso.median_props.account_creation_fee.amount.value, default_fee.amount.value );
    BOOST_REQUIRE_EQUAL( wso.median_props.maximum_block_size, default_block_size );
    BOOST_REQUIRE_EQUAL( wso.median_props.hbd_interest_rate, default_hbd_apr );
    BOOST_REQUIRE_EQUAL( wso.median_props.account_creation_fee.amount.value, default_fee.amount.value );
    BOOST_REQUIRE_EQUAL( dgpo.maximum_block_size, default_block_size );
    BOOST_REQUIRE_EQUAL( dgpo.hbd_interest_rate, default_hbd_apr );

    inject_hardfork( HIVE_BLOCKCHAIN_VERSION.minor_v() );

    // new values still only on witness
    BOOST_REQUIRE_EQUAL( witness.props.maximum_block_size, new_block_size );
    BOOST_REQUIRE_EQUAL( witness.props.hbd_interest_rate, new_hbd_apr );
    BOOST_REQUIRE_EQUAL( witness.props.account_creation_fee.amount.value, new_fee.amount.value );

    BOOST_REQUIRE_EQUAL( future_wso.median_props.maximum_block_size, default_block_size );
    BOOST_REQUIRE_EQUAL( future_wso.median_props.hbd_interest_rate, default_hbd_apr );
    BOOST_REQUIRE_EQUAL( future_wso.median_props.account_creation_fee.amount.value, default_fee.amount.value );
    BOOST_REQUIRE_EQUAL( wso.median_props.maximum_block_size, default_block_size );
    BOOST_REQUIRE_EQUAL( wso.median_props.hbd_interest_rate, default_hbd_apr );
    BOOST_REQUIRE_EQUAL( wso.median_props.account_creation_fee.amount.value, default_fee.amount.value );
    BOOST_REQUIRE_EQUAL( dgpo.maximum_block_size, default_block_size );
    BOOST_REQUIRE_EQUAL( dgpo.hbd_interest_rate, default_hbd_apr );

    // run one schedule
    generate_blocks( db->head_block_time() + 21 * HIVE_BLOCK_INTERVAL, false );

    // new values are now on witness and future wso, but not in active wso nor in dgpo (HF28 behavior)
    BOOST_REQUIRE_EQUAL( witness.props.maximum_block_size, new_block_size );
    BOOST_REQUIRE_EQUAL( witness.props.hbd_interest_rate, new_hbd_apr );
    BOOST_REQUIRE_EQUAL( witness.props.account_creation_fee.amount.value, new_fee.amount.value );
    BOOST_REQUIRE_EQUAL( future_wso.median_props.maximum_block_size, new_block_size );
    BOOST_REQUIRE_EQUAL( future_wso.median_props.hbd_interest_rate, new_hbd_apr );
    BOOST_REQUIRE_EQUAL( future_wso.median_props.account_creation_fee.amount.value, new_fee.amount.value );

    BOOST_REQUIRE_EQUAL( dgpo.maximum_block_size, default_block_size );
    BOOST_REQUIRE_EQUAL( dgpo.hbd_interest_rate, default_hbd_apr );
    BOOST_REQUIRE_EQUAL( wso.median_props.maximum_block_size, default_block_size );
    BOOST_REQUIRE_EQUAL( wso.median_props.hbd_interest_rate, default_hbd_apr );
    BOOST_REQUIRE_EQUAL( wso.median_props.account_creation_fee.amount.value, default_fee.amount.value );
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( global_witness_props_change_applied_after_hf_test )
{
  try
  {
    // this test changes witness values before hardfork and future schedule is also built with them
    // before hardfork, but they become officially active after

    inject_hardfork( HIVE_HARDFORK_1_27 );

    const auto& wso = db->get_witness_schedule_object();
    const auto& future_wso = db->get_future_witness_schedule_object();
    const auto& dgpo = db->get_dynamic_global_properties();
    const auto& witness = db->get_witness( HIVE_INIT_MINER_NAME );

    generate_blocks( db->head_block_time() + 63 * HIVE_BLOCK_INTERVAL, false );

    // run to the start of nearest schedule plus couple blocks
    uint32_t block_num = db->head_block_num();
    uint32_t block_num_of_change = ( block_num - 5 + 20 ) / 21 * 21 + 5;
    while( block_num < block_num_of_change )
    {
      generate_block();
      ++block_num;
    }

    // change witness properties
    uint32_t new_block_size1 = HIVE_MIN_BLOCK_SIZE_LIMIT * 3;
    uint16_t new_hbd_apr1 = HIVE_DEFAULT_HBD_INTEREST_RATE * 2;
    asset new_fee1 = ASSET( "1.500 TESTS" );
    set_witness_properties( *this, new_block_size1, new_hbd_apr1, new_fee1 );

    generate_block();

    // run one schedule
    generate_blocks( db->head_block_time() + 21 * HIVE_BLOCK_INTERVAL, false );

    // values changed on witness and future wso, but also copied to dgpo (old behavior)
    BOOST_REQUIRE_EQUAL( witness.props.maximum_block_size, new_block_size1 );
    BOOST_REQUIRE_EQUAL( witness.props.hbd_interest_rate, new_hbd_apr1 );
    BOOST_REQUIRE_EQUAL( witness.props.account_creation_fee.amount.value, new_fee1.amount.value );
    BOOST_REQUIRE_EQUAL( future_wso.median_props.maximum_block_size, new_block_size1 );
    BOOST_REQUIRE_EQUAL( future_wso.median_props.hbd_interest_rate, new_hbd_apr1 );
    BOOST_REQUIRE_EQUAL( future_wso.median_props.account_creation_fee.amount.value, new_fee1.amount.value );
    BOOST_REQUIRE_EQUAL( dgpo.maximum_block_size, new_block_size1 );
    BOOST_REQUIRE_EQUAL( dgpo.hbd_interest_rate, new_hbd_apr1 );

    BOOST_REQUIRE_EQUAL( wso.median_props.maximum_block_size, default_block_size );
    BOOST_REQUIRE_EQUAL( wso.median_props.hbd_interest_rate, default_hbd_apr );
    BOOST_REQUIRE_EQUAL( wso.median_props.account_creation_fee.amount.value, default_fee.amount.value );

    inject_hardfork( HIVE_BLOCKCHAIN_VERSION.minor_v() );

    // activation of hardfork itself changes nothing
    BOOST_REQUIRE_EQUAL( witness.props.maximum_block_size, new_block_size1 );
    BOOST_REQUIRE_EQUAL( witness.props.hbd_interest_rate, new_hbd_apr1 );
    BOOST_REQUIRE_EQUAL( witness.props.account_creation_fee.amount.value, new_fee1.amount.value );
    BOOST_REQUIRE_EQUAL( future_wso.median_props.maximum_block_size, new_block_size1 );
    BOOST_REQUIRE_EQUAL( future_wso.median_props.hbd_interest_rate, new_hbd_apr1 );
    BOOST_REQUIRE_EQUAL( future_wso.median_props.account_creation_fee.amount.value, new_fee1.amount.value );
    BOOST_REQUIRE_EQUAL( dgpo.maximum_block_size, new_block_size1 );
    BOOST_REQUIRE_EQUAL( dgpo.hbd_interest_rate, new_hbd_apr1 );

    BOOST_REQUIRE_EQUAL( wso.median_props.maximum_block_size, default_block_size );
    BOOST_REQUIRE_EQUAL( wso.median_props.hbd_interest_rate, default_hbd_apr );
    BOOST_REQUIRE_EQUAL( wso.median_props.account_creation_fee.amount.value, default_fee.amount.value );

    // change witness properties again
    uint32_t new_block_size2 = HIVE_MIN_BLOCK_SIZE_LIMIT * 5;
    uint16_t new_hbd_apr2 = HIVE_DEFAULT_HBD_INTEREST_RATE * 4;
    asset new_fee2 = ASSET( "2.500 TESTS" );
    set_witness_properties( *this, new_block_size2, new_hbd_apr2, new_fee2 );

    // run one schedule again
    generate_blocks( db->head_block_time() + 21 * HIVE_BLOCK_INTERVAL, false );

    // values that were previously in future wso (and also copied to dgpo due to old behavior)
    // are now propagated to wso (and copied again to dgpo, although it changes nothing) while
    // newer values are only on witness and future wso (new behavior)
    BOOST_REQUIRE_EQUAL( witness.props.maximum_block_size, new_block_size2 );
    BOOST_REQUIRE_EQUAL( witness.props.hbd_interest_rate, new_hbd_apr2 );
    BOOST_REQUIRE_EQUAL( witness.props.account_creation_fee.amount.value, new_fee2.amount.value );
    BOOST_REQUIRE_EQUAL( future_wso.median_props.maximum_block_size, new_block_size2 );
    BOOST_REQUIRE_EQUAL( future_wso.median_props.hbd_interest_rate, new_hbd_apr2 );
    BOOST_REQUIRE_EQUAL( future_wso.median_props.account_creation_fee.amount.value, new_fee2.amount.value );

    BOOST_REQUIRE_EQUAL( dgpo.maximum_block_size, new_block_size1 );
    BOOST_REQUIRE_EQUAL( dgpo.hbd_interest_rate, new_hbd_apr1 );
    BOOST_REQUIRE_EQUAL( wso.median_props.maximum_block_size, new_block_size1 );
    BOOST_REQUIRE_EQUAL( wso.median_props.hbd_interest_rate, new_hbd_apr1 );
    BOOST_REQUIRE_EQUAL( wso.median_props.account_creation_fee.amount.value, new_fee1.amount.value );
  }
 
  FC_LOG_AND_RETHROW()
}


BOOST_AUTO_TEST_CASE( treasury_hbd_does_not_affect_inflation_basic )
{
  try
  {
    // Test inflation behavior across HF27 and HF28 with HBD issuance. This basic test just checks that inflation with HBD is higher than inflation without
    inject_hardfork( HIVE_HARDFORK_1_27 );
    set_price_feed( price( ASSET( "1.000 TBD" ), ASSET( "1.000 TESTS" ) ) );
    generate_block();

    BOOST_REQUIRE_EQUAL( db->has_hardfork( HIVE_HARDFORK_1_27 ), true );
    BOOST_REQUIRE_EQUAL( db->has_hardfork( HIVE_HARDFORK_1_28 ), false );

    // First inflation check at HF27
    const auto& props = db->get_dynamic_global_properties();
    auto before_virtual_supply = props.virtual_supply.amount;

    generate_blocks( db->head_block_time() + 50 * HIVE_BLOCK_INTERVAL, false );

    auto after_virtual_supply = props.virtual_supply.amount;

    auto initial_inflation = after_virtual_supply - before_virtual_supply;
    BOOST_REQUIRE( initial_inflation > 0 );

    before_virtual_supply = props.virtual_supply.amount;
    ISSUE_FUNDS( db->get_treasury_name(), ASSET( "5000000.000 TBD" ) );
    
    generate_blocks( db->head_block_time() + 50 * HIVE_BLOCK_INTERVAL, false );

    after_virtual_supply = props.virtual_supply.amount;

    auto inflation_with_hbd = after_virtual_supply - before_virtual_supply;
    BOOST_REQUIRE( inflation_with_hbd > initial_inflation );

    // Move to HF28 and check reduced inflation
    inject_hardfork( HIVE_HARDFORK_1_28 );
    BOOST_REQUIRE_EQUAL( db->has_hardfork( HIVE_HARDFORK_1_28 ), true );

    before_virtual_supply = props.virtual_supply.amount;
    // we wait 49 because inject_hardfork generates a block
    generate_blocks( db->head_block_time() + 49 * HIVE_BLOCK_INTERVAL, false );

    after_virtual_supply = props.virtual_supply.amount;

    auto inflation_after_hf28 = after_virtual_supply - before_virtual_supply;
    BOOST_REQUIRE( inflation_after_hf28 < inflation_with_hbd );

    validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE(treasury_hbd_does_not_affect_inflation_advanced)
{
      // Test inflation behavior across HF27 and HF28 with HBD issuance. This checks exact values with the same algo as in process_funds
    try
    {
        auto calculate_current_inflation_rate = [&]() -> int64_t
        {
            int64_t start_inflation_rate = int64_t(HIVE_INFLATION_RATE_START_PERCENT);
            int64_t inflation_rate_adjustment = int64_t(db->head_block_num() / HIVE_INFLATION_NARROWING_PERIOD);
            int64_t inflation_rate_floor = int64_t(HIVE_INFLATION_RATE_STOP_PERCENT);

            return std::max(start_inflation_rate - inflation_rate_adjustment, inflation_rate_floor);
        };

        auto calculate_expected_new_virtual_supply = [&]() -> share_type
        {
            const auto& props = db->get_dynamic_global_properties();
            const auto& wso = db->get_witness_schedule_object();
            const auto& cwit = db->get_witness(props.current_witness);

            int64_t current_inflation_rate = calculate_current_inflation_rate();
            auto new_hive = (props.virtual_supply.amount * current_inflation_rate) / (int64_t(HIVE_100_PERCENT) * int64_t(HIVE_BLOCKS_PER_YEAR));
            if (db->has_hardfork(HIVE_HARDFORK_1_28_NO_DHF_HBD_IN_INFLATION)) {
              const auto &treasury_account = db->get_treasury();
              const auto hbd_supply_without_treasury = (props.get_current_hbd_supply() - treasury_account.get_hbd_balance()).amount < 0 ? asset(0, HBD_SYMBOL) : (props.get_current_hbd_supply() - treasury_account.get_hbd_balance());
              const auto virtual_supply_without_treasury = hbd_supply_without_treasury * db->get_feed_history().current_median_history + props.current_supply;

              new_hive = (virtual_supply_without_treasury.amount * current_inflation_rate) / (int64_t(HIVE_100_PERCENT) * int64_t(HIVE_BLOCKS_PER_YEAR));
            }

            auto content_reward = (new_hive * props.content_reward_percent) / HIVE_100_PERCENT;

            const auto& reward_idx = db->get_index<reward_fund_index, by_id>();
            share_type used_rewards = 0;
            for (auto itr = reward_idx.begin(); itr != reward_idx.end(); ++itr)
            {
                used_rewards += (content_reward * itr->percent_content_rewards) / HIVE_100_PERCENT;
            }

            content_reward = used_rewards;

            auto vesting_reward = (new_hive * props.vesting_reward_percent) / HIVE_100_PERCENT;
            auto dhf_new_funds = (new_hive * props.proposal_fund_percent) / HIVE_100_PERCENT;
            auto witness_reward = new_hive - content_reward - vesting_reward - dhf_new_funds;

            witness_reward *= HIVE_MAX_WITNESSES;

            if (cwit.schedule == witness_object::timeshare)
                witness_reward *= wso.timeshare_weight;
            else if (cwit.schedule == witness_object::miner)
                witness_reward *= wso.miner_weight;
            else if (cwit.schedule == witness_object::elected)
                witness_reward *= wso.elected_weight;

            witness_reward /= wso.witness_pay_normalization_factor;

            return content_reward + vesting_reward + witness_reward + dhf_new_funds;
        };

        const auto& props = db->get_dynamic_global_properties();

        inject_hardfork(HIVE_HARDFORK_1_27);
        set_price_feed(price(ASSET("1.000 TBD"), ASSET("1.000 TESTS")));
        generate_block();
        ISSUE_FUNDS(db->get_treasury_name(), ASSET("50000000.000 TBD"));

        auto before_virtual_supply = props.virtual_supply.amount;
        auto expected_new_virtual_supply = calculate_expected_new_virtual_supply();

        // check hf28 inflation
        generate_block();
        auto actual_new_virtual_supply = props.virtual_supply.amount - before_virtual_supply;
        BOOST_REQUIRE(expected_new_virtual_supply == actual_new_virtual_supply);

        inject_hardfork(HIVE_HARDFORK_1_28);
        generate_blocks( db->head_block_time() + 20 * HIVE_BLOCK_INTERVAL, false );

        before_virtual_supply = props.virtual_supply.amount;
        expected_new_virtual_supply = calculate_expected_new_virtual_supply();

        generate_block();
        actual_new_virtual_supply = props.virtual_supply.amount - before_virtual_supply;

        BOOST_REQUIRE(expected_new_virtual_supply == actual_new_virtual_supply);

        validate_database();
    }
    FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_SUITE_END()

#endif
