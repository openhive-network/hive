#include <hive/protocol/initial_values.hpp>

namespace hive { namespace protocol {

uint32_t initial_values::hive_cashout_windows_seconds = 60*60;// 1 hr

uint32_t initial_values::get_hive_cashout_windows_seconds()
{
  return hive_cashout_windows_seconds;
}

void initial_values::set_hive_cashout_windows_seconds( uint32_t val )
{
  hive_cashout_windows_seconds = val;
}

} } // hive::protocol
