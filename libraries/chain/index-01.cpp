#include <hive/chain/global_property_object.hpp>
#include <hive/chain/account_object.hpp>

#include <hive/chain/index.hpp>

#include <hive/chain/util/type_registrar_definition.hpp>

namespace hive { namespace chain {

void initialize_core_indexes_01( database& db )
{
  HIVE_ADD_CORE_INDEX(db, dynamic_global_property_index);
  HIVE_ADD_CORE_INDEX(db, account_index);
  HIVE_ADD_CORE_INDEX(db, account_metadata_index);
}

} }

HIVE_DEFINE_TYPE_REGISTRAR_REGISTER_TYPE(hive::chain::dynamic_global_property_index)
HIVE_DEFINE_TYPE_REGISTRAR_REGISTER_TYPE(hive::chain::account_index)
HIVE_DEFINE_TYPE_REGISTRAR_REGISTER_TYPE(hive::chain::account_metadata_index)
