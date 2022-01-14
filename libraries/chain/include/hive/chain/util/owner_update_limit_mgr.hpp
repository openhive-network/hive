#pragma once

#include <fc/time.hpp>

namespace hive { namespace chain { namespace util {

using fc::time_point_sec;

class owner_update_limit_mgr
{
    static const char* message;
    static const char* message_hf26;

  public:

    static bool check( const time_point_sec& head_block_time, const time_point_sec& last_time );
    static bool check( bool hardfork_is_activated, const time_point_sec& head_block_time, const time_point_sec& previous_time, const time_point_sec& last_time );

    static const char* msg( bool hardfork_is_activated );
};

} } }