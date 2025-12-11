#pragma once
#include <hive/chain/detail/state/global_property_object.hpp>

namespace hive { namespace chain {

  typedef multi_index_container<
    dynamic_global_property_object,
    indexed_by<
      ordered_unique< tag< by_id >,
        const_mem_fun< dynamic_global_property_object, dynamic_global_property_object::id_type, &dynamic_global_property_object::get_id > >
    >,
    multi_index_allocator< dynamic_global_property_object, 2 > // singleton (plus one internal)
  > dynamic_global_property_index;

} } // hive::chain

CHAINBASE_SET_INDEX_TYPE( hive::chain::dynamic_global_property_object, hive::chain::dynamic_global_property_index )
