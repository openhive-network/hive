#pragma once
#include <hive/chain/detail/state/escrow_object.hpp>

namespace hive { namespace chain {

  struct by_from_id {};
  struct by_ratification_deadline;
  typedef multi_index_container<
    escrow_object,
    indexed_by<
      ordered_unique< tag< by_id >,
        const_mem_fun< escrow_object, escrow_object::id_type, &escrow_object::get_id > >,
      ordered_unique< tag< by_from_id >,
        composite_key< escrow_object,
          const_mem_fun< escrow_object, const account_name_type&, &escrow_object::get_from >,
          const_mem_fun< escrow_object, uint32_t, &escrow_object::get_escrow_id >
        >
      >,
      ordered_unique< tag< by_ratification_deadline >,
        composite_key< escrow_object,
          const_mem_fun< escrow_object, bool, &escrow_object::is_approved >,
          const_mem_fun< escrow_object, time_point_sec, &escrow_object::get_ratification_deadline >,
          const_mem_fun< escrow_object, escrow_object::id_type, &escrow_object::get_id >
        >,
        composite_key_compare< std::less< bool >, std::less< time_point_sec >, std::less< escrow_id_type > >
      >
    >,
    multi_index_allocator< escrow_object >
  > escrow_index;

} } // hive::chain

CHAINBASE_SET_INDEX_TYPE( hive::chain::escrow_object, hive::chain::escrow_index )
