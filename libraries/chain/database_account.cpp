#include <hive/chain/global_property_object_multiindex.hpp>
#include <hive/chain/account_object_multiindex.hpp>
#include <hive/chain/detail/state/account_details_object.hpp>
#include <hive/chain/detail/state/tiny_account_object.hpp>

#include <hive/chain/index.hpp>
#include <chainbase/chainbase.inl>

#include <hive/chain/util/type_registrar_definition.hpp>
#include <hive/protocol/config.hpp>

namespace hive { namespace chain {

// tiny_account_object constructor and methods
// Defined here because they need full definitions of account_object, account_details_object
// and protocol constants (hardfork definitions)

template< typename Allocator >
tiny_account_object::tiny_account_object( allocator< Allocator > a, uint64_t _id,
  const account_object& acc, const account_details_object& details_obj )
  : id( _id )
{
  name = acc.get_name();
  // proxy and governance_vote_expiration_ts start at defaults (no proxy, maximum expiration)
  oldest_delayed_vote_time = details_obj.get_oldest_delayed_vote_time();
}

// Explicit instantiation for the allocator type used by chainbase
template tiny_account_object::tiny_account_object( allocator< tiny_account_object > a, uint64_t _id,
  const account_object& acc, const account_details_object& details_obj );

void tiny_account_object::update_governance_vote_expiration_ts( const time_point_sec vote_time )
{
  governance_vote_expiration_ts = vote_time + HIVE_GOVERNANCE_VOTE_EXPIRATION_PERIOD;
  if( governance_vote_expiration_ts < HARDFORK_1_25_FIRST_GOVERNANCE_VOTE_EXPIRE_TIMESTAMP )
  {
    const int64_t DIVIDER = HIVE_HARDFORK_1_25_MAX_OLD_GOVERNANCE_VOTE_EXPIRE_SHIFT.to_seconds();
    governance_vote_expiration_ts = HARDFORK_1_25_FIRST_GOVERNANCE_VOTE_EXPIRE_TIMESTAMP + fc::seconds(governance_vote_expiration_ts.sec_since_epoch() % DIVIDER);
  }
}

void tiny_account_object::modify_from_delayed_votes( const account_details_object& account_details )
{
  oldest_delayed_vote_time = account_details.get_oldest_delayed_vote_time();
}

void initialize_core_indexes_01( database& db )
{
  HIVE_ADD_CORE_INDEX(db, dynamic_global_property_index);
  HIVE_ADD_CORE_INDEX(db, account_index);
  HIVE_ADD_CORE_INDEX(db, account_details_index);
  HIVE_ADD_CORE_INDEX(db, tiny_account_index);
}

} }

HIVE_DEFINE_TYPE_REGISTRAR_REGISTER_TYPE(hive::chain::dynamic_global_property_index)
HIVE_DEFINE_TYPE_REGISTRAR_REGISTER_TYPE(hive::chain::account_index)
HIVE_DEFINE_TYPE_REGISTRAR_REGISTER_TYPE(hive::chain::account_details_index)
HIVE_DEFINE_TYPE_REGISTRAR_REGISTER_TYPE(hive::chain::tiny_account_index)

// Explicit template instantiations for chainbase::database methods
template const chainbase::generic_index<hive::chain::dynamic_global_property_index>& chainbase::database::get_index<hive::chain::dynamic_global_property_index>() const;
template chainbase::generic_index<hive::chain::dynamic_global_property_index>& chainbase::database::get_mutable_index<hive::chain::dynamic_global_property_index>();

template const chainbase::generic_index<hive::chain::account_index>& chainbase::database::get_index<hive::chain::account_index>() const;
template chainbase::generic_index<hive::chain::account_index>& chainbase::database::get_mutable_index<hive::chain::account_index>();

template const chainbase::generic_index<hive::chain::account_details_index>& chainbase::database::get_index<hive::chain::account_details_index>() const;
template chainbase::generic_index<hive::chain::account_details_index>& chainbase::database::get_mutable_index<hive::chain::account_details_index>();

template const chainbase::generic_index<hive::chain::tiny_account_index>& chainbase::database::get_index<hive::chain::tiny_account_index>() const;
template chainbase::generic_index<hive::chain::tiny_account_index>& chainbase::database::get_mutable_index<hive::chain::tiny_account_index>();
