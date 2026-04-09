#pragma once

namespace hive { namespace chain {

/// Raises the process soft file descriptor limit toward the hard limit.
/// Called before opening RocksDB databases so they can use max_open_files = -1
/// (RocksDB's recommended default) without hitting OS limits.
/// Thread-safe and idempotent (uses std::call_once internally).
void raise_fd_limit();

} } // hive::chain
