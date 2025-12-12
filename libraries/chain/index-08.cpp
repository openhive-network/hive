#include <hive/chain/detail/state/escrow_object_multiindex.hpp>
#include <hive/chain/detail/state/savings_withdraw_object_multiindex.hpp>
#include <hive/chain/detail/state/decline_voting_rights_request_object_multiindex.hpp>
#include <hive/chain/detail/state/limit_order_object_multiindex.hpp>

#include <hive/chain/index.hpp>
#include <chainbase/chainbase.inl>

#include <hive/chain/util/type_registrar_definition.hpp>

namespace hive { namespace chain {

void initialize_core_indexes_08( database& db )
{
  HIVE_ADD_CORE_INDEX(db, escrow_index);
  HIVE_ADD_CORE_INDEX(db, savings_withdraw_index);
  HIVE_ADD_CORE_INDEX(db, decline_voting_rights_request_index);
  HIVE_ADD_CORE_INDEX(db, limit_order_index);

}

const savings_withdraw_object* database::find_savings_withdraw( const account_name_type& owner, uint32_t request_id )const
{
  return find< savings_withdraw_object, by_from_rid >( boost::make_tuple( owner, request_id ) );
}

const limit_order_object* database::find_limit_order( const account_name_type& name, uint32_t orderid )const
{
  if( !has_hardfork( HIVE_HARDFORK_0_6__127 ) )
    orderid = orderid & 0x0000FFFF;

  return find< limit_order_object, by_account >( boost::make_tuple( name, orderid ) );
}


} }

HIVE_DEFINE_TYPE_REGISTRAR_REGISTER_TYPE(hive::chain::escrow_index)
HIVE_DEFINE_TYPE_REGISTRAR_REGISTER_TYPE(hive::chain::savings_withdraw_index)
HIVE_DEFINE_TYPE_REGISTRAR_REGISTER_TYPE(hive::chain::decline_voting_rights_request_index)
HIVE_DEFINE_TYPE_REGISTRAR_REGISTER_TYPE(hive::chain::limit_order_index)

// Explicit template instantiations for chainbase::database methods
template const chainbase::generic_index<hive::chain::escrow_index>& chainbase::database::get_index<hive::chain::escrow_index>() const;
template chainbase::generic_index<hive::chain::escrow_index>& chainbase::database::get_mutable_index<hive::chain::escrow_index>();

template const chainbase::generic_index<hive::chain::savings_withdraw_index>& chainbase::database::get_index<hive::chain::savings_withdraw_index>() const;
template chainbase::generic_index<hive::chain::savings_withdraw_index>& chainbase::database::get_mutable_index<hive::chain::savings_withdraw_index>();

template const chainbase::generic_index<hive::chain::decline_voting_rights_request_index>& chainbase::database::get_index<hive::chain::decline_voting_rights_request_index>() const;
template chainbase::generic_index<hive::chain::decline_voting_rights_request_index>& chainbase::database::get_mutable_index<hive::chain::decline_voting_rights_request_index>();

template const chainbase::generic_index<hive::chain::limit_order_index>& chainbase::database::get_index<hive::chain::limit_order_index>() const;
template chainbase::generic_index<hive::chain::limit_order_index>& chainbase::database::get_mutable_index<hive::chain::limit_order_index>();
