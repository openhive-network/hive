#include <hive/chain/detail/state/state_stamp_data_object_multiindex.hpp>

#include <hive/chain/index.hpp>
#include <chainbase/chainbase.inl>

#include <hive/chain/util/type_registrar_definition.hpp>

namespace hive { namespace chain {

void initialize_core_indexes_10( database& db )
{
  HIVE_ADD_CORE_INDEX( db, state_stamp_data_index );
}

} }

HIVE_DEFINE_TYPE_REGISTRAR_REGISTER_TYPE(hive::chain::state_stamp_data_index)

// Explicit template instantiations for chainbase::database methods
template const chainbase::generic_index<hive::chain::state_stamp_data_index>& chainbase::database::get_index<hive::chain::state_stamp_data_index>() const;
template chainbase::generic_index<hive::chain::state_stamp_data_index>& chainbase::database::get_mutable_index<hive::chain::state_stamp_data_index>();
