#pragma once
#include <hive/chain/detail/state/hardfork_property_object.hpp>

namespace hive { namespace chain {

  typedef multi_index_container<
    hardfork_property_object,
    indexed_by<
      ordered_unique< tag< by_id >,
        const_mem_fun< hardfork_property_object, hardfork_property_object::id_type, &hardfork_property_object::get_id > >
    >,
    multi_index_allocator< hardfork_property_object, 2 > // singleton (plus one internal)
  > hardfork_property_index;

} } // hive::chain

CHAINBASE_SET_INDEX_TYPE( hive::chain::hardfork_property_object, hive::chain::hardfork_property_index )
