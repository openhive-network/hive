#pragma once

#include <fc/time.hpp>

namespace hive { namespace chain { namespace util {

using fc::time_point_sec;

struct owner_update_limit_mgr
{
    static const char* message;

    static bool check( const time_point_sec& head_block_time, const time_point_sec& last_time );
    static bool check( bool hardfork_is_activated, const time_point_sec& head_block_time, const time_point_sec& previous_time, const time_point_sec& last_time );
};

} } }