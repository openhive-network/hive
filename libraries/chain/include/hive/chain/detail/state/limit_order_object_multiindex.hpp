#pragma once
#include <hive/chain/detail/state/limit_order_object.hpp>

namespace hive { namespace chain {

  struct by_price;
  struct by_expiration;
  struct by_account;
  typedef multi_index_container<
    limit_order_object,
    indexed_by<
      ordered_unique< tag< by_id >,
        const_mem_fun< limit_order_object, limit_order_object::id_type, &limit_order_object::get_id > >,
      ordered_unique< tag< by_expiration >,
        composite_key< limit_order_object,
          const_mem_fun< limit_order_object, time_point_sec, &limit_order_object::get_expiration >,
          const_mem_fun< limit_order_object, limit_order_object::id_type, &limit_order_object::get_id >
        >
      >,
      ordered_unique< tag< by_price >,
        composite_key< limit_order_object,
          const_mem_fun< limit_order_object, const price&, &limit_order_object::get_sell_price >,
          const_mem_fun< limit_order_object, limit_order_object::id_type, &limit_order_object::get_id >
        >,
        composite_key_compare< std::greater< price >, std::less< limit_order_id_type > >
      >,
      ordered_unique< tag< by_account >,
        composite_key< limit_order_object,
          const_mem_fun< limit_order_object, const account_name_type&, &limit_order_object::get_seller >,
          const_mem_fun< limit_order_object, uint32_t, &limit_order_object::get_orderid >
        >
      >
    >,
    multi_index_allocator< limit_order_object >
  > limit_order_index;

} } // hive::chain

CHAINBASE_SET_INDEX_TYPE( hive::chain::limit_order_object, hive::chain::limit_order_index )
