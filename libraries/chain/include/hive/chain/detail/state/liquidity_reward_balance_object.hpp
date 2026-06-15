#pragma once
#include <hive/chain/hive_fwd.hpp>
#include <hive/chain/hive_object_types.hpp>

namespace hive { namespace chain {

  /**
    *  If last_update is greater than 1 week, then volume gets reset to 0
    *
    *  When a user is a maker, their volume increases
    *  When a user is a taker, their volume decreases
    *
    *  Every 1000 blocks, the account that has the highest volume_weight() is paid the maximum of
    *  1000 HIVE or 1000 * virtual_supply / (100*blocks_per_year) aka 10 * virtual_supply / blocks_per_year
    *
    *  After being paid volume gets reset to 0
    */
  class liquidity_reward_balance_object : public object< liquidity_reward_balance_object_type, liquidity_reward_balance_object >
  {
    CHAINBASE_OBJECT( liquidity_reward_balance_object );
    public:
      CHAINBASE_DEFAULT_CONSTRUCTOR( liquidity_reward_balance_object )

      int64_t get_hive_volume() const { return hive_volume; }
      int64_t get_hbd_volume() const { return hbd_volume; }

      account_id_type   owner;
      time_point_sec    last_update = fc::time_point_sec::min(); /// used to decay negative liquidity balances. block num
      uint128_t         weight = 0;
      int64_t           hive_volume = 0;
      int64_t           hbd_volume = 0;

      /// this is the sort index
      uint128_t volume_weight()const
      {
        // Compute the int64 product via unsigned arithmetic so the two's-complement wrap is well-defined
        // rather than signed-overflow UB, and bit-identical to the historical result. See hive#857.
        return uint128_t( (int64_t)( (uint64_t)hive_volume * (uint64_t)hbd_volume ) * is_positive() );
      }

      uint128_t min_volume_weight()const
      {
        return std::min(hive_volume,hbd_volume) * is_positive();
      }

      void update_weight( bool hf9 )
      {
          weight = hf9 ? min_volume_weight() : volume_weight();
      }

      inline int is_positive()const
      {
        return ( hive_volume > 0 && hbd_volume > 0 ) ? 1 : 0;
      }
    CHAINBASE_UNPACK_CONSTRUCTOR(liquidity_reward_balance_object);
  };

} } // hive::chain

FC_REFLECT( hive::chain::liquidity_reward_balance_object,
          (id)(owner)(last_update)(weight)(hive_volume)(hbd_volume) )
