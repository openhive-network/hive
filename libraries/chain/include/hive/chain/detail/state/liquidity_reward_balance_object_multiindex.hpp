#pragma once
#include <hive/chain/detail/state/liquidity_reward_balance_object.hpp>
#include <hive/chain/account_object.hpp>

namespace hive { namespace chain {

  struct by_owner;
  struct by_volume_weight;

  typedef multi_index_container<
    liquidity_reward_balance_object,
    indexed_by<
      ordered_unique< tag< by_id >,
        const_mem_fun< liquidity_reward_balance_object, liquidity_reward_balance_object::id_type, &liquidity_reward_balance_object::get_id > >,
      ordered_unique< tag< by_owner >,
        member< liquidity_reward_balance_object, account_id_type, &liquidity_reward_balance_object::owner > >,
      ordered_unique< tag< by_volume_weight >,
        composite_key< liquidity_reward_balance_object,
            member< liquidity_reward_balance_object, fc::uint128, &liquidity_reward_balance_object::weight >,
            member< liquidity_reward_balance_object, account_id_type, &liquidity_reward_balance_object::owner >
        >,
        composite_key_compare< std::greater< fc::uint128 >, std::less< account_id_type > >
      >
    >,
    multi_index_allocator< liquidity_reward_balance_object, 1024 > // not used after HF12
  > liquidity_reward_balance_index;

} } // hive::chain

CHAINBASE_SET_INDEX_TYPE( hive::chain::liquidity_reward_balance_object, hive::chain::liquidity_reward_balance_index )
