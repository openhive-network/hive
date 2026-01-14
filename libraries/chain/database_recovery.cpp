#include <hive/chain/account_object_multiindex.hpp>

#include <hive/chain/index.hpp>
#include <chainbase/chainbase.inl>

#include <hive/chain/util/type_registrar_definition.hpp>

namespace hive { namespace chain {

void initialize_core_indexes_07( database& db )
{
  HIVE_ADD_CORE_INDEX(db, owner_authority_history_index);
  HIVE_ADD_CORE_INDEX(db, account_recovery_request_index);
  HIVE_ADD_CORE_INDEX(db, change_recovery_account_request_index);
}

} }

HIVE_DEFINE_TYPE_REGISTRAR_REGISTER_TYPE(hive::chain::owner_authority_history_index)
HIVE_DEFINE_TYPE_REGISTRAR_REGISTER_TYPE(hive::chain::account_recovery_request_index)
HIVE_DEFINE_TYPE_REGISTRAR_REGISTER_TYPE(hive::chain::change_recovery_account_request_index)

// Explicit template instantiations for chainbase::database methods
template const chainbase::generic_index<hive::chain::owner_authority_history_index>& chainbase::database::get_index<hive::chain::owner_authority_history_index>() const;
template chainbase::generic_index<hive::chain::owner_authority_history_index>& chainbase::database::get_mutable_index<hive::chain::owner_authority_history_index>();

template const chainbase::generic_index<hive::chain::account_recovery_request_index>& chainbase::database::get_index<hive::chain::account_recovery_request_index>() const;
template chainbase::generic_index<hive::chain::account_recovery_request_index>& chainbase::database::get_mutable_index<hive::chain::account_recovery_request_index>();

template const chainbase::generic_index<hive::chain::change_recovery_account_request_index>& chainbase::database::get_index<hive::chain::change_recovery_account_request_index>() const;
template chainbase::generic_index<hive::chain::change_recovery_account_request_index>& chainbase::database::get_mutable_index<hive::chain::change_recovery_account_request_index>();
