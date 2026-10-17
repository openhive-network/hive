#include <hive/chain/detail/state/scoped_authority_object_multiindex.hpp>
#include <hive/chain/index.hpp>
#include <chainbase/chainbase.inl>
#include <hive/chain/util/type_registrar_definition.hpp>

namespace hive { namespace chain {

void initialize_core_indexes_14( database& db )
{
  HIVE_ADD_CORE_INDEX( db, scoped_authority_index );
}

} }

HIVE_DEFINE_TYPE_REGISTRAR_REGISTER_TYPE( hive::chain::scoped_authority_index )

template const chainbase::generic_index<hive::chain::scoped_authority_index>&
  chainbase::database::get_index<hive::chain::scoped_authority_index>() const;
template chainbase::generic_index<hive::chain::scoped_authority_index>&
  chainbase::database::get_mutable_index<hive::chain::scoped_authority_index>();
