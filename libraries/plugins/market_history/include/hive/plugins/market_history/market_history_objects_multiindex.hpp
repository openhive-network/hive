#pragma once
#include <hive/plugins/market_history/market_history_objects.hpp>

namespace hive { namespace plugins { namespace market_history {

struct by_bucket;
typedef multi_index_container<
  bucket_object,
  indexed_by<
    ordered_unique< tag< by_id >,
      const_mem_fun< bucket_object, bucket_object::id_type, &bucket_object::get_id > >,
    ordered_unique< tag< by_bucket >,
      composite_key< bucket_object,
        member< bucket_object, uint32_t, &bucket_object::seconds >,
        member< bucket_object, fc::time_point_sec, &bucket_object::open >
      >,
      composite_key_compare< std::less< uint32_t >, std::less< fc::time_point_sec > >
    >
  >,
  multi_index_allocator< bucket_object >
> bucket_index;

struct by_time;
typedef multi_index_container<
  order_history_object,
  indexed_by<
    ordered_unique< tag< by_id >,
      const_mem_fun< order_history_object, order_history_object::id_type, &order_history_object::get_id > >,
    ordered_unique< tag< by_time >,
      composite_key< order_history_object,
        member< order_history_object, time_point_sec, &order_history_object::time >,
        const_mem_fun< order_history_object, order_history_object::id_type, &order_history_object::get_id >
      >
    >
  >,
  multi_index_allocator< order_history_object >
> order_history_index;

} } } // hive::plugins::market_history

CHAINBASE_SET_INDEX_TYPE( hive::plugins::market_history::bucket_object, hive::plugins::market_history::bucket_index )
CHAINBASE_SET_INDEX_TYPE( hive::plugins::market_history::order_history_object, hive::plugins::market_history::order_history_index )
