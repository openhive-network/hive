#include <hive/chain/global_property_object_multiindex.hpp>
#include <hive/chain/account_object_multiindex.hpp>
#include <hive/chain/detail/state/assets_object.hpp>
#include <hive/chain/detail/state/recovery_object.hpp>
#include <hive/chain/detail/state/time_object.hpp>
#include <hive/chain/detail/state/manabars_rc_object.hpp>
#include <hive/chain/detail/state/delayed_votes_object.hpp>
#include <hive/chain/detail/state/tiny_account_object.hpp>

#include <hive/chain/index.hpp>
#include <chainbase/chainbase.inl>

#include <hive/chain/util/type_registrar_definition.hpp>

namespace hive { namespace chain {

// tiny_account_object constructor and modify methods
// Defined here because they need full definitions of account_object, time_object, delayed_votes_object

template< typename Allocator >
tiny_account_object::tiny_account_object( allocator< Allocator > a, uint64_t _id,
  const account_object& acc, const time_object& time_obj, const delayed_votes_object& dvotes )
  : id( _id ), delayed_votes( a )
{
  name = acc.get_name();
  proxy = acc.get_proxy();
  governance_vote_expiration_ts = acc.get_governance_vote_expiration_ts();
  next_vesting_withdrawal = time_obj.get_next_vesting_withdrawal();
  clone_delayed_votes( dvotes.get_delayed_votes() );
}

// Explicit instantiation for the allocator type used by chainbase
template tiny_account_object::tiny_account_object( allocator< tiny_account_object > a, uint64_t _id,
  const account_object& acc, const time_object& time_obj, const delayed_votes_object& dvotes );

void tiny_account_object::modify_from_account( const account_object& acc )
{
  proxy = acc.get_proxy();
  governance_vote_expiration_ts = acc.get_governance_vote_expiration_ts();
}

void tiny_account_object::modify_from_time( const time_object& time_obj )
{
  next_vesting_withdrawal = time_obj.get_next_vesting_withdrawal();
}

void tiny_account_object::modify_from_delayed_votes( const delayed_votes_object& dvotes )
{
  clone_delayed_votes( dvotes.get_delayed_votes() );
}

void initialize_core_indexes_01( database& db )
{
  HIVE_ADD_CORE_INDEX(db, dynamic_global_property_index);
  HIVE_ADD_CORE_INDEX(db, account_index);
  HIVE_ADD_CORE_INDEX(db, assets_index);
  HIVE_ADD_CORE_INDEX(db, recovery_index);
  HIVE_ADD_CORE_INDEX(db, time_index);
  HIVE_ADD_CORE_INDEX(db, manabars_rc_index);
  HIVE_ADD_CORE_INDEX(db, delayed_votes_index);
  HIVE_ADD_CORE_INDEX(db, tiny_account_index);
}

} }

HIVE_DEFINE_TYPE_REGISTRAR_REGISTER_TYPE(hive::chain::dynamic_global_property_index)
HIVE_DEFINE_TYPE_REGISTRAR_REGISTER_TYPE(hive::chain::account_index)
HIVE_DEFINE_TYPE_REGISTRAR_REGISTER_TYPE(hive::chain::assets_index)
HIVE_DEFINE_TYPE_REGISTRAR_REGISTER_TYPE(hive::chain::recovery_index)
HIVE_DEFINE_TYPE_REGISTRAR_REGISTER_TYPE(hive::chain::time_index)
HIVE_DEFINE_TYPE_REGISTRAR_REGISTER_TYPE(hive::chain::manabars_rc_index)
HIVE_DEFINE_TYPE_REGISTRAR_REGISTER_TYPE(hive::chain::delayed_votes_index)
HIVE_DEFINE_TYPE_REGISTRAR_REGISTER_TYPE(hive::chain::tiny_account_index)

// Explicit template instantiations for chainbase::database methods
template const chainbase::generic_index<hive::chain::dynamic_global_property_index>& chainbase::database::get_index<hive::chain::dynamic_global_property_index>() const;
template chainbase::generic_index<hive::chain::dynamic_global_property_index>& chainbase::database::get_mutable_index<hive::chain::dynamic_global_property_index>();

template const chainbase::generic_index<hive::chain::account_index>& chainbase::database::get_index<hive::chain::account_index>() const;
template chainbase::generic_index<hive::chain::account_index>& chainbase::database::get_mutable_index<hive::chain::account_index>();

template const chainbase::generic_index<hive::chain::assets_index>& chainbase::database::get_index<hive::chain::assets_index>() const;
template chainbase::generic_index<hive::chain::assets_index>& chainbase::database::get_mutable_index<hive::chain::assets_index>();

template const chainbase::generic_index<hive::chain::recovery_index>& chainbase::database::get_index<hive::chain::recovery_index>() const;
template chainbase::generic_index<hive::chain::recovery_index>& chainbase::database::get_mutable_index<hive::chain::recovery_index>();

template const chainbase::generic_index<hive::chain::time_index>& chainbase::database::get_index<hive::chain::time_index>() const;
template chainbase::generic_index<hive::chain::time_index>& chainbase::database::get_mutable_index<hive::chain::time_index>();

template const chainbase::generic_index<hive::chain::manabars_rc_index>& chainbase::database::get_index<hive::chain::manabars_rc_index>() const;
template chainbase::generic_index<hive::chain::manabars_rc_index>& chainbase::database::get_mutable_index<hive::chain::manabars_rc_index>();

template const chainbase::generic_index<hive::chain::delayed_votes_index>& chainbase::database::get_index<hive::chain::delayed_votes_index>() const;
template chainbase::generic_index<hive::chain::delayed_votes_index>& chainbase::database::get_mutable_index<hive::chain::delayed_votes_index>();

template const chainbase::generic_index<hive::chain::tiny_account_index>& chainbase::database::get_index<hive::chain::tiny_account_index>() const;
template chainbase::generic_index<hive::chain::tiny_account_index>& chainbase::database::get_mutable_index<hive::chain::tiny_account_index>();
