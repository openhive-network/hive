#pragma once
#include <hive/chain/util/manabar.hpp>

#include <hive/plugins/rc/rc_utility.hpp>
#include <hive/plugins/rc/resource_count.hpp>
#include <hive/chain/database.hpp>

#include <hive/chain/hive_object_types.hpp>

#include <fc/int_array.hpp>

namespace hive { namespace chain {
struct by_account;
} }

namespace hive { namespace plugins { namespace rc {

using namespace std;
using namespace hive::chain;
using hive::protocol::asset;

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
  rc_direct_delegation_object_type  = ( HIVE_RC_SPACE_ID << 8 ) + 3
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

    account_name_type            account;
    hive::chain::util::manabar   rc_manabar;
    asset                        max_rc_creation_adjustment = asset( 0, VESTS_SYMBOL );

    // This is used for bug-catching, to match that the vesting shares in a
    // pre-op are equal to what they were at the last post-op.
    int64_t                      last_max_rc = 0;

    uint64_t                     delegated_rc = 0;
    uint64_t                     received_delegated_rc = 0;
};
typedef oid_ref< rc_account_object > rc_account_id_type;


class rc_direct_delegation_object : public object< rc_direct_delegation_object_type, rc_direct_delegation_object >
{
  CHAINBASE_OBJECT( rc_direct_delegation_object );
  public:
    template< typename Allocator >
    rc_direct_delegation_object( allocator< Allocator > a, uint64_t _id,
    const account_id_type& _from, const account_id_type& _to, const uint64_t& _delegated_rc)
    : id( _id ), from(_from), to(_to), delegated_rc(_delegated_rc) {}

    account_id_type from;
    account_id_type to;
    uint64_t        delegated_rc = 0;
  CHAINBASE_UNPACK_CONSTRUCTOR(rc_direct_delegation_object);
};

int64_t get_maximum_rc( const hive::chain::account_object& account, const rc_account_object& rc_account, bool only_delegable_rc = false );
void update_rc_account_after_delegation( database& _db, const rc_account_object& rc_account, const account_object& account, uint32_t now, int64_t delta, bool regenerate_mana = false );

typedef multi_index_container<
  rc_resource_param_object,
  indexed_by<
    ordered_unique< tag< by_id >,
      const_mem_fun< rc_resource_param_object, rc_resource_param_object::id_type, &rc_resource_param_object::get_id > >
  >,
  allocator< rc_resource_param_object >
> rc_resource_param_index;

typedef multi_index_container<
  rc_pool_object,
  indexed_by<
    ordered_unique< tag< by_id >,
      const_mem_fun< rc_pool_object, rc_pool_object::id_type, &rc_pool_object::get_id > >
  >,
  allocator< rc_pool_object >
> rc_pool_index;

typedef multi_index_container<
  rc_account_object,
  indexed_by<
    ordered_unique< tag< by_id >,
      const_mem_fun< rc_account_object, rc_account_object::id_type, &rc_account_object::get_id > >,
    ordered_unique< tag< by_name >,
      member< rc_account_object, account_name_type, &rc_account_object::account > >
  >,
  allocator< rc_account_object >
> rc_account_index;

struct by_from_to;

typedef multi_index_container<
  rc_direct_delegation_object,
  indexed_by<
    ordered_unique< tag< by_id >,
      const_mem_fun< rc_direct_delegation_object, rc_direct_delegation_object::id_type, &rc_direct_delegation_object::get_id > >,
    ordered_unique< tag< by_from_to >,
      composite_key< rc_direct_delegation_object,
        member< rc_direct_delegation_object, account_id_type, &rc_direct_delegation_object::from >,
        member< rc_direct_delegation_object, account_id_type, &rc_direct_delegation_object::to >
      >
    >
  >,
  allocator< rc_direct_delegation_object >
> rc_direct_delegation_object_index;

} } } // hive::plugins::rc

FC_REFLECT( hive::plugins::rc::rc_resource_param_object, (id)(resource_param_array) )
CHAINBASE_SET_INDEX_TYPE( hive::plugins::rc::rc_resource_param_object, hive::plugins::rc::rc_resource_param_index )

FC_REFLECT( hive::plugins::rc::rc_pool_object, (id)(pool_array) )
CHAINBASE_SET_INDEX_TYPE( hive::plugins::rc::rc_pool_object, hive::plugins::rc::rc_pool_index )

FC_REFLECT( hive::plugins::rc::rc_account_object,
  (id)
  (account)
  (rc_manabar)
  (max_rc_creation_adjustment)
  (last_max_rc)
  (delegated_rc)
  (received_delegated_rc)
  )
CHAINBASE_SET_INDEX_TYPE( hive::plugins::rc::rc_account_object, hive::plugins::rc::rc_account_index )

FC_REFLECT( hive::plugins::rc::rc_direct_delegation_object,
  (id)
  (from)
  (to)
  )
CHAINBASE_SET_INDEX_TYPE( hive::plugins::rc::rc_direct_delegation_object, hive::plugins::rc::rc_direct_delegation_object_index )

