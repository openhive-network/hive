#pragma once
#include <hive/chain/detail/state/convert_request_object.hpp>

namespace hive { namespace chain {

  struct by_owner;
  struct by_conversion_date;
  typedef multi_index_container<
    convert_request_object,
    indexed_by<
      ordered_unique< tag< by_id >,
        const_mem_fun< convert_request_object, convert_request_object::id_type, &convert_request_object::get_id > >,
      ordered_unique< tag< by_conversion_date >,
        composite_key< convert_request_object,
          const_mem_fun< convert_request_object, time_point_sec, &convert_request_object::get_conversion_date >,
          const_mem_fun< convert_request_object, convert_request_object::id_type, &convert_request_object::get_id >
        >
      >,
      ordered_unique< tag< by_owner >,
        composite_key< convert_request_object,
          const_mem_fun< convert_request_object, account_id_type, &convert_request_object::get_owner >,
          const_mem_fun< convert_request_object, uint32_t, &convert_request_object::get_request_id >
        >
      >
    >,
    multi_index_allocator< convert_request_object >
  > convert_request_index;

} } // hive::chain

CHAINBASE_SET_INDEX_TYPE( hive::chain::convert_request_object, hive::chain::convert_request_index )
