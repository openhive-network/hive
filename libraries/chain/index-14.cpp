#include <hive/chain/rc/rc_objects.hpp>

#include <hive/chain/external_storage/comment_rocksdb_objects.hpp>

#include <hive/chain/index.hpp>

#include <hive/chain/util/type_registrar_definition.hpp>

namespace hive { namespace chain {

void initialize_core_indexes_14( database& db )
{
  HIVE_ADD_CORE_INDEX( db, hive::chain::volatile_comment_index );
}

} }


HIVE_DEFINE_TYPE_REGISTRAR_REGISTER_TYPE(hive::chain::volatile_comment_index)
