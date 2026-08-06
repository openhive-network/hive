#pragma once

#include <cstdint>

namespace hive { namespace chain {

/// Raises the process soft file descriptor limit toward the hard limit.
/// Called before opening RocksDB databases.
/// Thread-safe and idempotent (uses std::call_once internally).
void raise_fd_limit();

/// Finite max_open_files value for a single RocksDB database, derived from the process's
/// effective soft descriptor limit minus headroom, shared across the stores this process is
/// expected to open.
///
/// RocksDB's default of -1 keeps one descriptor per live SST file for as long as the database is
/// open, which makes descriptor demand proportional to the file count while the process limit
/// stays a fixed constant. A finite value puts those open files into a bounded LRU table cache:
/// opening one more file closes the least recently used one, so a lookup that lands on an evicted
/// file has to reopen it and re-read its index/filter blocks. The cost is slower reads under
/// pressure rather than failed writes and a wedged node (issue #869).
///
/// Must be called after raise_fd_limit() so it sees the raised limit.
int max_open_files_budget();

/// The derivation used by max_open_files_budget(), split out so it can be exercised against
/// synthetic limits without touching the process rlimit. Always returns a positive value: below
/// the point where headroom plus a per-store floor fits into @p soft_limit it returns that floor.
int max_open_files_budget_for( uint64_t soft_limit );

/// Rate-limited sample of the process's open descriptor count against its effective limit,
/// warning when usage crosses a high fraction of the ceiling.
///
/// The startup limit check cannot see usage that grows over days of uptime, which is why
/// descriptor exhaustion previously arrived with no warning at all. Cheap: the actual count is
/// taken only once every few thousand calls.
void report_fd_usage_if_needed();

} } // hive::chain
