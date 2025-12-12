#include <hive/chain/rc/rc_objects.hpp>

#include <hive/chain/index.hpp>
#include <chainbase/chainbase.inl>

#include <hive/chain/util/type_registrar_definition.hpp>

namespace hive { namespace chain {

void initialize_core_indexes_12( database& db )
{
  HIVE_ADD_CORE_INDEX( db, rc_resource_param_index );
  HIVE_ADD_CORE_INDEX( db, rc_pool_index );
  HIVE_ADD_CORE_INDEX( db, rc_stats_index );
}

} }

HIVE_DEFINE_TYPE_REGISTRAR_REGISTER_TYPE( hive::chain::rc_resource_param_index )
HIVE_DEFINE_TYPE_REGISTRAR_REGISTER_TYPE( hive::chain::rc_pool_index )
HIVE_DEFINE_TYPE_REGISTRAR_REGISTER_TYPE( hive::chain::rc_stats_index )

// Explicit template instantiations for chainbase::database methods
template const chainbase::generic_index<hive::chain::rc_resource_param_index>& chainbase::database::get_index<hive::chain::rc_resource_param_index>() const;
template chainbase::generic_index<hive::chain::rc_resource_param_index>& chainbase::database::get_mutable_index<hive::chain::rc_resource_param_index>();

template const chainbase::generic_index<hive::chain::rc_pool_index>& chainbase::database::get_index<hive::chain::rc_pool_index>() const;
template chainbase::generic_index<hive::chain::rc_pool_index>& chainbase::database::get_mutable_index<hive::chain::rc_pool_index>();

template const chainbase::generic_index<hive::chain::rc_stats_index>& chainbase::database::get_index<hive::chain::rc_stats_index>() const;
template chainbase::generic_index<hive::chain::rc_stats_index>& chainbase::database::get_mutable_index<hive::chain::rc_stats_index>();