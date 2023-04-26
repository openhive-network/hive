#include <hive/chain/account_object.hpp>

#include <hive/chain/index.hpp>

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
