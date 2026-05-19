#pragma once
#include <hive/plugins/transaction_status/transaction_status_objects.hpp>

namespace hive { namespace plugins { namespace transaction_status {

struct by_trx_id {};
struct by_block_num;

typedef multi_index_container<
  transaction_status_object,
  indexed_by<
    ordered_unique< tag< by_id >,
      const_mem_fun< transaction_status_object, transaction_status_object::id_type, &transaction_status_object::get_id > >,
    ordered_unique< tag< by_trx_id >,
      member< transaction_status_object, transaction_id_type, &transaction_status_object::transaction_id > >,
    ordered_unique< tag< by_block_num >,
      composite_key< transaction_status_object,
        member< transaction_status_object, uint32_t, &transaction_status_object::block_num >,
        member< transaction_status_object, transaction_id_type, &transaction_status_object::transaction_id >
      >,
      composite_key_compare< std::less< uint32_t >, std::less< transaction_id_type > >
    >
  >,
  multi_index_allocator< transaction_status_object >
> transaction_status_index;


struct by_block_num {};

typedef multi_index_container<
  transaction_status_block_object,
  indexed_by<
    ordered_unique< tag< by_id >,
      const_mem_fun< transaction_status_block_object, transaction_status_block_object::id_type, &transaction_status_block_object::get_id > >,
    ordered_unique< tag< by_block_num >,
      member< transaction_status_block_object, uint32_t, &transaction_status_block_object::block_num > >
  >,
  multi_index_allocator< transaction_status_block_object >
> transaction_status_block_index;


} } } // hive::plugins::transaction_status

CHAINBASE_SET_INDEX_TYPE( hive::plugins::transaction_status::transaction_status_object, hive::plugins::transaction_status::transaction_status_index )
CHAINBASE_SET_INDEX_TYPE( hive::plugins::transaction_status::transaction_status_block_object, hive::plugins::transaction_status::transaction_status_block_index )
