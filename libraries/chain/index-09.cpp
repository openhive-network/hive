#include <hive/chain/account_object.hpp>
#include <hive/chain/hive_objects.hpp>

#include <hive/chain/index.hpp>

#include <hive/chain/util/type_registrar_definition.hpp>

namespace hive { namespace chain {

void initialize_core_indexes_09( database& db )
{
  HIVE_ADD_CORE_INDEX(db, reward_fund_index);
  HIVE_ADD_CORE_INDEX(db, vesting_delegation_index);
  HIVE_ADD_CORE_INDEX(db, vesting_delegation_expiration_index);
}

} }

HIVE_DEFINE_TYPE_REGISTRAR_REGISTER_TYPE(hive::chain::reward_fund_index)
HIVE_DEFINE_TYPE_REGISTRAR_REGISTER_TYPE(hive::chain::vesting_delegation_index)
HIVE_DEFINE_TYPE_REGISTRAR_REGISTER_TYPE(hive::chain::vesting_delegation_expiration_index)