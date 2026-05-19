#pragma once
#include <hive/plugins/reputation/reputation_objects.hpp>

namespace hive { namespace plugins { namespace reputation {

typedef multi_index_container<
  reputation_object,
  indexed_by<
    ordered_unique< tag< by_id >,
      const_mem_fun< reputation_object, reputation_object::id_type, &reputation_object::get_id > >,
    ordered_unique< tag< by_account >,
      member< reputation_object, account_name_type, &reputation_object::account > >
  >,
  multi_index_allocator< reputation_object >
> reputation_index;

} } } // hive::plugins::reputation

CHAINBASE_SET_INDEX_TYPE( hive::plugins::reputation::reputation_object, hive::plugins::reputation::reputation_index )
