#include <hive/chain/account_object.hpp>
#include <hive/chain/witness_objects.hpp>
#include <hive/chain/transaction_object.hpp>

#include <hive/chain/index.hpp>

#include <hive/chain/util/type_registrar_definition.hpp>

namespace hive { namespace chain {

void initialize_core_indexes_02( database& db )
{
  HIVE_ADD_CORE_INDEX(db, account_authority_index);
  HIVE_ADD_CORE_INDEX(db, witness_index);
  HIVE_ADD_CORE_INDEX(db, transaction_index);
}

} }

HIVE_DEFINE_TYPE_REGISTRAR_REGISTER_TYPE(hive::chain::account_authority_index)
HIVE_DEFINE_TYPE_REGISTRAR_REGISTER_TYPE(hive::chain::witness_index)
HIVE_DEFINE_TYPE_REGISTRAR_REGISTER_TYPE(hive::chain::transaction_index)
