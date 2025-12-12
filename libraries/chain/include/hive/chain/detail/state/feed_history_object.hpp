#pragma once
#include <hive/chain/hive_fwd.hpp>
#include <hive/protocol/asset.hpp>
#include <hive/chain/hive_object_types.hpp>

namespace hive { namespace chain {

  using hive::protocol::price;
  using chainbase::t_deque;

  /**
    *  This object gets updated once per hour, on the hour. Tracks price of HIVE (technically in HBD, but
    *  the meaning is in dollars).
    */
  class feed_history_object : public object< feed_history_object_type, feed_history_object, std::true_type >
  {
    CHAINBASE_OBJECT( feed_history_object );
    public:
      CHAINBASE_DEFAULT_CONSTRUCTOR( feed_history_object, (price_history) )

      price current_median_history; ///< the current median of the price history, used as the base for most convert operations
      price market_median_history; ///< same as current_median_history except when the latter is artificially changed upward
      price current_min_history; ///< used as immediate price for collateralized conversion (after fee correction)
      price current_max_history;

      using t_price_history = t_deque< price >;

      t_deque< price > price_history; ///< tracks this last week of median_feed one per hour

      size_t get_dynamic_alloc() const
      {
        size_t size = 0;
        size += price_history.size() * sizeof( decltype( price_history )::value_type );
        return size;
      }

    CHAINBASE_UNPACK_CONSTRUCTOR(feed_history_object, (price_history));
  };

} } // hive::chain

FC_REFLECT( hive::chain::feed_history_object,
          (id)(current_median_history)(market_median_history)(current_min_history)(current_max_history)(price_history) )
