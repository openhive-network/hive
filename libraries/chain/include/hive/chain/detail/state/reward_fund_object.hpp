#pragma once
#include <hive/chain/hive_fwd.hpp>
#include <hive/protocol/asset.hpp>
#include <hive/protocol/misc_utilities.hpp>
#include <hive/chain/hive_object_types.hpp>

namespace hive { namespace chain {

  using hive::protocol::HIVE_asset;

  typedef protocol::fixed_string< 16 > reward_fund_name_type;

  class reward_fund_object : public object< reward_fund_object_type, reward_fund_object >
  {
    CHAINBASE_OBJECT( reward_fund_object );
    public:
      template< typename Allocator >
      reward_fund_object( allocator< Allocator > a, uint64_t _id,
        const string& _name, const HIVE_asset& _balance, const time_point_sec& _creation_time, const uint128_t& _claims = 0 )
        : id( _id ), name( _name ), reward_balance( _balance ), recent_claims( _claims ), last_update( _creation_time )
      {}

      //amount of HIVE in reward fund
      const HIVE_asset& get_reward_balance() const { return reward_balance; }

      reward_fund_name_type   name;
      HIVE_asset              reward_balance;
      uint128_t               recent_claims = 0;
      time_point_sec          last_update;
      uint128_t               content_constant = HIVE_CONTENT_CONSTANT_HF0;
      uint16_t                percent_curation_rewards = HIVE_1_PERCENT * 25;
      uint16_t                percent_content_rewards = HIVE_100_PERCENT;
      protocol::curve_id      author_reward_curve = protocol::curve_id::quadratic;
      protocol::curve_id      curation_reward_curve = protocol::curve_id::bounded_curation;

    CHAINBASE_UNPACK_CONSTRUCTOR(reward_fund_object);
  };

} } // hive::chain

FC_REFLECT( hive::chain::reward_fund_object,
        (id)
        (name)
        (reward_balance)
        (recent_claims)
        (last_update)
        (content_constant)
        (percent_curation_rewards)
        (percent_content_rewards)
        (author_reward_curve)
        (curation_reward_curve)
      )
