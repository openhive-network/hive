#pragma once

#include <hive/protocol/hive_specialised_exceptions.hpp>

namespace hive { namespace chain {

/// Validates that a price is available.
/// @param exchange_rate Price to check.
/// @param context       Human-readable context for the error message.
inline void validate_price_feed_available( const HBD_price& exchange_rate, const char* context )
{
  HIVE_CHAIN_STATE_ASSERT( !exchange_rate.is_null(), exchange_rate,
    "Cannot ${context} because there is no price feed.", ( "context", context ) );
}

} } // hive::chain
