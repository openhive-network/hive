#ifdef IS_TEST_NET
#include <boost/test/unit_test.hpp>

#include <hive/chain/account_object.hpp>

#include <hive/plugins/state_snapshot/state_snapshot_plugin.hpp>

#include "../db_fixture/snapshots_fixture.hpp"

using namespace hive::chain;
using namespace hive::protocol;

BOOST_AUTO_TEST_SUITE( snapshots_tests )

BOOST_AUTO_TEST_CASE( additional_allocation_after_snapshot_load )
{
  struct test_fixture : public snapshots_fixture {
    test_fixture(bool remove_db_files) : snapshots_fixture(remove_db_files, "additional_allocation_after_snapshot_load", 24)
    {}

    void load_snapshot() override
    {
      snapshots_fixture::load_snapshot();

      const auto& index = db->get_index<account_index>();
      BOOST_REQUIRE_GT(index.get_item_additional_allocation(), 0);
    }

    void create_objects() override
    {
      snapshots_fixture::create_objects();

      set_price_feed( price( ASSET( "1.000 TBD" ), ASSET( "1.000 TESTS" ) ) );
      generate_block();

      const auto& index = db->get_index<account_index>();
      const size_t initial_allocations = index.get_item_additional_allocation();

      ACTOR_DEFAULT_FEE( alice )
      generate_block();
      ISSUE_FUNDS( "alice", ASSET( "100000.000 TESTS" ) );
      BOOST_REQUIRE( compare_delayed_vote_count("alice", {}) );
      BOOST_REQUIRE_EQUAL(index.get_item_additional_allocation(), initial_allocations);

      vest( "alice", "alice", ASSET( "100.000 TESTS" ), alice_private_key );
      generate_block();
      BOOST_CHECK( compare_delayed_vote_count("alice", { static_cast<uint64_t>(get_vesting( "alice" ).amount.value) }) );
      BOOST_CHECK_EQUAL(index.get_item_additional_allocation(), initial_allocations + 2*sizeof(hive::chain::delayed_votes_data));
    }
  };

  try
  {
    BOOST_TEST_MESSAGE( "--- Testing: additional_allocation_after_snapshot_load" );

    // remove any snapshots from last run
    const fc::path temp_data_dir = hive::utilities::temp_directory_path();
    fc::remove_all( ( temp_data_dir / "additional_allocation_after_snapshot_load" ) );

    {
      test_fixture fixture(true);
      fixture.create_objects();
    }
    {
      test_fixture fixture(false);
      fixture.dump_snapshot();
    }
    {
      test_fixture fixture(false);
      fixture.load_snapshot();
    }

  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_SUITE_END()
#endif
