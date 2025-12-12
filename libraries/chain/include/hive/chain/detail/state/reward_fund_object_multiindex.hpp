#pragma once
#include <hive/chain/detail/state/reward_fund_object.hpp>

namespace hive { namespace chain {

  typedef multi_index_container<
    reward_fund_object,
    indexed_by<
      ordered_unique< tag< by_id >,
        const_mem_fun< reward_fund_object, reward_fund_object::id_type, &reward_fund_object::get_id > >,
      ordered_unique< tag< by_name >,
        member< reward_fund_object, reward_fund_name_type, &reward_fund_object::name > >
    >,
    multi_index_allocator< reward_fund_object, 2 > // singleton (plus one internal)
  > reward_fund_index;

} } // hive::chain

CHAINBASE_SET_INDEX_TYPE( hive::chain::reward_fund_object, hive::chain::reward_fund_index )
