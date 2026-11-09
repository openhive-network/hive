#pragma once

#include <rocksdb/options.h>

#include <cstdint>

namespace hive { namespace chain {

/// Block-based table `format_version` of every SST file hived writes. 6 is the newest version
/// RocksDB 9.10 can read; RocksDB 10.11 made 7 the default, which 9.10 rejects.
constexpr uint32_t ROCKSDB_PINNED_FORMAT_VERSION = 6;

/// Pins the on-disk format options whose RocksDB defaults changed after 9.10 back to values
/// RocksDB 9.10 reads, so databases and snapshot files written by this build stay readable by
/// an older hived (rollback without rebuilding the comment archive or account history):
///   - BlockBasedTableOptions::format_version -> ROCKSDB_PINNED_FORMAT_VERSION (default 7 since 10.11);
///   - ColumnFamilyOptions::compression -> kSnappyCompression (default LZ4 since 11.5 when LZ4
///     is compiled in, which an older hived build may lack).
/// Other block-based table options already set in @p options are kept.
/// compression_per_level is cleared so the base compression applies at every level;
/// OptimizeLevelStyleCompaction fills it with NoCompression/LZ4 and those entries would
/// otherwise shadow the base compression pin.
void pin_rocksdb_on_disk_format( ::rocksdb::ColumnFamilyOptions& options );

} } // hive::chain
