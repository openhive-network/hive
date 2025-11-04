#pragma once

#include <hive/protocol/asset.hpp>
#include <hive/protocol/config.hpp>

namespace hive { namespace protocol {
  struct hive_collateral
  {
    static asset estimate_hive_collateral( const price& current_median_history, const price& current_min_history, const asset& hbd_amount_to_get )
    {
      //must reflect calculations from collateralized_convert_evaluator::do_apply

      HIVE_PROTOCOL_VALIDATION_ASSERT( !static_cast<price>( current_median_history ).is_null(), "Cannot estimate conversion collateral because there is no price feed." );

      auto needed_hive = multiply_with_fee( hbd_amount_to_get, current_min_history,
        HIVE_COLLATERALIZED_CONVERSION_FEE, HIVE_SYMBOL );
      uint128_t _amount = ( uint128_t( needed_hive.amount.value ) * HIVE_CONVERSION_COLLATERAL_RATIO ) / HIVE_100_PERCENT;
      asset required_collateral = asset( fc::uint128_to_uint64(_amount), HIVE_SYMBOL );

      return required_collateral;
    }
  };

} } // hive::protocol

