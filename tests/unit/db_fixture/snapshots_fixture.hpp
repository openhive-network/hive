#pragma once

#include "hived_fixture.hpp"

#include <hive/plugins/state_snapshot/state_snapshot_plugin.hpp>

#include <fc/log/logger_config.hpp>
#include <fc/optional.hpp>

namespace hive { namespace chain {

class snapshots_fixture : public hived_fixture
{
public:
  snapshots_fixture(
    bool remove_db_files,
    std::string snapshot_root_dir,
    fc::optional<uint32_t> hardfork = fc::optional<uint32_t>())
    : hived_fixture(remove_db_files), snapshot_root_dir{snapshot_root_dir}, hardfork{hardfork}
  {}
  virtual ~snapshots_fixture()
  {}

  virtual void create_objects()
  {
    postponed_init();

    if (hardfork) {
      generate_block();
      db->set_hardfork( hardfork.value() );
      generate_block();
    }

    // Fill up the rest of the required miners
    for( int i = HIVE_NUM_INIT_MINERS; i < HIVE_MAX_WITNESSES; i++ )
    {
      account_create( HIVE_INIT_MINER_NAME + fc::to_string( i ), init_account_pub_key );
      fund( HIVE_INIT_MINER_NAME + fc::to_string( i ), HIVE_MIN_PRODUCER_REWARD );
      witness_create( HIVE_INIT_MINER_NAME + fc::to_string( i ), init_account_priv_key, "foo.bar", init_account_pub_key, HIVE_MIN_PRODUCER_REWARD.amount );
    }
    validate_database();
  }

  virtual void dump_snapshot()
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

    if (hardfork) {
      generate_block();
      db->set_hardfork( hardfork.value() );
      generate_block();
    }
  }

  virtual void load_snapshot()
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

    if (hardfork) {
      generate_block();
      db->set_hardfork( hardfork.value() );
      generate_block();
    }
  }

private:
  std::string snapshot_root_dir;
  fc::optional<uint32_t> hardfork;
};

} }
