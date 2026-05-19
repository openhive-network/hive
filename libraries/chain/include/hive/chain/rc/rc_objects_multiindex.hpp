#pragma once
#include <hive/chain/rc/rc_objects.hpp>

namespace hive { namespace chain {

typedef multi_index_container<
  rc_resource_param_object,
  indexed_by<
    ordered_unique< tag< by_id >,
      const_mem_fun< rc_resource_param_object, rc_resource_param_object::id_type, &rc_resource_param_object::get_id > >
  >,
  multi_index_allocator< rc_resource_param_object, 2 > // singleton (plus one internal)
> rc_resource_param_index;

typedef multi_index_container<
  rc_pool_object,
  indexed_by<
    ordered_unique< tag< by_id >,
      const_mem_fun< rc_pool_object, rc_pool_object::id_type, &rc_pool_object::get_id > >
  >,
  multi_index_allocator< rc_pool_object, 2 > // singleton (plus one internal)
> rc_pool_index;

typedef multi_index_container<
  rc_stats_object,
  indexed_by<
    ordered_unique< tag< by_id >,
      const_mem_fun< rc_stats_object, rc_stats_object::id_type, &rc_stats_object::get_id > >
  >,
  multi_index_allocator< rc_stats_object, 4 > // dubleton (plus one internal)
> rc_stats_index;

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
  multi_index_allocator< rc_direct_delegation_object >
> rc_direct_delegation_index;

typedef multi_index_container<
  rc_expired_delegation_object,
  indexed_by<
    ordered_unique< tag< by_id >,
      const_mem_fun< rc_expired_delegation_object, rc_expired_delegation_object::id_type, &rc_expired_delegation_object::get_id > >,
    ordered_unique< tag< by_account >,
      member< rc_expired_delegation_object, account_id_type, &rc_expired_delegation_object::from > >
  >,
  multi_index_allocator< rc_expired_delegation_object >
> rc_expired_delegation_index;

struct by_timestamp;

typedef multi_index_container<
  rc_usage_bucket_object,
  indexed_by<
    ordered_unique< tag< by_id >,
      const_mem_fun< rc_usage_bucket_object, rc_usage_bucket_object::id_type, &rc_usage_bucket_object::get_id > >,
    ordered_unique< tag< by_timestamp >,
      const_mem_fun< rc_usage_bucket_object, time_point_sec, &rc_usage_bucket_object::get_timestamp > >
  >,
  multi_index_allocator< rc_usage_bucket_object, 32 > // multiton (always 24 plus one internal)
> rc_usage_bucket_index;

} } // hive::chain

CHAINBASE_SET_INDEX_TYPE( hive::chain::rc_resource_param_object, hive::chain::rc_resource_param_index )
CHAINBASE_SET_INDEX_TYPE( hive::chain::rc_pool_object, hive::chain::rc_pool_index )
CHAINBASE_SET_INDEX_TYPE( hive::chain::rc_stats_object, hive::chain::rc_stats_index )
CHAINBASE_SET_INDEX_TYPE( hive::chain::rc_direct_delegation_object, hive::chain::rc_direct_delegation_index )
CHAINBASE_SET_INDEX_TYPE( hive::chain::rc_expired_delegation_object, hive::chain::rc_expired_delegation_index )
CHAINBASE_SET_INDEX_TYPE( hive::chain::rc_usage_bucket_object, hive::chain::rc_usage_bucket_index )
