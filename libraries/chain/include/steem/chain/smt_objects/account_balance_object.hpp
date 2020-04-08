#pragma once

#include <steem/chain/steem_object_types.hpp>
#include <steem/protocol/smt_operations.hpp>

#ifdef STEEM_ENABLE_SMT

namespace steem { namespace chain {

/**
 * Class responsible for holding regular (i.e. non-reward) balance of SMT for given account.
 * It has not been unified with reward balance object counterpart, due to different number
 * of fields needed to hold balances (2 for regular, 3 for reward).
 */
class account_regular_balance_object : public object< account_regular_balance_object_type, account_regular_balance_object >
{
public:   
   template < typename Allocator>
   account_regular_balance_object( allocator< Allocator > a, int64_t _id,
      const account_name_type& _owner, asset_symbol_type liquid_symbol )
      : id( _id ), owner( _owner ), liquid( 0, liquid_symbol ), vesting( 0, liquid_symbol.get_paired_symbol() )
   {}

   void on_remove() const
   {
      FC_ASSERT( is_empty(), "SMTs not transfered out before account_regular_balance_object removal." );
   }

   bool is_empty() const
   {
      return ( liquid.amount == 0 ) && ( vesting.amount == 0 );
   }

   asset_symbol_type get_liquid_symbol() const
   {
      return liquid.symbol;
   }

   // id_type is actually oid<account_regular_balance_object>
   id_type             id;
   /// Name of the account, the balance is held for.
   account_name_type   owner;
   BALANCE( liquid, get_liquid ); /// 'balance' for STEEM
   BALANCE( vesting, get_vesting ); /// 'vesting_shares' for VESTS
   
   friend class fc::reflector<account_regular_balance_object>;
};

/**
 * Class responsible for holding reward balance of SMT for given account.
 * It has not been unified with regular balance object counterpart, due to different number
 * of fields needed to hold balances (2 for regular, 3 for reward).
 */
class account_rewards_balance_object : public object< account_rewards_balance_object_type, account_rewards_balance_object >
{
public:   
   template < typename Allocator >
   account_rewards_balance_object( allocator< Allocator > a, int64_t _id,
      const account_name_type& _owner, asset_symbol_type liquid_symbol )
      : id( _id ), owner( _owner ), pending_liquid( 0, liquid_symbol ),
      pending_vesting_shares( 0, liquid_symbol.get_paired_symbol() ), pending_vesting_value( 0, liquid_symbol )
   {}

   void on_remove() const
   {
      FC_ASSERT( is_empty(), "SMTs not transfered out before account_rewards_balance_object removal." );
   }

   bool is_empty() const
   {
      return ( pending_liquid.amount == 0 ) && ( pending_vesting_shares.amount == 0 );
   }

   asset_symbol_type get_liquid_symbol() const
   {
      return pending_liquid.symbol;
   }

   // id_type is actually oid<account_rewards_balance_object>
   id_type id;
   /// Name of the account, the balance is held for.
   account_name_type owner;
   BALANCE( pending_liquid, get_pending_liquid ); /// 'reward_steem_balance' for pending STEEM
   BALANCE( pending_vesting_shares, get_pending_vesting_shares ); /// 'reward_vesting_balance' for pending VESTS
public:
   asset pending_vesting_value;   /// 'reward_vesting_steem' for pending VESTS

   friend class fc::reflector<account_rewards_balance_object>;
};

struct by_owner_liquid_symbol;

typedef multi_index_container <
   account_regular_balance_object,
   indexed_by <
      ordered_unique< tag< by_id >,
         member< account_regular_balance_object, account_regular_balance_id_type, &account_regular_balance_object::id>
      >,
      ordered_unique<tag<by_owner_liquid_symbol>,
         composite_key<account_regular_balance_object,
            member< account_regular_balance_object, account_name_type, &account_regular_balance_object::owner >,
            const_mem_fun< account_regular_balance_object, asset_symbol_type, &account_regular_balance_object::get_liquid_symbol >
         >
      >
   >,
   allocator< account_regular_balance_object >
> account_regular_balance_index;

typedef multi_index_container <
   account_rewards_balance_object,
   indexed_by <
      ordered_unique< tag< by_id >,
         member< account_rewards_balance_object, account_rewards_balance_id_type, &account_rewards_balance_object::id>
      >,
      ordered_unique<tag<by_owner_liquid_symbol>,
         composite_key<account_rewards_balance_object,
            member< account_rewards_balance_object, account_name_type, &account_rewards_balance_object::owner >,
            const_mem_fun< account_rewards_balance_object, asset_symbol_type, &account_rewards_balance_object::get_liquid_symbol >
         >
      >
   >,
   allocator< account_rewards_balance_object >
> account_rewards_balance_index;

} } // namespace steem::chain

FC_REFLECT( steem::chain::account_regular_balance_object,
   (id)
   (owner)
   (liquid)
   (vesting)
)

FC_REFLECT( steem::chain::account_rewards_balance_object,
   (id)
   (owner)
   (pending_liquid)
   (pending_vesting_shares)
   (pending_vesting_value)
)

CHAINBASE_SET_INDEX_TYPE( steem::chain::account_regular_balance_object, steem::chain::account_regular_balance_index )
CHAINBASE_SET_INDEX_TYPE( steem::chain::account_rewards_balance_object, steem::chain::account_rewards_balance_index )

#endif
