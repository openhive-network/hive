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
    vest( HIVE_INIT_MINER_NAME, "bob", asset( bob_amount / 1000, HIVE_SYMBOL ), init_account_priv_key );
    vest( HIVE_INIT_MINER_NAME, "gil", asset( gil_amount / 1000, HIVE_SYMBOL ), init_account_priv_key );
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

BOOST_AUTO_TEST_SUITE_END()

#endif
