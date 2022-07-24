
#include <hive/chain/hive_object_types.hpp>

#include <hive/chain/index.hpp>

#include <hive/chain/block_summary_object.hpp>
#include <hive/chain/pending_required_action_object.hpp>
#include <hive/chain/pending_optional_action_object.hpp>
#include <hive/chain/smt_objects.hpp>
#include <hive/chain/hive_objects.hpp>
#include <hive/chain/transaction_object.hpp>
#include <hive/chain/witness_schedule.hpp>

namespace hive { namespace chain {

void initialize_core_indexes_01( database& db )
{
  HIVE_ADD_CORE_INDEX(db, dynamic_global_property_index);
  HIVE_ADD_CORE_INDEX(db, account_index);
  HIVE_ADD_CORE_INDEX(db, account_metadata_index);
}

} }
