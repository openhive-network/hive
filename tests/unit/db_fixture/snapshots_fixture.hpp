#pragma once

#include "hived_proxy_fixture.hpp"

#include <hive/plugins/state_snapshot/state_snapshot_plugin.hpp>

#include <fc/log/logger_config.hpp>
#include <fc/optional.hpp>
#include <fc/filesystem.hpp>

namespace hive { namespace chain {

struct snapshots_fixture : hived_proxy_fixture
{
  snapshots_fixture(
    bool remove_db_files = true)
    : hived_proxy_fixture(remove_db_files)
  {}
  ~snapshots_fixture()
  {}

  void clear_snapshot(std::string snapshot_root_dir)
  {
    // remove any snapshots from last run
    const fc::path temp_data_dir = hive::utilities::temp_directory_path();
    fc::remove_all( ( temp_data_dir / snapshot_root_dir ) );
  }

  void dump_snapshot(std::string snapshot_root_dir)
  {
    reset_fixture(false);
    hive::plugins::state_snapshot::state_snapshot_plugin* plugin = nullptr;
    postponed_init(
      {
        hived_fixture::config_line_t( { "plugin",
          { "state_snapshot" } }
        ),
        hived_fixture::config_line_t( { "dump-snapshot",
          { std::string("snap") } }
        ),
        hived_fixture::config_line_t( { "snapshot-root-dir",
          { snapshot_root_dir } }
        )
      },
      &plugin);
  }

  void load_snapshot(std::string snapshot_root_dir)
  {
    reset_fixture(false);
    hive::plugins::state_snapshot::state_snapshot_plugin* snapshot = nullptr;
    postponed_init(
      {
        hived_fixture::config_line_t( { "plugin",
          { "state_snapshot" } }
        ),
        hived_fixture::config_line_t( { "load-snapshot",
          { std::string("snap") } }
        ),
        hived_fixture::config_line_t( { "snapshot-root-dir",
          { snapshot_root_dir } }
        )
      },
      &snapshot);
  }

};

} }
