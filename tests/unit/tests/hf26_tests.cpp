#ifdef IS_TEST_NET

#include <boost/test/unit_test.hpp>

#include <hive/chain/util/owner_update_limit_mgr.hpp>

#include "../db_fixture/database_fixture.hpp"

using namespace hive::chain;
using namespace hive::chain::util;

BOOST_FIXTURE_TEST_SUITE( hf26_tests, hf24_database_fixture )

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
