#include <hive/chain/rc/rc_objects.hpp>

#include <hive/chain/index.hpp>

#include <hive/chain/util/type_registrar_definition.hpp>

namespace hive { namespace chain {

void initialize_core_indexes_12( database& db )
{
  //TODO: uncomment later when moving RC activation to core
  //HIVE_ADD_CORE_INDEX( hive::chain::rc_expired_delegation_index )
  //HIVE_ADD_CORE_INDEX( hive::chain::rc_resource_param_index )
  //HIVE_ADD_CORE_INDEX( hive::chain::rc_pool_index )
  //HIVE_ADD_CORE_INDEX( hive::chain::rc_stats_index )
}

} }

HIVE_DEFINE_TYPE_REGISTRAR_REGISTER_TYPE( hive::chain::rc_expired_delegation_index )
HIVE_DEFINE_TYPE_REGISTRAR_REGISTER_TYPE( hive::chain::rc_resource_param_index )
HIVE_DEFINE_TYPE_REGISTRAR_REGISTER_TYPE( hive::chain::rc_pool_index )
HIVE_DEFINE_TYPE_REGISTRAR_REGISTER_TYPE( hive::chain::rc_stats_index )