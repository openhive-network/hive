#pragma once
#include <hive/plugins/account_history_rocksdb/account_history_rocksdb_objects.hpp>

namespace hive { namespace plugins { namespace account_history_rocksdb {

struct by_block;

typedef multi_index_container<
  volatile_operation_object,
  indexed_by<
    ordered_unique< tag< by_id >,
      const_mem_fun< volatile_operation_object, volatile_operation_object::id_type, &volatile_operation_object::get_id > >,
    ordered_unique< tag< by_block >,
      composite_key< volatile_operation_object,
        member< volatile_operation_object, uint32_t, &volatile_operation_object::block>,
        const_mem_fun< volatile_operation_object, volatile_operation_object::id_type, &volatile_operation_object::get_id >
      >
    >
  >,
  multi_index_allocator< volatile_operation_object >
> volatile_operation_index;

} } } // hive::plugins::account_history_rocksdb

CHAINBASE_SET_INDEX_TYPE( hive::plugins::account_history_rocksdb::volatile_operation_object, hive::plugins::account_history_rocksdb::volatile_operation_index )
