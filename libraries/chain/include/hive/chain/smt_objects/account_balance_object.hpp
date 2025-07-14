#pragma once

#include <hive/chain/account_object.hpp>
#include <hive/chain/hive_object_types.hpp>
#include <hive/protocol/smt_operations.hpp>

#ifdef HIVE_ENABLE_SMT

namespace hive { namespace chain {

/**
  * Class responsible for holding regular (i.e. non-reward) balance of SMT for given account.
  * It has not been unified with reward balance object counterpart, due to different number
  * of fields needed to hold balances (2 for regular, 3 for reward).
  */
class account_regular_balance_object : public object< account_regular_balance_object_type, account_regular_balance_object >
{
  CHAINBASE_OBJECT( account_regular_balance_object );

public:   
  template < typename Allocator>
  account_regular_balance_object( allocator< Allocator > a, uint64_t _id,
    const account_object& _owner, asset_symbol_type liquid_symbol )
    : id( _id ), owner( _owner.get_account_id() ), liquid( 0, liquid_symbol ), vesting( 0, liquid_symbol.get_paired_symbol() )
  {}

  bool is_empty() const
  {
    return ( liquid.amount == 0 ) && ( vesting.amount == 0 );
  }

  asset_symbol_type get_liquid_symbol() const
  {
    return liquid.symbol;
  }

  /// Name of the account, the balance is held for.
  account_id_type owner;
  asset           liquid;   /// 'balance' for HIVE
  asset           vesting;  /// 'vesting_shares' for VESTS

  CHAINBASE_UNPACK_CONSTRUCTOR(account_regular_balance_object);
};

/**
  * Class responsible for holding reward balance of SMT for given account.
  * It has not been unified with regular balance object counterpart, due to different number
  * of fields needed to hold balances (2 for regular, 3 for reward).
  */
class account_rewards_balance_object : public object< account_rewards_balance_object_type, account_rewards_balance_object >
{
  CHAINBASE_OBJECT( account_rewards_balance_object );

public:   
  template < typename Allocator >
  account_rewards_balance_object( allocator< Allocator > a, uint64_t _id,
    const account_object& _owner, asset_symbol_type _liquid_symbol )
    : id( _id ), owner( _owner.get_account_id() ), pending_liquid( 0, _liquid_symbol ),
    pending_vesting_shares( 0, _liquid_symbol.get_paired_symbol() ), pending_vesting_value( 0, _liquid_symbol )
  {}

  bool is_empty() const
  {
    return ( pending_liquid.amount == 0 ) && ( pending_vesting_shares.amount == 0 );
  }

  asset_symbol_type get_liquid_symbol() const
  {
    return pending_liquid.symbol;
  }

  /// Name of the account, the balance is held for.
  account_id_type owner;
  asset           pending_liquid;          /// 'reward_hive_balance' for pending HIVE
  asset           pending_vesting_shares;  /// 'reward_vesting_balance' for pending VESTS
  asset           pending_vesting_value;   /// 'reward_vesting_hive' for pending VESTS

  CHAINBASE_UNPACK_CONSTRUCTOR(account_rewards_balance_object);
};

struct by_owner_liquid_symbol;

typedef multi_index_container <
  account_regular_balance_object,
  indexed_by <
    ordered_unique< tag< by_id >,
      const_mem_fun< account_regular_balance_object, account_regular_balance_object::id_type, &account_regular_balance_object::get_id>
    >,
    ordered_unique<tag<by_owner_liquid_symbol>,
      composite_key<account_regular_balance_object,
        member< account_regular_balance_object, account_id_type, &account_regular_balance_object::owner >,
        const_mem_fun< account_regular_balance_object, asset_symbol_type, &account_regular_balance_object::get_liquid_symbol >
      >
    >
  >,
  multi_index_allocator< account_regular_balance_object >
> account_regular_balance_index;

typedef multi_index_container <
  account_rewards_balance_object,
  indexed_by <
    ordered_unique< tag< by_id >,
      const_mem_fun< account_rewards_balance_object, account_rewards_balance_object::id_type, &account_rewards_balance_object::get_id>
    >,
    ordered_unique<tag<by_owner_liquid_symbol>,
      composite_key<account_rewards_balance_object,
        member< account_rewards_balance_object, account_id_type, &account_rewards_balance_object::owner >,
        const_mem_fun< account_rewards_balance_object, asset_symbol_type, &account_rewards_balance_object::get_liquid_symbol >
      >
    >
  >,
  multi_index_allocator< account_rewards_balance_object >
> account_rewards_balance_index;

} } // namespace hive::chain

FC_REFLECT( hive::chain::account_regular_balance_object,
  (id)
  (owner)
  (liquid)
  (vesting)
)

FC_REFLECT( hive::chain::account_rewards_balance_object,
  (id)
  (owner)
  (pending_liquid)
  (pending_vesting_shares)
  (pending_vesting_value)
)

CHAINBASE_SET_INDEX_TYPE( hive::chain::account_regular_balance_object, hive::chain::account_regular_balance_index )
CHAINBASE_SET_INDEX_TYPE( hive::chain::account_rewards_balance_object, hive::chain::account_rewards_balance_index )

#endif
