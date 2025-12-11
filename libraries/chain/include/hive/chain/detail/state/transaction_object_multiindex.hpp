#pragma once
#include <hive/chain/detail/state/transaction_object.hpp>

namespace hive { namespace chain {

  struct by_expiration;
  struct by_trx_id;
  typedef multi_index_container<
    transaction_object,
    indexed_by<
      ordered_unique< tag< by_id >,
        const_mem_fun< transaction_object, transaction_object::id_type, &transaction_object::get_id > >,
      ordered_unique< tag< by_trx_id >,
        member< transaction_object, transaction_id_type, &transaction_object::trx_id > >,
      ordered_unique< tag< by_expiration >,
        composite_key< transaction_object,
          member<transaction_object, time_point_sec, &transaction_object::expiration >,
          const_mem_fun<transaction_object, transaction_object::id_type, &transaction_object::get_id >
        >
      >
    >,
    multi_index_allocator< transaction_object >
  > transaction_index;

} } // hive::chain

CHAINBASE_SET_INDEX_TYPE( hive::chain::transaction_object, hive::chain::transaction_index )
