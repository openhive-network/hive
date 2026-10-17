#include <hive/chain/util/owner_update_limit_mgr.hpp>

#include <hive/protocol/config.hpp>

namespace hive { namespace chain { namespace util {

bool owner_update_limit_mgr::check( const time_point_sec& head_block_time, const time_point_sec& last_time )
{
  return head_block_time - last_time > HIVE_OWNER_UPDATE_LIMIT;
}

bool owner_update_limit_mgr::check( bool hardfork_is_activated, const time_point_sec& head_block_time, const time_point_sec& previous_time, const time_point_sec& last_time )
{
  if( hardfork_is_activated )//HF26
  {
    if( check( head_block_time, last_time ) )
      return true;
    else
      return check( head_block_time, previous_time );
  }
  else
    return check( head_block_time, last_time );
}

std::string owner_update_limit_mgr::msg( bool hardfork_is_activated )
{
  return std::string( "Owner authority can only be updated " ) + ( hardfork_is_activated ? "twice " : "once " )
    + describe_limit( HIVE_OWNER_UPDATE_LIMIT ) + ".";
}

std::string owner_update_limit_mgr::recovery_msg()
{
  return "Account can only be recovered once " + describe_limit( HIVE_OWNER_UPDATE_LIMIT ) + ".";
}

std::string owner_update_limit_mgr::describe_limit( const fc::microseconds& limit )
{
  const int64_t seconds = limit.to_seconds();

  int64_t count = seconds;
  const char* single = "a second";
  const char* plural = " seconds";
  if( seconds > 0 && seconds % 3600 == 0 )
  {
    count = seconds / 3600;
    single = "an hour";
    plural = " hours";
  }
  else if( seconds > 0 && seconds % 60 == 0 )
  {
    count = seconds / 60;
    single = "a minute";
    plural = " minutes";
  }

  if( count == 1 )
    return single;
  return "per " + std::to_string( count ) + plural;
}

} } }
