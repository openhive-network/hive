#pragma once
#include <hive/chain/detail/state/withdraw_vesting_route_object.hpp>

namespace hive { namespace chain {

  struct by_withdraw_route;
  struct by_destination;
  typedef multi_index_container<
    withdraw_vesting_route_object,
    indexed_by<
      ordered_unique< tag< by_id >,
        const_mem_fun< withdraw_vesting_route_object, withdraw_vesting_route_object::id_type, &withdraw_vesting_route_object::get_id > >,
      ordered_unique< tag< by_withdraw_route >,
        composite_key< withdraw_vesting_route_object,
          member< withdraw_vesting_route_object, account_name_type, &withdraw_vesting_route_object::from_account >,
          member< withdraw_vesting_route_object, account_name_type, &withdraw_vesting_route_object::to_account >
        >,
        composite_key_compare< std::less< account_name_type >, std::less< account_name_type > >
      >,
      ordered_unique< tag< by_destination >,
        composite_key< withdraw_vesting_route_object,
          member< withdraw_vesting_route_object, account_name_type, &withdraw_vesting_route_object::to_account >,
          const_mem_fun< withdraw_vesting_route_object, withdraw_vesting_route_object::id_type, &withdraw_vesting_route_object::get_id >
        >
      >
    >,
    multi_index_allocator< withdraw_vesting_route_object >
  > withdraw_vesting_route_index;

} } // hive::chain

CHAINBASE_SET_INDEX_TYPE( hive::chain::withdraw_vesting_route_object, hive::chain::withdraw_vesting_route_index )
