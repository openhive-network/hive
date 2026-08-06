#include <hive/chain/external_storage/fd_budget.hpp>

#include <sys/resource.h>
#include <dirent.h>

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <climits>
#include <cstdint>
#include <mutex>

#include <fc/log/logger.hpp>

namespace hive { namespace chain {

namespace {

constexpr rlim_t MAX_FD_CAP = 65536;
constexpr rlim_t FD_WARN_THRESHOLD = 8192;

/// Descriptors kept outside the RocksDB budgets: P2P and API sockets, block_log and its
/// artifacts, shared memory, log files, and whatever the runtime needs transiently.
constexpr rlim_t FD_HEADROOM = 8192;
/// RocksDB databases this process may hold open at once (account history, comment archive,
/// plus room for stores added later). The budget is split between them so the sum of the
/// per-store ceilings plus the headroom cannot reach the process limit.
constexpr rlim_t EXPECTED_ROCKSDB_STORES = 4;
/// Floor for the derived budget: below this RocksDB thrashes its table cache.
constexpr rlim_t MIN_OPEN_FILES_BUDGET = 256;

/// Fraction of the effective limit at which descriptor usage is reported as a problem.
constexpr rlim_t FD_USAGE_WARN_PERCENT = 70;
/// Sampling period, counted in calls rather than time: the caller is on the per-block flush
/// path, so at one flush per 3s block this is roughly hourly.
constexpr uint64_t FD_SAMPLE_EVERY_N_CALLS = 1200;

std::once_flag raise_flag;

void do_raise_fd_limit()
{
  struct rlimit rl;
  if( getrlimit( RLIMIT_NOFILE, &rl ) != 0 )
  {
    wlog( "Failed to read file descriptor limit (errno=${e}). "
          "Keeping current system defaults; RocksDB will fall back to a conservative "
          "max_open_files budget.",
          ("e", errno) );
    return;
  }

  rlim_t old_soft = rl.rlim_cur;
  rlim_t hard = rl.rlim_max;
  rlim_t target = std::min( hard, MAX_FD_CAP );

  if( old_soft < target )
  {
    rl.rlim_cur = target;
    if( setrlimit( RLIMIT_NOFILE, &rl ) != 0 )
    {
      wlog( "Failed to raise file descriptor soft limit from ${old} to ${target} (errno=${e}). "
            "Keeping current soft limit. Consider raising ulimit -n manually.",
            ("old", static_cast<uint64_t>( old_soft ))
            ("target", static_cast<uint64_t>( target ))
            ("e", errno) );
      rl.rlim_cur = old_soft;
    }
  }

  ilog( "File descriptor limits: soft ${old} -> ${cur}, hard ${hard}",
        ("old", static_cast<uint64_t>( old_soft ))
        ("cur", static_cast<uint64_t>( rl.rlim_cur ))
        ("hard", static_cast<uint64_t>( hard )) );

  if( rl.rlim_cur < FD_WARN_THRESHOLD )
  {
    wlog( "File descriptor limit is low (${cur}). "
          "This may cause RocksDB errors under heavy load. "
          "Consider raising the limit: ulimit -n ${recommended} or adjust system configuration.",
          ("cur", static_cast<uint64_t>( rl.rlim_cur ))
          ("recommended", static_cast<uint64_t>( FD_WARN_THRESHOLD )) );
  }
}

rlim_t current_soft_limit()
{
  struct rlimit rl;
  if( getrlimit( RLIMIT_NOFILE, &rl ) != 0 )
    return 0;
  return rl.rlim_cur;
}

/// Number of descriptors this process currently holds open, or 0 when it cannot be determined
/// (the caller then simply skips the check rather than guessing).
rlim_t count_open_fds()
{
  DIR* dir = opendir( "/proc/self/fd" );
  if( dir == nullptr )
    return 0;

  rlim_t count = 0;
  while( readdir( dir ) != nullptr )
    ++count;
  closedir( dir );

  // "." , ".." and the descriptor opendir itself holds.
  return count > 3 ? count - 3 : 0;
}

} // anonymous namespace

void raise_fd_limit()
{
  std::call_once( raise_flag, do_raise_fd_limit );
}

int max_open_files_budget_for( uint64_t soft_limit )
{
  rlim_t soft = static_cast<rlim_t>( soft_limit );

  rlim_t budget = MIN_OPEN_FILES_BUDGET;
  if( soft > FD_HEADROOM + EXPECTED_ROCKSDB_STORES * MIN_OPEN_FILES_BUDGET )
    budget = ( soft - FD_HEADROOM ) / EXPECTED_ROCKSDB_STORES;

  return static_cast<int>( std::min<rlim_t>( budget, static_cast<rlim_t>( INT_MAX ) ) );
}

int max_open_files_budget()
{
  rlim_t soft = current_soft_limit();
  if( soft == 0 )
  {
    wlog( "Cannot read the file descriptor limit; using a conservative RocksDB max_open_files of ${b}.",
          ("b", static_cast<uint64_t>( MIN_OPEN_FILES_BUDGET )) );
    return static_cast<int>( MIN_OPEN_FILES_BUDGET );
  }

  int budget = max_open_files_budget_for( soft );

  ilog( "RocksDB max_open_files budget: ${b} per database (soft limit ${s}, headroom ${h}, split ${n} ways).",
        ("b", budget)
        ("s", static_cast<uint64_t>( soft ))
        ("h", static_cast<uint64_t>( FD_HEADROOM ))
        ("n", static_cast<uint64_t>( EXPECTED_ROCKSDB_STORES )) );

  return budget;
}

void report_fd_usage_if_needed()
{
  static std::atomic<uint64_t> call_count{ 0 };
  static std::atomic<rlim_t>   last_reported_percent{ 0 };

  if( ( call_count++ % FD_SAMPLE_EVERY_N_CALLS ) != 0 )
    return;

  rlim_t soft = current_soft_limit();
  if( soft == 0 )
    return;

  rlim_t used = count_open_fds();
  if( used == 0 )
    return;

  rlim_t used_percent = used * 100 / soft;

  if( used_percent < FD_USAGE_WARN_PERCENT )
  {
    // Usage came back down; allow the next crossing to be reported again.
    last_reported_percent.store( 0 );
    return;
  }

  // Report the first crossing and then only further deterioration, so a node sitting at a high
  // watermark does not emit a line on every sample.
  rlim_t previous = last_reported_percent.load();
  if( previous != 0 && used_percent <= previous )
    return;
  last_reported_percent.store( used_percent );

  wlog( "Open file descriptors at ${p}% of the limit (${u} of ${s}). "
        "The dominant consumer is normally the RocksDB stores, which hold one descriptor per open "
        "table file. If this keeps climbing the node will eventually fail to write and stop "
        "advancing its last irreversible block.",
        ("p", static_cast<uint64_t>( used_percent ))
        ("u", static_cast<uint64_t>( used ))
        ("s", static_cast<uint64_t>( soft )) );
}

} } // hive::chain
