#include <hive/chain/external_storage/fd_budget.hpp>

#include <sys/resource.h>
#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <mutex>

#include <fc/log/logger.hpp>

namespace hive { namespace chain {

namespace {

constexpr rlim_t MAX_FD_CAP = 65536;
constexpr rlim_t FD_WARN_THRESHOLD = 8192;

std::once_flag raise_flag;

void do_raise_fd_limit()
{
  struct rlimit rl;
  if( getrlimit( RLIMIT_NOFILE, &rl ) != 0 )
  {
    wlog( "Failed to read file descriptor limit (errno=${e}). "
          "RocksDB will use max_open_files=-1 with current system defaults.",
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

} // anonymous namespace

void raise_fd_limit()
{
  std::call_once( raise_flag, do_raise_fd_limit );
}

} } // hive::chain
