#include <hive/chain/util/owner_update_limit_mgr.hpp>

#include <hive/protocol/config.hpp>

namespace hive { namespace chain { namespace util {

const char* owner_update_limit_mgr::message       = "Owner authority can only be updated once an hour.";
const char* owner_update_limit_mgr::message_hf26  = "Owner authority can only be updated twice an hour.";

bool owner_update_limit_mgr::check( const time_point_sec& head_block_time, const time_point_sec& last_time )
{
  ilog( "mario: head_block_time: ${head_block_time}, last_time: ${last_time} bool: ${b}",
      ("head_block_time", head_block_time.to_iso_string())("last_time", last_time.to_iso_string())("b", head_block_time - last_time > HIVE_OWNER_UPDATE_LIMIT) );
  return head_block_time - last_time > HIVE_OWNER_UPDATE_LIMIT;
}

bool owner_update_limit_mgr::check( bool hardfork_is_activated, const time_point_sec& head_block_time, const time_point_sec& previous_time, const time_point_sec& last_time )
{
  ilog( "mario: owner_update_limit_mgr::check-A" );
  if( hardfork_is_activated )//HF26
  {
    ilog( "mario: owner_update_limit_mgr::check-B" );
    if( check( head_block_time, last_time ) )
    {
      ilog( "mario: owner_update_limit_mgr::check-C" );
      return true;
    }
    else
    {
      ilog( "mario: owner_update_limit_mgr::check-D" );
      return check( head_block_time, previous_time );
    }
  }
  else
  {
    ilog( "mario: owner_update_limit_mgr::check-E" );
    return check( head_block_time, last_time );
  }
}

const char* owner_update_limit_mgr::msg( bool hardfork_is_activated )
{
  return hardfork_is_activated ? message_hf26 : message;
}

} } }
