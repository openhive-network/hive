#pragma once
#include <hive/chain/external_storage/comment_rocksdb_objects.hpp>

namespace hive { namespace chain {

struct by_block;
struct by_permlink;

typedef multi_index_container<
  volatile_comment_object,
  indexed_by<
    ordered_unique< tag< by_id >,
      const_mem_fun< volatile_comment_object, volatile_comment_object::id_type, &volatile_comment_object::get_id > >,
    ordered_unique< tag< by_block >,
      composite_key< volatile_comment_object,
        member< volatile_comment_object, uint32_t, &volatile_comment_object::block_number>,
        const_mem_fun< volatile_comment_object, volatile_comment_object::id_type, &volatile_comment_object::get_id >
      >
    >,
    ordered_unique< tag< by_permlink >,
      const_mem_fun< volatile_comment_object, const comment_object::author_and_permlink_hash_type&, &volatile_comment_object::get_author_and_permlink_hash > >
  >,
  multi_index_allocator< volatile_comment_object >
> volatile_comment_index;

} } // hive::chain

CHAINBASE_SET_INDEX_TYPE( hive::chain::volatile_comment_object, hive::chain::volatile_comment_index )
