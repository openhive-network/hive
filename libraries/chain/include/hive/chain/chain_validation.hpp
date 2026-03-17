#pragma once

#include <hive/protocol/hive_specialised_exceptions.hpp>

namespace hive { namespace chain {

/// Validates that a price feed is available.
/// @param fhistory  Feed history object to check.
/// @param context   Human-readable context for the error message.
template<typename FeedHistoryT>
inline void validate_price_feed_available( const FeedHistoryT& fhistory, const char* context )
{
  HIVE_CHAIN_STATE_ASSERT( !fhistory.current_median_history.is_null(), fhistory.current_median_history,
    "Cannot ${context} because there is no price feed.",
    ("context", context) );
}

} } // hive::chain
