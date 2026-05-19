#pragma once
#include <hive/plugins/block_log_info/block_log_info_objects.hpp>

namespace hive { namespace plugins { namespace block_log_info {

typedef multi_index_container<
  block_log_hash_state_object,
  indexed_by<
    ordered_unique< tag< by_id >,
      const_mem_fun< block_log_hash_state_object, block_log_hash_state_object::id_type, &block_log_hash_state_object::get_id > >
  >,
  multi_index_allocator< block_log_hash_state_object >
> block_log_hash_state_index;

typedef multi_index_container<
  block_log_pending_message_object,
  indexed_by<
    ordered_unique< tag< by_id >,
      const_mem_fun< block_log_pending_message_object, block_log_pending_message_object::id_type, &block_log_pending_message_object::get_id > >
  >,
  multi_index_allocator< block_log_pending_message_object >
> block_log_pending_message_index;

} } } // hive::plugins::block_log_info

CHAINBASE_SET_INDEX_TYPE( hive::plugins::block_log_info::block_log_hash_state_object, hive::plugins::block_log_info::block_log_hash_state_index )
CHAINBASE_SET_INDEX_TYPE( hive::plugins::block_log_info::block_log_pending_message_object, hive::plugins::block_log_info::block_log_pending_message_index )
