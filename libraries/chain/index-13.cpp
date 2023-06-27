#include <hive/chain/rc/rc_objects.hpp>

#include <hive/chain/index.hpp>

#include <hive/chain/util/type_registrar_definition.hpp>

namespace hive { namespace chain {

void initialize_core_indexes_13( database& db )
{
  HIVE_ADD_CORE_INDEX( db, rc_expired_delegation_index );
  HIVE_ADD_CORE_INDEX( db, rc_pending_data_index );
  HIVE_ADD_CORE_INDEX( db, rc_direct_delegation_index );
  HIVE_ADD_CORE_INDEX( db, rc_usage_bucket_index );
}

} }

HIVE_DEFINE_TYPE_REGISTRAR_REGISTER_TYPE( hive::chain::rc_expired_delegation_index )
HIVE_DEFINE_TYPE_REGISTRAR_REGISTER_TYPE( hive::chain::rc_pending_data_index )
HIVE_DEFINE_TYPE_REGISTRAR_REGISTER_TYPE( hive::chain::rc_direct_delegation_index )
HIVE_DEFINE_TYPE_REGISTRAR_REGISTER_TYPE( hive::chain::rc_usage_bucket_index )