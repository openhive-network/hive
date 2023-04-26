#include <hive/chain/hardfork_property_object.hpp>
#include <hive/chain/hive_objects.hpp>

#include <hive/chain/index.hpp>

#include <hive/chain/util/type_registrar_definition.hpp>

namespace hive { namespace chain {

void initialize_core_indexes_06( database& db )
{
  HIVE_ADD_CORE_INDEX(db, liquidity_reward_balance_index);
  HIVE_ADD_CORE_INDEX(db, hardfork_property_index);
  HIVE_ADD_CORE_INDEX(db, withdraw_vesting_route_index);
}

} }

HIVE_DEFINE_TYPE_REGISTRAR_REGISTER_TYPE(hive::chain::liquidity_reward_balance_index)
HIVE_DEFINE_TYPE_REGISTRAR_REGISTER_TYPE(hive::chain::hardfork_property_index)
HIVE_DEFINE_TYPE_REGISTRAR_REGISTER_TYPE(hive::chain::withdraw_vesting_route_index)
