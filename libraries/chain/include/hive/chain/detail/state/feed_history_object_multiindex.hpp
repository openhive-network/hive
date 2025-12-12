#pragma once
#include <hive/chain/detail/state/feed_history_object.hpp>

namespace hive { namespace chain {

  typedef multi_index_container<
    feed_history_object,
    indexed_by<
      ordered_unique< tag< by_id >,
        const_mem_fun< feed_history_object, feed_history_object::id_type, &feed_history_object::get_id > >
    >,
    multi_index_allocator< feed_history_object, 2 > // singleton (plus one internal)
  > feed_history_index;

} } // hive::chain

CHAINBASE_SET_INDEX_TYPE( hive::chain::feed_history_object, hive::chain::feed_history_index )
