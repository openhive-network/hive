#pragma once
#include <hive/chain/detail/state/collateralized_convert_request_object.hpp>

namespace hive { namespace chain {

  struct by_owner;
  struct by_conversion_date;
  typedef multi_index_container<
    collateralized_convert_request_object,
    indexed_by<
      ordered_unique< tag< by_id >,
        const_mem_fun< collateralized_convert_request_object, collateralized_convert_request_object::id_type, &collateralized_convert_request_object::get_id > >,
      ordered_unique< tag< by_conversion_date >,
        composite_key< collateralized_convert_request_object,
          const_mem_fun< collateralized_convert_request_object, time_point_sec, &collateralized_convert_request_object::get_conversion_date >,
          const_mem_fun< collateralized_convert_request_object, collateralized_convert_request_object::id_type, &collateralized_convert_request_object::get_id >
        >
      >,
      ordered_unique< tag< by_owner >,
        composite_key< collateralized_convert_request_object,
          const_mem_fun< collateralized_convert_request_object, account_id_type, &collateralized_convert_request_object::get_owner >,
          const_mem_fun< collateralized_convert_request_object, uint32_t, &collateralized_convert_request_object::get_request_id >
        >
      >
    >,
    multi_index_allocator< collateralized_convert_request_object >
  > collateralized_convert_request_index;

} } // hive::chain

CHAINBASE_SET_INDEX_TYPE( hive::chain::collateralized_convert_request_object, hive::chain::collateralized_convert_request_index )
