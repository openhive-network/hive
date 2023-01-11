#pragma once

#include <hive/chain/util/asset.hpp>

#include <hive/protocol/asset.hpp>
#include <hive/protocol/config.hpp>
#include <hive/protocol/types.hpp>
#include <hive/protocol/misc_utilities.hpp>

#include <fc/reflect/reflect.hpp>

#include <fc/uint128.hpp>

namespace hive { namespace chain { namespace util {

using hive::protocol::asset;
using hive::protocol::price;
using hive::protocol::share_type;

using fc::uint128_t;

struct comment_reward_context
{
  share_type rshares;
  uint16_t   reward_weight = 0;
  asset      max_hbd;
  uint128_t  total_reward_shares2 = 0;
  asset      total_reward_fund_hive;
  price      current_hive_price;
  protocol::curve_id   reward_curve = protocol::quadratic;
  uint128_t  content_constant = HIVE_CONTENT_CONSTANT_HF0;
};

uint64_t get_rshare_reward( const comment_reward_context& ctx );

inline uint128_t get_content_constant_s()
{
  return HIVE_CONTENT_CONSTANT_HF0; // looking good for posters
}

uint128_t evaluate_reward_curve( const uint128_t& rshares, const protocol::curve_id& curve = protocol::quadratic, const uint128_t& var1 = HIVE_CONTENT_CONSTANT_HF0 );

inline bool is_comment_payout_dust( const price& p, uint64_t hive_payout )
{
  return to_hbd( p, asset( hive_payout, HIVE_SYMBOL ) ) < HIVE_MIN_PAYOUT_HBD;
}

} } } // hive::chain::util

FC_REFLECT( hive::chain::util::comment_reward_context,
  (rshares)
  (reward_weight)
  (max_hbd)
  (total_reward_shares2)
  (total_reward_fund_hive)
  (current_hive_price)
  (reward_curve)
  (content_constant)
  )
