#ifdef IS_TEST_NET

#include <boost/test/unit_test.hpp>

#include <hive/chain/util/owner_update_limit_mgr.hpp>

#include "../db_fixture/database_fixture.hpp"

using namespace hive::chain;
using namespace hive::chain::util;

BOOST_FIXTURE_TEST_SUITE( hf26_tests, hf26_database_fixture )

BOOST_AUTO_TEST_CASE( update_operation )
{
  try
  {
    BOOST_TEST_MESSAGE( "Testing: update authorization before and after HF26" );

    ACTORS( (alice) );
    fund( "alice", 1000000 );
    generate_block();

    db_plugin->debug_update( [=]( database& db )
    {
      db.modify( db.get_witness_schedule_object(), [&]( witness_schedule_object& wso )
      {
        wso.median_props.account_creation_fee = ASSET( "0.100 TESTS" );
      });
    });

    generate_block();

    BOOST_TEST_MESSAGE( "Creating account bob with alice" );

    account_create_operation acc_create;
    acc_create.fee = ASSET( "0.100 TESTS" );
    acc_create.creator = "alice";
    acc_create.new_account_name = "bob";
    acc_create.owner = authority( 1, generate_private_key( "bob_owner" ).get_public_key(), 1 );
    acc_create.active = authority( 1, generate_private_key( "bob_active" ).get_public_key(), 1 );
    acc_create.posting = authority( 1, generate_private_key( "bob_posting" ).get_public_key(), 1 );
    acc_create.memo_key = generate_private_key( "bob_memo" ).get_public_key();
    acc_create.json_metadata = "";

    signed_transaction tx;
    tx.operations.push_back( acc_create );
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    sign( tx, alice_private_key );
    db->push_transaction( tx, 0 );

    vest( HIVE_INIT_MINER_NAME, "bob", asset( 1000, HIVE_SYMBOL ) );

    auto exec_update_op = [this]( const std::string& old_key,  const std::string& new_key, bool is_exception = false )
    {
      BOOST_TEST_MESSAGE("============update of authorization============");
      BOOST_TEST_MESSAGE( db->head_block_time().to_iso_string().c_str() );

      signed_transaction tx;

      account_update_operation acc_update;
      acc_update.account = "bob";

      acc_update.owner = authority( 1, generate_private_key( new_key ).get_public_key(), 1 );

      tx.operations.push_back( acc_update );
      tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
      sign( tx, generate_private_key( old_key ) );

      if( is_exception )
      {
        HIVE_REQUIRE_THROW( db->push_transaction( tx, 0 ), fc::assert_exception );
      }
      else
      {
        db->push_transaction( tx, 0 );
      }

      generate_block();
    };

    exec_update_op( "bob_owner", "key numer 01" );
    exec_update_op( "key numer 01", "key numer 02" );
    exec_update_op( "key numer 02", "key numer 03", true/*is_exception*/ );
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

BOOST_AUTO_TEST_SUITE_END()

#endif
