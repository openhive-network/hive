#pragma once

#include <hive/protocol/asset.hpp>
#include <hive/protocol/config.hpp>

namespace hive::protocol {
  struct hive_interest
  {
    static fc::uint128_t evaluate_current_interest(fc::uint128_t& hbd_seconds, const fc::time_point_sec& head_block_time, const HBD_asset& hbd, const fc::time_point_sec& hbd_seconds_last_update,
                                                   uint16_t hbd_interest_rate, bool do_evaluation)
    {
      hbd_seconds += fc::uint128_t(hbd.amount.value) * (head_block_time - hbd_seconds_last_update).to_seconds();

      if (hbd_seconds != 0 && do_evaluation)
      {
        auto interest = hbd_seconds / HIVE_SECONDS_PER_YEAR;
        interest *= hbd_interest_rate;
        interest /= HIVE_100_PERCENT;
        return interest;
      }

      return 0;
    }
  };

} // hive::protocol
