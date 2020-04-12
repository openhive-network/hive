#pragma once
#include <hive/chain/util/manabar.hpp>

#include <hive/plugins/rc/rc_utility.hpp>
#include <hive/plugins/rc/resource_count.hpp>

#include <hive/chain/hive_object_types.hpp>
#include <hive/protocol/asset.hpp>

#include <fc/int_array.hpp>

namespace hive { namespace plugins { namespace rc {

using namespace hive::chain;
using hive::protocol::asset;
using hive::protocol::asset_symbol_type;

#ifndef HIVE_RC_SPACE_ID
#define HIVE_RC_SPACE_ID 16
#endif

#define HIVE_RC_DRC_FLOAT_LEVEL   (20*HIVE_1_PERCENT)
#define HIVE_RC_MAX_DRC_RATE      1000

enum rc_object_types
{
  rc_resource_param_object_type   = ( HIVE_RC_SPACE_ID << 8 ),
  rc_pool_object_type             = ( HIVE_RC_SPACE_ID << 8 ) + 1,
  rc_account_object_type          = ( HIVE_RC_SPACE_ID << 8 ) + 2,
  rc_delegation_pool_object_type  = ( HIVE_RC_SPACE_ID << 8 ) + 3,
  rc_delegation_from_account_object_type = ( HIVE_RC_SPACE_ID << 8 ) + 4,
  rc_indel_edge_object_type       = ( HIVE_RC_SPACE_ID << 8 ) + 5,
  rc_outdel_drc_edge_object_type  = ( HIVE_RC_SPACE_ID << 8 ) + 6
};

class rc_resource_param_object : public object< rc_resource_param_object_type, rc_resource_param_object >
{
  CHAINBASE_OBJECT( rc_resource_param_object );
  public:
    CHAINBASE_DEFAULT_CONSTRUCTOR( rc_resource_param_object )

    fc::int_array< rc_resource_params, HIVE_NUM_RESOURCE_TYPES >
                    resource_param_array;
};
typedef oid_ref< rc_resource_param_object > rc_resource_param_id_type;

class rc_pool_object : public object< rc_pool_object_type, rc_pool_object >
{
  CHAINBASE_OBJECT( rc_pool_object );
  public:
    CHAINBASE_DEFAULT_CONSTRUCTOR( rc_pool_object )

    fc::int_array< int64_t, HIVE_NUM_RESOURCE_TYPES >
                    pool_array;
};
typedef oid_ref< rc_pool_object > rc_pool_id_type;

class rc_account_object : public object< rc_account_object_type, rc_account_object >
{
  CHAINBASE_OBJECT( rc_account_object );
  public:
    CHAINBASE_DEFAULT_CONSTRUCTOR( rc_account_object )

    account_name_type     account;
    account_name_type     creator;
    hive::chain::util::manabar   rc_manabar;
    asset                 max_rc_creation_adjustment = asset( 0, VESTS_SYMBOL );
    asset                 vests_delegated_to_pools = asset( 0, VESTS_SYMBOL );
    fc::array< account_name_type, HIVE_RC_MAX_SLOTS > indel_slots;

    uint32_t              out_delegations = 0;
    // This is used for bug-catching, to match that the vesting shares in a
    // pre-op are equal to what they were at the last post-op.
    int64_t               last_max_rc = 0;
};

typedef oid_ref< rc_account_object > rc_account_id_type;

/**
  * Represents a delegation pool.
  */
class rc_delegation_pool_object : public object< rc_delegation_pool_object_type, rc_delegation_pool_object >
{
  CHAINBASE_OBJECT( rc_delegation_pool_object );
  public:
    CHAINBASE_DEFAULT_CONSTRUCTOR( rc_delegation_pool_object )

    account_name_type             account;
    asset_symbol_type             asset_symbol;

    hive::chain::util::manabar   rc_pool_manabar;
    int64_t                       max_rc = 0;
};

typedef oid_ref< rc_delegation_pool_object > rc_delegation_pool_id_type;

/**
 * Represents the total amount of an asset delegated by a user.
 *
 * Only used for SMT support.
 */
class rc_delegation_from_account_object : public object< rc_delegation_from_account_object_type, rc_delegation_from_account_object >
{
  CHAINBASE_OBJECT( rc_delegation_from_account_object );
  public:
    CHAINBASE_DEFAULT_CONSTRUCTOR(rc_delegation_from_account_object)

     account_name_type             account;
     asset                         amount;

     asset_symbol_type get_asset_symbol()const{ return amount.symbol; }
};

typedef oid_ref< rc_delegation_from_account_object > rc_delegation_from_account_id_type;

/**
  * Represents a delegation from a user to a pool.
  */
class rc_indel_edge_object : public object< rc_indel_edge_object_type, rc_indel_edge_object >
{
  CHAINBASE_OBJECT( rc_indel_edge_object );
  public:
    CHAINBASE_DEFAULT_CONSTRUCTOR( rc_indel_edge_object )

    account_name_type             from_account;
    account_name_type             to_pool;
    asset                         amount;

    asset_symbol_type get_asset_symbol()const { return amount.symbol; }
};

typedef oid_ref< rc_indel_edge_object > rc_indel_edge_id_type;

/**
  * Represents a delegation from a pool to a user based on delegated resource credits (DRC).
  *
  * In the case of a pool that is not under heavy load, DRC:RC has a 1:1 exchange rate.
  *
  * However, if the pool drops below HIVE_RC_DRC_FLOAT_LEVEL, DRC:RC exchange rate starts
  * to rise according to `f(x) = 1/(a+b*x)` where `x` is the pool level, and coefficients `a`,
  * `b` are set such that `f(HIVE_RC_DRC_FLOAT_LEVEL) = 1` and `f(0) = HIVE_RC_MAX_DRC_RATE`.
  *
  * This ensures the limited RC of oversubscribed pools under heavy load are
  * shared "fairly" among their users proportionally to DRC.  This logic
  * provides a smooth transition between the "fiat regime" (i.e. DRC represent
  * a direct allocation of RC) and the "proportional regime" (i.e. DRC represent
  * the fraction of RC that the user is allowed).
  */
class rc_outdel_drc_edge_object : public object< rc_outdel_drc_edge_object_type, rc_outdel_drc_edge_object >
{
  CHAINBASE_OBJECT( rc_outdel_drc_edge_object );
  public:
    CHAINBASE_DEFAULT_CONSTRUCTOR( rc_outdel_drc_edge_object )

    account_name_type             from_pool;
    account_name_type             to_account;
    asset_symbol_type             asset_symbol;
    hive::chain::util::manabar    drc_manabar;
    int64_t                       drc_max_mana = 0;
};

int64_t get_maximum_rc( const hive::chain::account_object& account, const rc_account_object& rc_account );

struct by_edge;
struct by_account_symbol;
struct by_pool;

typedef multi_index_container<
   rc_resource_param_object,
   indexed_by<
      ordered_unique< tag< by_id >, const_mem_fun< rc_resource_param_object, rc_resource_param_object::id_type, &rc_resource_param_object::get_id > >
   >,
   allocator< rc_resource_param_object >
> rc_resource_param_index;

typedef multi_index_container<
   rc_pool_object,
   indexed_by<
      ordered_unique< tag< by_id >, const_mem_fun< rc_pool_object, rc_pool_object::id_type, &rc_pool_object::get_id > >
   >,
   allocator< rc_pool_object >
> rc_pool_index;

typedef multi_index_container<
   rc_account_object,
   indexed_by<
      ordered_unique< tag< by_id >, const_mem_fun< rc_account_object, rc_account_object::id_type, &rc_account_object::get_id > >,
      ordered_unique< tag< by_name >, member< rc_account_object, account_name_type, &rc_account_object::account > >
   >,
   allocator< rc_account_object >
> rc_account_index;

typedef multi_index_container<
   rc_delegation_pool_object,
   indexed_by<
      ordered_unique< tag< by_id >, const_mem_fun< rc_delegation_pool_object, rc_delegation_pool_object::id_type, &rc_delegation_pool_object::get_id > >,
      ordered_unique< tag< by_account_symbol >,
      composite_key< rc_delegation_pool_object,
            member< rc_delegation_pool_object, account_name_type, &rc_delegation_pool_object::account >,
            member< rc_delegation_pool_object, asset_symbol_type, &rc_delegation_pool_object::asset_symbol >
         >
      >
   >,
   allocator< rc_delegation_pool_object >
> rc_delegation_pool_index;

typedef multi_index_container<
   rc_delegation_from_account_object,
   indexed_by<
      ordered_unique< tag< by_id >, const_mem_fun< rc_delegation_from_account_object, rc_delegation_from_account_object::id_type, &rc_delegation_from_account_object::get_id > >,
      ordered_unique< tag< by_account_symbol >,
         composite_key< rc_delegation_from_account_object,
            member< rc_delegation_from_account_object, account_name_type, &rc_delegation_from_account_object::account >,
            const_mem_fun< rc_delegation_from_account_object, asset_symbol_type, &rc_delegation_from_account_object::get_asset_symbol >
         >
      >
   >,
   allocator< rc_delegation_from_account_object >
> rc_delegation_from_account_index;

typedef multi_index_container<
   rc_indel_edge_object,
   indexed_by<
      ordered_unique< tag< by_id >, const_mem_fun< rc_indel_edge_object, rc_indel_edge_object::id_type, &rc_indel_edge_object::get_id > >,
      ordered_unique< tag< by_edge >,
         composite_key< rc_indel_edge_object,
            member< rc_indel_edge_object, account_name_type, &rc_indel_edge_object::from_account >,
            const_mem_fun< rc_indel_edge_object, asset_symbol_type, &rc_indel_edge_object::get_asset_symbol >,
            member< rc_indel_edge_object, account_name_type, &rc_indel_edge_object::to_pool >
         >
      >,
      ordered_unique< tag< by_pool >,
         composite_key< rc_indel_edge_object,
            member< rc_indel_edge_object, account_name_type, &rc_indel_edge_object::to_pool >,
            const_mem_fun< rc_indel_edge_object, asset_symbol_type, &rc_indel_edge_object::get_asset_symbol >,
            member< rc_indel_edge_object, account_name_type, &rc_indel_edge_object::from_account >
         >
      >
   >,
   allocator< rc_indel_edge_object >
> rc_indel_edge_index;

typedef multi_index_container<
   rc_outdel_drc_edge_object,
   indexed_by<
      ordered_unique< tag< by_id >, const_mem_fun< rc_outdel_drc_edge_object, rc_outdel_drc_edge_object::id_type, &rc_outdel_drc_edge_object::get_id > >,
      ordered_unique< tag< by_edge >,
         composite_key< rc_outdel_drc_edge_object,
            member< rc_outdel_drc_edge_object, account_name_type, &rc_outdel_drc_edge_object::from_pool >,
            member< rc_outdel_drc_edge_object, account_name_type, &rc_outdel_drc_edge_object::to_account >,
            member< rc_outdel_drc_edge_object, asset_symbol_type, &rc_outdel_drc_edge_object::asset_symbol >
         >
      >,
      ordered_unique< tag< by_pool >,
         composite_key< rc_outdel_drc_edge_object,
            member< rc_outdel_drc_edge_object, account_name_type, &rc_outdel_drc_edge_object::from_pool >,
            member< rc_outdel_drc_edge_object, asset_symbol_type, &rc_outdel_drc_edge_object::asset_symbol >,
            const_mem_fun< rc_outdel_drc_edge_object, rc_outdel_drc_edge_object::id_type, &rc_outdel_drc_edge_object::get_id >
         >
      >
   >,
   allocator< rc_outdel_drc_edge_object >
> rc_outdel_drc_edge_index;

} } } // hive::plugins::rc

FC_REFLECT( hive::plugins::rc::rc_resource_param_object, (id)(resource_param_array) )
CHAINBASE_SET_INDEX_TYPE( hive::plugins::rc::rc_resource_param_object, hive::plugins::rc::rc_resource_param_index )

FC_REFLECT( hive::plugins::rc::rc_pool_object, (id)(pool_array) )
CHAINBASE_SET_INDEX_TYPE( hive::plugins::rc::rc_pool_object, hive::plugins::rc::rc_pool_index )

FC_REFLECT( hive::plugins::rc::rc_account_object,
  (id)
  (account)
  (creator)
  (rc_manabar)
  (max_rc_creation_adjustment)
  (vests_delegated_to_pools)
  (out_delegations)
  (indel_slots)
  (last_max_rc)
)
CHAINBASE_SET_INDEX_TYPE( hive::plugins::rc::rc_account_object, hive::plugins::rc::rc_account_index )

FC_REFLECT( hive::plugins::rc::rc_delegation_pool_object,
  (id)
  (account)
  (asset_symbol)
  (rc_pool_manabar)
  (max_rc)
  )
CHAINBASE_SET_INDEX_TYPE( hive::plugins::rc::rc_delegation_pool_object, hive::plugins::rc::rc_delegation_pool_index )

FC_REFLECT( hive::plugins::rc::rc_delegation_from_account_object,
            (id)
            (account)
            (amount)
          )
CHAINBASE_SET_INDEX_TYPE( hive::plugins::rc::rc_delegation_from_account_object, hive::plugins::rc::rc_delegation_from_account_index )


FC_REFLECT( hive::plugins::rc::rc_indel_edge_object,
  (id)
  (from_account)
  (to_pool)
  (amount)
  )
CHAINBASE_SET_INDEX_TYPE( hive::plugins::rc::rc_indel_edge_object, hive::plugins::rc::rc_indel_edge_index )

FC_REFLECT( hive::plugins::rc::rc_outdel_drc_edge_object,
  (id)
  (from_pool)
  (to_account)
  (asset_symbol)
  (drc_manabar)
  (drc_max_mana)
  )
CHAINBASE_SET_INDEX_TYPE( hive::plugins::rc::rc_outdel_drc_edge_object, hive::plugins::rc::rc_outdel_drc_edge_index )
