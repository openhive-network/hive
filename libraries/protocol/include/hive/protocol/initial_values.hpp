#pragma once

#include <hive/protocol/types.hpp>

namespace hive { namespace protocol {

class initial_values
{
  static uint32_t hive_cashout_windows_seconds;

  public:

    static uint32_t get_hive_cashout_windows_seconds();

    static void set_hive_cashout_windows_seconds( uint32_t val );
};

} } // hive::protocol
