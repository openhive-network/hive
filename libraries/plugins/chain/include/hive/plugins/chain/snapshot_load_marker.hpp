#pragma once

#include <boost/filesystem/path.hpp>

#include <optional>
#include <string>

namespace hive { namespace plugins { namespace chain {

/** Records that a snapshot load into the state is not finished, so that a restart knows it.
    The file lives next to shared_memory.bin, under a name that chainbase::database::wipe() does not remove, so it survives
    the wipes done by the load itself and shares fate with the state. It is part of hived's interface: an orchestrator can
    read it without starting hived. Content: {"stage":"loading"|"replaying","snapshot":"<name>"}.
*/
class snapshot_load_marker
{
public:
  enum class load_stage
  {
    /// The state is being replaced by the snapshot; until this finishes the state is unusable.
    loading,
    /// The snapshot is loaded, but the block log still has to be replayed forward from the snapshot's block.
    replaying
  };

  struct pending_load
  {
    load_stage  stage = load_stage::loading;
    std::string snapshot;
  };

  static constexpr const char* FILE_NAME = "snapshot_load_pending.json";

  explicit snapshot_load_marker( const boost::filesystem::path& shared_memory_dir );

  /// Nothing when no load is pending. A marker that cannot be parsed counts as an unfinished load.
  std::optional< pending_load > read() const;
  /// Replaces the marker atomically, so a crash leaves either the previous or the new content.
  void write( load_stage stage, const std::string& snapshot ) const;
  void clear() const;

  const boost::filesystem::path& path() const { return _path; }

private:
  boost::filesystem::path _path;
};

} } } // hive::plugins::chain
