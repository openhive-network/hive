#pragma once
#include <hive/chain/detail/state/state_stamp_data_object.hpp>

namespace hive { namespace chain {

  typedef multi_index_container<
    state_stamp_data_object,
    indexed_by<
      ordered_unique< tag< by_id >,
        const_mem_fun< state_stamp_data_object, state_stamp_data_object::id_type, &state_stamp_data_object::get_id > >
    >,
    multi_index_allocator< state_stamp_data_object, 2 > // singleton (plus one internal)
  > state_stamp_data_index;

} } // hive::chain

CHAINBASE_SET_INDEX_TYPE( hive::chain::state_stamp_data_object, hive::chain::state_stamp_data_index )
