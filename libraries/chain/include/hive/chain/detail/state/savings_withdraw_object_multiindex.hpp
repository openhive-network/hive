#pragma once
#include <hive/chain/detail/state/savings_withdraw_object.hpp>

namespace hive { namespace chain {

  struct by_from_rid {};
  struct by_to_complete;
  struct by_complete_from_rid;
  typedef multi_index_container<
    savings_withdraw_object,
    indexed_by<
      ordered_unique< tag< by_id >,
        const_mem_fun< savings_withdraw_object, savings_withdraw_object::id_type, &savings_withdraw_object::get_id > >,
      ordered_unique< tag< by_from_rid >,
        composite_key< savings_withdraw_object,
          const_mem_fun< savings_withdraw_object, const account_name_type&, &savings_withdraw_object::get_from >,
          const_mem_fun< savings_withdraw_object, uint32_t, &savings_withdraw_object::get_request_id >
        >
      >,
      ordered_unique< tag< by_complete_from_rid >,
        composite_key< savings_withdraw_object,
          const_mem_fun< savings_withdraw_object, time_point_sec, &savings_withdraw_object::get_completion_time >,
          const_mem_fun< savings_withdraw_object, const account_name_type&, &savings_withdraw_object::get_from >,
          const_mem_fun< savings_withdraw_object, uint32_t, &savings_withdraw_object::get_request_id >
        >
      >,
      ordered_unique< tag< by_to_complete >,
        composite_key< savings_withdraw_object,
          const_mem_fun< savings_withdraw_object, const account_name_type&, &savings_withdraw_object::get_to >,
          const_mem_fun< savings_withdraw_object, time_point_sec, &savings_withdraw_object::get_completion_time >,
          const_mem_fun< savings_withdraw_object, savings_withdraw_object::id_type, &savings_withdraw_object::get_id >
        >
      >
    >,
    multi_index_allocator< savings_withdraw_object >
  > savings_withdraw_index;

} } // hive::chain

CHAINBASE_SET_INDEX_TYPE( hive::chain::savings_withdraw_object, hive::chain::savings_withdraw_index )
