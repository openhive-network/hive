#ifdef IS_TEST_NET

#include <boost/test/unit_test.hpp>

#include <hive/chain/util/owner_update_limit_mgr.hpp>
#include <hive/protocol/misc_utilities.hpp>
#include <hive/protocol/exceptions.hpp>

#include "../db_fixture/database_fixture.hpp"

using namespace hive::chain;
using namespace hive::chain::util;

BOOST_FIXTURE_TEST_SUITE( hf26_tests, cluster_database_fixture )

BOOST_AUTO_TEST_CASE( update_operation )
{
  try
  {
    struct data
    {
      std::string old_key;
      std::string new_key;

      bool is_exception = false;
    };

    std::vector<data> _v
    { 
      { "bob_owner",    "key number 01", false },
      { "key number 01", "key number 02", true },
      { "key number 01", "key number 03", true }
    };

    std::vector<data> _v_hf26 =
    { 
      { "bob_owner",    "key number 01", false },
      { "key number 01", "key number 02", false },
      { "key number 02", "key number 03", true }
    };

    auto _content = [&_v]( ptr_hardfork_database_fixture& executor )
    {
      BOOST_TEST_MESSAGE( "Testing: update authorization before and after HF26" );

      BOOST_REQUIRE_EQUAL( (bool)executor, true );

      ACTORS_EXT( (*executor), (alice) );
      executor->fund( "alice", 1000000 );
      executor->generate_block();

      executor->db_plugin->debug_update( [=]( database& db )
      {
        db.modify( db.get_witness_schedule_object(), [&]( witness_schedule_object& wso )
        {
          wso.median_props.account_creation_fee = ASSET( "0.100 TESTS" );
        });
      });

      executor->generate_block();

      BOOST_TEST_MESSAGE( "Creating account bob with alice" );

      account_create_operation acc_create;
      acc_create.fee = ASSET( "0.100 TESTS" );
      acc_create.creator = "alice";
      acc_create.new_account_name = "bob";
      acc_create.owner = authority( 1, executor->generate_private_key( "bob_owner" ).get_public_key(), 1 );
      acc_create.active = authority( 1, executor->generate_private_key( "bob_active" ).get_public_key(), 1 );
      acc_create.posting = authority( 1, executor->generate_private_key( "bob_posting" ).get_public_key(), 1 );
      acc_create.memo_key = executor->generate_private_key( "bob_memo" ).get_public_key();
      acc_create.json_metadata = "";

      signed_transaction tx;
      tx.operations.push_back( acc_create );
      tx.set_expiration( executor->db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
      executor->sign( tx, alice_private_key );
      executor->db->push_transaction( tx, 0 );

      executor->vest( HIVE_INIT_MINER_NAME, "bob", asset( 1000, HIVE_SYMBOL ) );

      auto exec_update_op = [&]( const std::string& old_key,  const std::string& new_key, bool is_exception )
      {
        BOOST_TEST_MESSAGE("============update of authorization============");
        BOOST_TEST_MESSAGE( executor->db->head_block_time().to_iso_string().c_str() );

        signed_transaction tx;

        account_update_operation acc_update;
        acc_update.account = "bob";

        acc_update.owner = authority( 1, executor->generate_private_key( new_key ).get_public_key(), 1 );

        tx.operations.push_back( acc_update );
        tx.set_expiration( executor->db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
        executor->sign( tx, executor->generate_private_key( old_key ) );

        if( is_exception )
        {
          HIVE_REQUIRE_THROW( executor->db->push_transaction( tx, 0 ), fc::assert_exception );
        }
        else
        {
          executor->db->push_transaction( tx, 0 );
        }

        executor->generate_block();
      };

      for( auto& data : _v )
      {
        exec_update_op( data.old_key, data.new_key, data.is_exception );
      }
    };

    execute_hardfork<25>( _content );

    _v = _v_hf26;

    execute_hardfork<26>( _content );

  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( owner_update_limit )
{
  BOOST_TEST_MESSAGE( "Before HF26 changing an authorization was only once per hour, after HF26 is twice per hour" );

  bool hardfork_is_activated = false;

  fc::time_point_sec start_time = fc::variant( "2022-01-01T00:00:00" ).as< fc::time_point_sec >();

  auto previous_time    = start_time + fc::seconds(1);
  auto last_time        = start_time + fc::seconds(5);
  auto head_block_time  = start_time + fc::seconds(5);

  {
    BOOST_REQUIRE_EQUAL( owner_update_limit_mgr::check( head_block_time, last_time ), false );

    head_block_time += fc::seconds(1);
    BOOST_REQUIRE_EQUAL( owner_update_limit_mgr::check( head_block_time, last_time ), false );

    head_block_time += fc::seconds(4);
    BOOST_REQUIRE_EQUAL( owner_update_limit_mgr::check( head_block_time, last_time ), false );

    head_block_time += fc::seconds(1);
    BOOST_REQUIRE_EQUAL( owner_update_limit_mgr::check( head_block_time, last_time ), false );

    head_block_time += fc::seconds(1);
    BOOST_REQUIRE_EQUAL( owner_update_limit_mgr::check( head_block_time, last_time ), true );
  }

  {
    last_time           = start_time + fc::seconds(1);
    head_block_time     = start_time + fc::seconds(1);

    BOOST_REQUIRE_EQUAL( owner_update_limit_mgr::check( hardfork_is_activated, head_block_time, previous_time, last_time ), false );

    head_block_time += fc::seconds(1);
    BOOST_REQUIRE_EQUAL( owner_update_limit_mgr::check( hardfork_is_activated, head_block_time, previous_time, last_time ), false );

    head_block_time += fc::seconds(5);
    BOOST_REQUIRE_EQUAL( owner_update_limit_mgr::check( hardfork_is_activated, head_block_time, previous_time, last_time ), false );

    head_block_time += fc::seconds(1);
    BOOST_REQUIRE_EQUAL( owner_update_limit_mgr::check( hardfork_is_activated, head_block_time, previous_time, last_time ), true );
  }

  {
    previous_time   = start_time + fc::seconds(1);
    last_time       = start_time + fc::seconds(5);
    head_block_time = start_time + fc::seconds(5);

    BOOST_REQUIRE_EQUAL( owner_update_limit_mgr::check( hardfork_is_activated, head_block_time, previous_time, last_time ), false );

    head_block_time += fc::seconds(6);
    BOOST_REQUIRE_EQUAL( owner_update_limit_mgr::check( hardfork_is_activated, head_block_time, previous_time, last_time ), false );

    head_block_time += fc::seconds(1);
    BOOST_REQUIRE_EQUAL( owner_update_limit_mgr::check( hardfork_is_activated, head_block_time, previous_time, last_time ), true );
  }

  {
    previous_time   = start_time;
    last_time       = start_time + fc::seconds(1);
    head_block_time = start_time + fc::seconds(7);
    BOOST_REQUIRE_EQUAL( owner_update_limit_mgr::check( hardfork_is_activated, head_block_time, previous_time, last_time ), false );
  }

  hardfork_is_activated = true;

  {
    previous_time   = start_time + fc::seconds(1);
    last_time       = start_time + fc::seconds(50);
    head_block_time = start_time + fc::seconds(50);

    BOOST_REQUIRE_EQUAL( owner_update_limit_mgr::check( hardfork_is_activated, head_block_time, previous_time, last_time ), true );
  }

  {
    previous_time   = start_time + fc::seconds(1);
    last_time       = start_time + fc::seconds(7);
    head_block_time = start_time + fc::seconds(7);
    BOOST_REQUIRE_EQUAL( owner_update_limit_mgr::check( hardfork_is_activated, head_block_time, previous_time, last_time ), false );

    previous_time   -= fc::seconds(1);
    BOOST_REQUIRE_EQUAL( owner_update_limit_mgr::check( hardfork_is_activated, head_block_time, previous_time, last_time ), true );
  }

  {
    previous_time   = start_time + fc::seconds(1);
    last_time       = start_time + fc::seconds(3);
    head_block_time = start_time + fc::seconds(9);
    BOOST_REQUIRE_EQUAL( owner_update_limit_mgr::check( hardfork_is_activated, head_block_time, previous_time, last_time ), true );
  }

  {
    previous_time   = start_time + fc::seconds(3);
    last_time       = start_time + fc::seconds(3);
    head_block_time = start_time + fc::seconds(9);
    BOOST_REQUIRE_EQUAL( owner_update_limit_mgr::check( hardfork_is_activated, head_block_time, previous_time, last_time ), false );
  }

  {
    previous_time   = start_time + fc::seconds(3) - fc::seconds(1);
    last_time       = start_time + fc::seconds(3);
    head_block_time = start_time + fc::seconds(9);
    BOOST_REQUIRE_EQUAL( owner_update_limit_mgr::check( hardfork_is_activated, head_block_time, previous_time, last_time ), true );
  }

  {
    previous_time   = start_time + fc::seconds(1);
    last_time       = start_time + fc::seconds(1);
    head_block_time = start_time + fc::seconds(7);
    BOOST_REQUIRE_EQUAL( owner_update_limit_mgr::check( hardfork_is_activated, head_block_time, previous_time, last_time ), false );
  }

  {
    previous_time   = start_time + fc::seconds(1);
    last_time       = start_time + fc::seconds(1);
    head_block_time = start_time + fc::seconds(7) + fc::seconds(1);
    BOOST_REQUIRE_EQUAL( owner_update_limit_mgr::check( hardfork_is_activated, head_block_time, previous_time, last_time ), true );
  }

  {
    previous_time   = start_time;
    last_time       = start_time + fc::seconds(1);
    head_block_time = start_time + fc::seconds(7);
    BOOST_REQUIRE_EQUAL( owner_update_limit_mgr::check( hardfork_is_activated, head_block_time, previous_time, last_time ), true );
  }

  {
    previous_time   = start_time;
    last_time       = start_time;
    head_block_time = start_time + fc::seconds(7);
    BOOST_REQUIRE_EQUAL( owner_update_limit_mgr::check( hardfork_is_activated, head_block_time, previous_time, last_time ), true );
  }
}

BOOST_AUTO_TEST_CASE( pack_transaction_basic )
{
  try
  {
    bool is_hf26 = false;

    auto _content = [&is_hf26]( ptr_hardfork_database_fixture& executor )
    {
      BOOST_TEST_MESSAGE( "Testing: transaction's pack before and after HF26" );

      BOOST_REQUIRE_EQUAL( (bool)executor, true );

      ACTORS_EXT( (*executor), (alice)(bob) );
      executor->fund( "alice", 1000000 );
      executor->fund( "bob", 1000000 );
      executor->generate_block();

      auto _get_trx = []( ptr_hardfork_database_fixture& executor, const std::vector<operation>& ops, const fc::ecc::private_key& private_key, hive::protocol::pack_type pack )
      {
        signed_transaction _result;

        for( auto& op : ops )
        {
          _result.operations.push_back( op );
        }

        _result.set_expiration( executor->db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
        {
          hive::protocol::serialization_mode_controller::pack_guard guard( pack );
          executor->sign( _result, private_key );
        }

        return _result;
      };

      auto _op_transfer = [&_get_trx]( ptr_hardfork_database_fixture& executor, const fc::ecc::private_key& private_key, bool is_hf26 )
      {
        BOOST_TEST_MESSAGE( "Executing operation using legacy/hf26 serialization" );

        auto _66 = asset( 66, HIVE_SYMBOL );

        transfer_operation _op;
        _op.from    = "alice";
        _op.to      = "bob";
        _op.amount  = _66;

        {
          auto _alice_balance_previous  = executor->get_balance( "alice" );
          auto _bob_balance_previous    = executor->get_balance( "bob" );

          signed_transaction _tx = _get_trx( executor, { _op }, private_key, hive::protocol::pack_type::legacy );
          executor->db->push_transaction( _tx, 0 );

          auto _alice_balance_after = executor->get_balance( "alice" );
          auto _bob_balance_after   = executor->get_balance( "bob" );

          BOOST_REQUIRE_EQUAL( _alice_balance_previous.amount.value, ( _alice_balance_after + _66 ).amount.value );
          BOOST_REQUIRE_EQUAL( _bob_balance_previous.amount.value, ( _bob_balance_after - _66 ).amount.value );

          executor->generate_block();
        }

        if( is_hf26 )
        {
          auto _alice_balance_previous  = executor->get_balance( "alice" );
          auto _bob_balance_previous    = executor->get_balance( "bob" );

          signed_transaction _tx = _get_trx( executor, { _op }, private_key, hive::protocol::pack_type::hf26 );
          executor->db->push_transaction( _tx, 0 );

          auto _alice_balance_after = executor->get_balance( "alice" );
          auto _bob_balance_after   = executor->get_balance( "bob" );

          BOOST_REQUIRE_EQUAL( _alice_balance_previous.amount.value, ( _alice_balance_after + _66 ).amount.value );
          BOOST_REQUIRE_EQUAL( _bob_balance_previous.amount.value, ( _bob_balance_after - _66 ).amount.value );

          executor->generate_block();
        }
        else
        {
          signed_transaction _tx = _get_trx( executor, { _op }, private_key, hive::protocol::pack_type::hf26 );
          HIVE_REQUIRE_THROW( executor->db->push_transaction( _tx, 0 ), tx_missing_active_auth );
        }
      };

      _op_transfer( executor, alice_private_key, is_hf26 );
    };

    execute_hardfork<25>( _content );

    is_hf26 = true;

    execute_hardfork<26>( _content );

  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_SUITE_END()

#endif
