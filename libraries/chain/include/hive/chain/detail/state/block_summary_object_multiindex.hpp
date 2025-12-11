#pragma once
#include <hive/chain/detail/state/block_summary_object.hpp>

namespace hive { namespace chain {

  typedef multi_index_container<
    block_summary_object,
    indexed_by<
      ordered_unique< tag< by_id >,
        const_mem_fun< block_summary_object, block_summary_object::id_type, &block_summary_object::get_id > >
    >,
    multi_index_allocator< block_summary_object > // multiton (exactly 64k objects plus one internal)
  > block_summary_index;

} } // hive::chain

CHAINBASE_SET_INDEX_TYPE( hive::chain::block_summary_object, hive::chain::block_summary_index )

