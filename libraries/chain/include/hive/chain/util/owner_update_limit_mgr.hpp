#pragma once

#include <fc/time.hpp>

#include <string>

namespace hive { namespace chain { namespace util {

using fc::time_point_sec;

class owner_update_limit_mgr
{
  public:

    static bool check( const time_point_sec& head_block_time, const time_point_sec& last_time );
    static bool check( bool hardfork_is_activated, const time_point_sec& head_block_time, const time_point_sec& previous_time, const time_point_sec& last_time );

    // Messages state the actual HIVE_OWNER_UPDATE_LIMIT, which differs between mainnet and (configurable) testnet.
    static std::string msg( bool hardfork_is_activated );
    static std::string recovery_msg();

    // "an hour" / "a minute" / "a second" for a single unit, otherwise "per N hours|minutes|seconds".
    static std::string describe_limit( const fc::microseconds& limit );
};

} } }
