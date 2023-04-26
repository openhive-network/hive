#include <hive/chain/block_summary_object.hpp>
#include <hive/chain/comment_object.hpp>
#include <hive/chain/witness_objects.hpp>

#include <hive/chain/index.hpp>

#include <hive/chain/util/type_registrar_definition.hpp>

namespace hive { namespace chain {

void initialize_core_indexes_03( database& db )
{
  HIVE_ADD_CORE_INDEX(db, block_summary_index);
  HIVE_ADD_CORE_INDEX(db, witness_schedule_index);
  HIVE_ADD_CORE_INDEX(db, comment_index);
}

} }

HIVE_DEFINE_TYPE_REGISTRAR_REGISTER_TYPE(hive::chain::block_summary_index)
HIVE_DEFINE_TYPE_REGISTRAR_REGISTER_TYPE(hive::chain::witness_schedule_index)
HIVE_DEFINE_TYPE_REGISTRAR_REGISTER_TYPE(hive::chain::comment_index)