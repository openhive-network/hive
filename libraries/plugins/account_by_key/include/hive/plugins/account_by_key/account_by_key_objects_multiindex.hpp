#pragma once
#include <hive/plugins/account_by_key/account_by_key_objects.hpp>

namespace hive { namespace plugins { namespace account_by_key {

struct by_key;

typedef multi_index_container<
  key_lookup_object,
  indexed_by<
    ordered_unique< tag< by_id >,
      const_mem_fun< key_lookup_object, key_lookup_object::id_type, &key_lookup_object::get_id > >,
    ordered_unique< tag< by_key >,
      composite_key< key_lookup_object,
        member< key_lookup_object, public_key_type, &key_lookup_object::key >,
        member< key_lookup_object, account_name_type, &key_lookup_object::account >
      >
    >
  >,
  multi_index_allocator< key_lookup_object >
> key_lookup_index;

} } } // hive::plugins::account_by_key

CHAINBASE_SET_INDEX_TYPE( hive::plugins::account_by_key::key_lookup_object, hive::plugins::account_by_key::key_lookup_index )
