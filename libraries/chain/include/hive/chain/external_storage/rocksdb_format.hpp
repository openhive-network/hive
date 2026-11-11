#pragma once

#include <rocksdb/options.h>

#include <cstdint>

namespace hive { namespace chain {

/// Block-based table `format_version` of every SST file hived writes. Set explicitly rather than
/// left to the library default, so a RocksDB upgrade cannot change the on-disk format silently.
/// RocksDB reads files of every older format_version, so databases and snapshots written with 6
/// (hived builds before this one) still open; compaction rewrites their files with this version.
/// Files written with 7 cannot be read by RocksDB older than 10.11 (e.g. a 9.10 hived): the format
/// was pinned to 6 for such a rollback until it was decided that nodes are not rolled back to builds
/// with the older RocksDB, which also means a snapshot written by this build does not load there.
constexpr uint32_t ROCKSDB_FORMAT_VERSION = 7;

/// Applies the on-disk format options of every SST file hived writes:
///   - BlockBasedTableOptions::format_version -> ROCKSDB_FORMAT_VERSION;
///   - ColumnFamilyOptions::compression -> kSnappyCompression (default LZ4 since RocksDB 11.5 when
///     LZ4 is compiled in, which hive's RocksDB build is not; Snappy measured no slower than LZ4 or
///     no compression and gives the smallest archive).
/// Other block-based table options already set in @p options are kept.
/// compression_per_level is cleared so the base compression applies at every level;
/// OptimizeLevelStyleCompaction fills it with NoCompression/LZ4 and those entries would
/// otherwise shadow the base compression.
void apply_rocksdb_on_disk_format( ::rocksdb::ColumnFamilyOptions& options );

} } // hive::chain
