#ifdef IS_TEST_NET
#include <boost/test/unit_test.hpp>

#include <hive/chain/account_object.hpp>

#include <hive/plugins/state_snapshot/state_snapshot_plugin.hpp>

#include "../db_fixture/hived_fixture.hpp"

using namespace hive::chain;
using namespace hive::protocol;

BOOST_AUTO_TEST_SUITE( snapshots_tests )

BOOST_AUTO_TEST_CASE( additional_allocation_after_snapshot_load )
{
  struct test_fixture : public hived_fixture {
    test_fixture(bool remove_db_files) : hived_fixture(remove_db_files)
    {}

    void dump_snapshot()
    {
      hive::plugins::state_snapshot::state_snapshot_plugin* plugin = nullptr;
      postponed_init(
        {
          config_line_t( { "plugin",
            { "state_snapshot" } }
          ),
          config_line_t( { "dump-snapshot",
            { std::string("snap") } }
          ),
          config_line_t( { "snapshot-root-dir",
            { std::string("additional_allocation_after_snapshot_load") } }
          )
        },
        &plugin);

      generate_block();
      db->set_hardfork( 24 );
      generate_block();

    }

    void load_snapshot()
    {
      hive::plugins::state_snapshot::state_snapshot_plugin* snapshot = nullptr;
      postponed_init(
        {
          config_line_t( { "plugin",
            { "state_snapshot" } }
          ),
          config_line_t( { "load-snapshot",
            { std::string("snap") } }
          ),
          config_line_t( { "snapshot-root-dir",
            { std::string("additional_allocation_after_snapshot_load") } }
          )
        },
        &snapshot);

      generate_block();
      db->set_hardfork( 24 );
      generate_block();

      const auto& index = db->get_index<account_index>();
      BOOST_REQUIRE_GT(index.get_item_additional_allocation(), 0);
    }

    void create_objects()
    {
      postponed_init();

      generate_block();
      db->set_hardfork( 24 );
      generate_block();

      // Fill up the rest of the required miners
      for( int i = HIVE_NUM_INIT_MINERS; i < HIVE_MAX_WITNESSES; i++ )
      {
        account_create( HIVE_INIT_MINER_NAME + fc::to_string( i ), init_account_pub_key );
        fund( HIVE_INIT_MINER_NAME + fc::to_string( i ), HIVE_MIN_PRODUCER_REWARD );
        witness_create( HIVE_INIT_MINER_NAME + fc::to_string( i ), init_account_priv_key, "foo.bar", init_account_pub_key, HIVE_MIN_PRODUCER_REWARD.amount );
      }
      validate_database();

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
