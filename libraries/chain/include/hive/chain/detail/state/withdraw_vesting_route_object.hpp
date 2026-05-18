#pragma once
#include <hive/chain/hive_fwd.hpp>
#include <hive/chain/hive_object_types.hpp>

namespace hive { namespace chain {

/**
  * @breif a route to send withdrawn vesting shares.
  */
class withdraw_vesting_route_object : public object< withdraw_vesting_route_object_type, withdraw_vesting_route_object >
{
  CHAINBASE_OBJECT( withdraw_vesting_route_object, true );
public:
  CHAINBASE_DEFAULT_CONSTRUCTOR( withdraw_vesting_route_object )

  account_name_type from_account; // cannot be made into account_object_id because power down uses name order
  account_name_type to_account; // cannot be made into account_object_id because power down uses name order
  uint16_t          percent = 0;
  bool              auto_vest = false;

  CHAINBASE_UNPACK_CONSTRUCTOR( withdraw_vesting_route_object );
};

} } // hive::chain

FC_REFLECT( hive::chain::withdraw_vesting_route_object,
          (id)(from_account)(to_account)(percent)(auto_vest) )
