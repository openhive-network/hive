#pragma once

#include <hive/chain/detail/state/scoped_authority_object.hpp>
#include <boost/multi_index/composite_key.hpp>

namespace hive { namespace chain {

struct by_grantor_id {};
struct by_scoped_actor {};

using scoped_authority_index = multi_index_container<
  scoped_authority_object,
  indexed_by<
    ordered_unique< tag< by_id >,
      const_mem_fun< scoped_authority_object, scoped_authority_object::id_type,
        &scoped_authority_object::get_id > >,
    ordered_unique< tag< by_grantor_id >,
      composite_key< scoped_authority_object,
        member< scoped_authority_object, account_name_type, &scoped_authority_object::grantor >,
        member< scoped_authority_object, uint32_t, &scoped_authority_object::grant_id > > >,
    ordered_unique< tag< by_scoped_actor >,
      composite_key< scoped_authority_object,
        member< scoped_authority_object, account_name_type, &scoped_authority_object::actor >,
        member< scoped_authority_object, account_name_type, &scoped_authority_object::grantor >,
        member< scoped_authority_object, uint32_t, &scoped_authority_object::grant_id > > >
  >,
  multi_index_allocator< scoped_authority_object >
>;

} } // hive::chain

CHAINBASE_SET_INDEX_TYPE( hive::chain::scoped_authority_object, hive::chain::scoped_authority_index )
