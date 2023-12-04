#include <hive/chain/full_block_object.hpp>

#include <hive/chain/index.hpp>

#include <hive/chain/util/type_registrar_definition.hpp>

namespace hive { namespace chain {

void initialize_pruning_indexes( database& db )
{
  HIVE_ADD_NON_REVERTABLE_CORE_INDEX( db, full_block_index );
}

} }

HIVE_DEFINE_TYPE_REGISTRAR_REGISTER_TYPE( hive::chain::full_block_index )
