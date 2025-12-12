#include <hive/chain/witness_objects_multiindex.hpp>

#include <hive/chain/index.hpp>
#include <chainbase/chainbase.inl>

#include <hive/chain/util/type_registrar_definition.hpp>

namespace hive { namespace chain {

void initialize_core_indexes_04( database& db )
{
  HIVE_ADD_CORE_INDEX(db, witness_index);
  HIVE_ADD_CORE_INDEX(db, witness_vote_index);
  HIVE_ADD_CORE_INDEX(db, witness_schedule_index);

}

const witness_object* database::find_witness( const account_name_type& name ) const
{
  return find< witness_object, by_name >( name );
}


const witness_schedule_object& database::get_witness_schedule_object()const
{ try {
  return get< witness_schedule_object >();
} FC_CAPTURE_AND_RETHROW() }

const witness_schedule_object& database::get_future_witness_schedule_object() const
{
  try
  {
    return get<witness_schedule_object>(witness_schedule_object::id_type(1));
  }
  catch (const std::out_of_range&)
  {
    FC_THROW_EXCEPTION(fc::key_not_found_exception, "Future witness schedule does not exist");
  }
}


} }

HIVE_DEFINE_TYPE_REGISTRAR_REGISTER_TYPE(hive::chain::witness_schedule_index)
HIVE_DEFINE_TYPE_REGISTRAR_REGISTER_TYPE(hive::chain::witness_vote_index)
HIVE_DEFINE_TYPE_REGISTRAR_REGISTER_TYPE(hive::chain::witness_index)

// Explicit template instantiations for chainbase::database methods
template const chainbase::generic_index<hive::chain::witness_index>& chainbase::database::get_index<hive::chain::witness_index>() const;
template chainbase::generic_index<hive::chain::witness_index>& chainbase::database::get_mutable_index<hive::chain::witness_index>();

template const chainbase::generic_index<hive::chain::witness_vote_index>& chainbase::database::get_index<hive::chain::witness_vote_index>() const;
template chainbase::generic_index<hive::chain::witness_vote_index>& chainbase::database::get_mutable_index<hive::chain::witness_vote_index>();

template const chainbase::generic_index<hive::chain::witness_schedule_index>& chainbase::database::get_index<hive::chain::witness_schedule_index>() const;
template chainbase::generic_index<hive::chain::witness_schedule_index>& chainbase::database::get_mutable_index<hive::chain::witness_schedule_index>();