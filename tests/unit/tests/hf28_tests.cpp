#ifdef IS_TEST_NET

#include <boost/test/unit_test.hpp>

#include <hive/chain/hive_objects.hpp>
#include <hive/chain/dhf_objects.hpp>
#include <hive/chain/account_object.hpp>

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
        BOOST_TEST_MESSAGE( "Testing: 'declined_voting_rights_operation'" );
        BOOST_REQUIRE_EQUAL( (bool)executor, true );

        ACTORS_EXT( (*executor), (alice)(bob) );
        executor->fund( "alice", 1000 );
        executor->fund( "bob", 1000 );
        executor->vest( "alice", 1000 );
        executor->vest( "bob", 1000 );

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

      dhf_database::create_proposal_data cpd( executor->db->head_block_time() );

      BOOST_TEST_MESSAGE( "Create 'create_proposal_operation'" );
      int64_t _id_proposal = executor->create_proposal( cpd.creator, cpd.receiver, cpd.start_date, cpd.end_date, cpd.daily_pay, alice_private_key, false/*with_block_generation*/ );

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

      dhf_database::create_proposal_data cpd( executor->db->head_block_time() );

      BOOST_TEST_MESSAGE( "Create 'create_proposal_operation'" );
      int64_t _id_proposal = executor->create_proposal( cpd.creator, cpd.receiver, cpd.start_date, cpd.end_date, cpd.daily_pay, alice_private_key, false/*with_block_generation*/ );

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
          executor->db->create< account_object >( account );
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
        executor->fund( actor.name, 100000 );
        executor->fund( actor.name, ASSET( "200.000 TBD" ) );
        executor->vest( actor.name, 100000 );
      }
      executor->generate_block();

      dhf_database::create_proposal_data cpd( executor->db->head_block_time() );

      BOOST_TEST_MESSAGE( "Create 'create_proposal_operation'" );
      int64_t _id_proposal = executor->create_proposal( cpd.creator, cpd.receiver, cpd.start_date, cpd.end_date, cpd.daily_pay, alice_private_key, false/*with_block_generation*/ );
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

BOOST_AUTO_TEST_SUITE_END()

#endif
