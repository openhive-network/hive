#pragma once
#include <hive/chain/detail/state/recurrent_transfer_object.hpp>

namespace hive { namespace chain {

  struct by_from_to_id;
  struct by_from_id;
  struct by_trigger_date;
  typedef multi_index_container<
    recurrent_transfer_object,
    indexed_by<
      ordered_unique< tag< by_id >,
        const_mem_fun< recurrent_transfer_object, recurrent_transfer_object::id_type, &recurrent_transfer_object::get_id > >,
      ordered_unique< tag< by_trigger_date >,
        composite_key< recurrent_transfer_object,
          const_mem_fun< recurrent_transfer_object, time_point_sec, &recurrent_transfer_object::get_trigger_date >,
          const_mem_fun< recurrent_transfer_object, recurrent_transfer_object::id_type, &recurrent_transfer_object::get_id >
        >
      >,
      ordered_unique< tag< by_from_id >,
        composite_key< recurrent_transfer_object,
          member< recurrent_transfer_object, account_id_type, &recurrent_transfer_object::from_id >,
          const_mem_fun< recurrent_transfer_object, recurrent_transfer_object::id_type, &recurrent_transfer_object::get_id >
        >
      >,
      ordered_unique< tag< by_from_to_id >,
        composite_key< recurrent_transfer_object,
          member< recurrent_transfer_object, account_id_type, &recurrent_transfer_object::from_id >,
          member< recurrent_transfer_object, account_id_type, &recurrent_transfer_object::to_id >,
          member< recurrent_transfer_object, uint8_t, &recurrent_transfer_object::pair_id >
        >,
        composite_key_compare< std::less< account_id_type >, std::less< account_id_type >, std::less< uint8_t > >
      >
    >,
    multi_index_allocator< recurrent_transfer_object >
  > recurrent_transfer_index;

} } // hive::chain

CHAINBASE_SET_INDEX_TYPE( hive::chain::recurrent_transfer_object, hive::chain::recurrent_transfer_index )
