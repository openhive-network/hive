#pragma once
#include <hive/plugins/metadata/metadata_objects.hpp>

namespace hive { namespace plugins { namespace metadata {

struct by_account {};

typedef multi_index_container <
  account_metadata_object,
  indexed_by<
    ordered_unique< tag< by_id >,
      const_mem_fun< account_metadata_object, account_metadata_object::id_type, &account_metadata_object::get_id > >,
    ordered_unique< tag< by_account >,
      member< account_metadata_object, account_id_type, &account_metadata_object::account > >
  >,
  multi_index_allocator< account_metadata_object >
> account_metadata_index;

}}}

CHAINBASE_SET_INDEX_TYPE( hive::plugins::metadata::account_metadata_object, hive::plugins::metadata::account_metadata_index )
