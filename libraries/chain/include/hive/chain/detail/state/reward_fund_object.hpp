#pragma once
#include <hive/chain/hive_fwd.hpp>
#include <hive/chain/util/balance.hpp>
#include <hive/protocol/config.hpp>
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
    const string& _name, temp_HIVE_balance&& _balance, const time_point_sec& _creation_time, const uint128_t& _claims = 0 )
  : id( _id ), name( _name ), reward_balance( std::move( _balance ) ), recent_claims( _claims ), last_update( _creation_time )
  {}

// getters:

  const reward_fund_name_type& get_name() const { return name; }

  //amount of HIVE in reward fund
  const HIVE_asset& get_reward_balance() const { return reward_balance; }

  uint128_t get_recent_claims() const { return recent_claims; }
  time_point_sec get_last_update() const { return last_update; }

  uint128_t get_content_constant() const { return content_constant; }
  uint16_t get_percent_curation_rewards() const { return percent_curation_rewards; }
  uint16_t get_percent_content_rewards() const { return percent_content_rewards; }
  protocol::curve_id get_author_reward_curve() const { return author_reward_curve; }
  protocol::curve_id get_curation_reward_curve() const { return curation_reward_curve; }

  void check_on_remove() const
  {
    FC_ASSERT( reward_balance.is_empty(), "Removing reward_fund_object with non-empty balance field" );
  }

// setters:

  HIVE_balance& access_reward_balance() { return reward_balance; }
  uint128_t& access_recent_claims() { return recent_claims; }
  time_point_sec& access_last_update() { return last_update; }

// only changed during hardfork:

  void set_content_constant( uint128_t value ) { content_constant = value; }
  void set_percent_curation_rewards( uint16_t value ) { percent_curation_rewards = value; }
  void set_percent_content_rewards( uint16_t value ) { percent_content_rewards = value; }
  void set_author_reward_curve( protocol::curve_id value ) { author_reward_curve = value; }
  void set_curation_reward_curve( protocol::curve_id value ) { curation_reward_curve = value; }

private:
  reward_fund_name_type   name;
  HIVE_balance            reward_balance;
  uint128_t               recent_claims = 0;
  time_point_sec          last_update;
  uint128_t               content_constant = HIVE_CONTENT_CONSTANT_HF0;
  uint16_t                percent_curation_rewards = HIVE_1_PERCENT * 25;
  uint16_t                percent_content_rewards = HIVE_100_PERCENT;
  protocol::curve_id      author_reward_curve = protocol::curve_id::quadratic;
  protocol::curve_id      curation_reward_curve = protocol::curve_id::bounded_curation;

  CHAINBASE_UNPACK_CONSTRUCTOR( reward_fund_object );
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
