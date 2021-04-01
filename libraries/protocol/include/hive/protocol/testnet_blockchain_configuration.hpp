#pragma once

#include<cstdint>

namespace hive { namespace protocol { namespace testnet_blockchain_configuration {
  class configuration
  {
    uint32_t hive_cashout_windows_seconds = 60*60;// 1 hr;

    public:

      uint32_t get_hive_cashout_windows_seconds() const { return hive_cashout_windows_seconds; }
      void set_hive_cashout_windows_seconds( uint32_t val ){ hive_cashout_windows_seconds = val; }
  };

  extern configuration configuration_data;

} } }// hive::protocol::testnet_blockchain_configuration
