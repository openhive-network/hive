#include <hive/chain/detail/state/feed_history_object_multiindex.hpp>
#include <hive/chain/detail/state/convert_request_object_multiindex.hpp>
#include <hive/chain/detail/state/collateralized_convert_request_object_multiindex.hpp>

#include <hive/chain/index.hpp>
#include <chainbase/chainbase.inl>

#include <hive/chain/util/type_registrar_definition.hpp>

namespace hive { namespace chain {

void initialize_core_indexes_05( database& db )
{
  HIVE_ADD_CORE_INDEX(db, feed_history_index);
  HIVE_ADD_CORE_INDEX(db, convert_request_index);
  HIVE_ADD_CORE_INDEX(db, collateralized_convert_request_index);
}

} }

HIVE_DEFINE_TYPE_REGISTRAR_REGISTER_TYPE(hive::chain::feed_history_index)
HIVE_DEFINE_TYPE_REGISTRAR_REGISTER_TYPE(hive::chain::convert_request_index)
HIVE_DEFINE_TYPE_REGISTRAR_REGISTER_TYPE(hive::chain::collateralized_convert_request_index)

// Explicit template instantiations for chainbase::database methods
template const chainbase::generic_index<hive::chain::feed_history_index>& chainbase::database::get_index<hive::chain::feed_history_index>() const;
template chainbase::generic_index<hive::chain::feed_history_index>& chainbase::database::get_mutable_index<hive::chain::feed_history_index>();

template const chainbase::generic_index<hive::chain::convert_request_index>& chainbase::database::get_index<hive::chain::convert_request_index>() const;
template chainbase::generic_index<hive::chain::convert_request_index>& chainbase::database::get_mutable_index<hive::chain::convert_request_index>();

template const chainbase::generic_index<hive::chain::collateralized_convert_request_index>& chainbase::database::get_index<hive::chain::collateralized_convert_request_index>() const;
template chainbase::generic_index<hive::chain::collateralized_convert_request_index>& chainbase::database::get_mutable_index<hive::chain::collateralized_convert_request_index>();