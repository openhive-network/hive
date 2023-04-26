#include <hive/chain/hive_objects.hpp>

#include <hive/chain/index.hpp>

#include <hive/chain/util/type_registrar_definition.hpp>

namespace hive { namespace chain {

void initialize_core_indexes_08( database& db )
{
  HIVE_ADD_CORE_INDEX(db, escrow_index);
  HIVE_ADD_CORE_INDEX(db, savings_withdraw_index);
  HIVE_ADD_CORE_INDEX(db, decline_voting_rights_request_index);
}

} }

HIVE_DEFINE_TYPE_REGISTRAR_REGISTER_TYPE(hive::chain::escrow_index)
HIVE_DEFINE_TYPE_REGISTRAR_REGISTER_TYPE(hive::chain::savings_withdraw_index)
HIVE_DEFINE_TYPE_REGISTRAR_REGISTER_TYPE(hive::chain::decline_voting_rights_request_index)