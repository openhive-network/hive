#pragma once

#include <boost/filesystem/path.hpp>

#include <cstdint>
#include <string>

namespace rocksdb { class DB; }

namespace hive { namespace chain {

/// Block range over which a RocksDB database records its operations. Tracing is off while
/// @ref dir is empty.
struct rocksdb_trace_config
{
  boost::filesystem::path dir;
  uint32_t                first_block = 0;
  uint32_t                last_block = 0;

  bool enabled() const { return !dir.empty(); }
};

/// Records the operations of one RocksDB database with RocksDB's own tracing (DB::StartTrace), so
/// they can be replayed later with `db_bench -benchmarks=replay` on another set of options.
///
/// The trace starts when the last irreversible block first reaches a value in
/// [first_block, last_block) and ends when it reaches last_block, or when the database is closed
/// before that; it is recorded once per process. Everything goes into `<dir>/<database name>/`:
///   - checkpoint/      the database as it was when the trace started (a RocksDB checkpoint:
///                      SST files are hard links where the file system allows it), the starting
///                      point a replay of the trace needs;
///   - trace            the trace file;
///   - trace_info.json  the database name, the block range requested and the blocks the trace
///                      actually started and ended at, why it ended, and the RocksDB version.
/// The output directory must not exist yet; the constructor refuses to reuse one.
class rocksdb_trace
{
  public:

    rocksdb_trace( const rocksdb_trace_config& config, const std::string& db_name );

    /// Starts or ends the trace as @p lib enters or leaves the configured range. Throws when the
    /// checkpoint or the trace cannot be started.
    void on_lib( ::rocksdb::DB& db, uint32_t lib );

    /// Ends a running trace, at the given last irreversible block. Failures are logged, not
    /// thrown: this runs while the database is being closed.
    void stop( ::rocksdb::DB& db, uint32_t lib, const char* reason );

    bool is_active() const { return _state == state::tracing; }

    const boost::filesystem::path& output_dir() const { return _output_dir; }

  private:

    enum class state { waiting, tracing, done };

    void start( ::rocksdb::DB& db, uint32_t lib );
    void write_info( const char* end_reason ) const;

    const rocksdb_trace_config  _config;
    const std::string           _db_name;
    const boost::filesystem::path _output_dir;
    state                       _state = state::waiting;
    uint32_t                    _start_block = 0;
    uint32_t                    _end_block = 0;
};

} } // hive::chain
