#include <hive/chain/detail/state/escrow_object_multiindex.hpp>
#include <hive/chain/detail/state/savings_withdraw_object_multiindex.hpp>
#include <hive/chain/detail/state/decline_voting_rights_request_object_multiindex.hpp>
#include <hive/chain/detail/state/limit_order_object_multiindex.hpp>

#include <hive/chain/account_object_multiindex.hpp>
#include <hive/chain/index.hpp>
#include <chainbase/chainbase.inl>

#include <hive/chain/util/type_registrar_definition.hpp>

#include <hive/protocol/hive_operations.hpp>

namespace hive { namespace chain {

void initialize_core_indexes_08( database& db )
{
  HIVE_ADD_CORE_INDEX(db, escrow_index);
  HIVE_ADD_CORE_INDEX(db, savings_withdraw_index);
  HIVE_ADD_CORE_INDEX(db, decline_voting_rights_request_index);
  HIVE_ADD_CORE_INDEX(db, limit_order_index);

}

const escrow_object& database::get_escrow( const account_name_type& name, uint32_t escrow_id )const
{ try {
  const auto* _escrow = find_escrow( name, escrow_id );
  FC_ASSERT( _escrow != nullptr, "Escrow balance with 'name' ${name} 'escrow_id' ${escrow_id} doesn't exist.", (name)(escrow_id) );
  return *_escrow;
} FC_CAPTURE_AND_RETHROW( (name)(escrow_id) ) }

const escrow_object* database::find_escrow( const account_name_type& name, uint32_t escrow_id )const
{
  return find< escrow_object, by_from_id >( boost::make_tuple( name, escrow_id ) );
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

void database::expire_escrow_ratification()
{
  const auto& escrow_idx = get_index< escrow_index >().indices().get< by_ratification_deadline >();
  auto escrow_itr = escrow_idx.lower_bound( false );

  while( escrow_itr != escrow_idx.end() && !escrow_itr->is_approved() && escrow_itr->ratification_deadline <= head_block_time() )
  {
    const auto& old_escrow = *escrow_itr;
    ++escrow_itr;

    adjust_balance( old_escrow.from, old_escrow.get_hive_balance() );
    adjust_balance( old_escrow.from, old_escrow.get_hbd_balance() );
    adjust_balance( old_escrow.from, old_escrow.get_fee() );

    push_virtual_operation( escrow_rejected_operation( old_escrow.from, old_escrow.to, old_escrow.agent, old_escrow.escrow_id,
      old_escrow.get_hbd_balance(), old_escrow.get_hive_balance(), old_escrow.get_fee() ) );

    modify( get_account( old_escrow.from ), []( account_object& a )
    {
      a.pending_escrow_transfers--;
    } );

    remove( old_escrow );
  }
}

void database::remove_pending_escrows( const account_object& account, const account_name_type& account_name )
{
  // Remove pending escrows (return balance to account - compare with expire_escrow_ratification())
  const auto& escrow_idx = get_index< escrow_index, by_from_id >();
  auto escrow_itr = escrow_idx.lower_bound( account_name );
  while( escrow_itr != escrow_idx.end() && escrow_itr->from == account_name )
  {
    auto& escrow = *escrow_itr;
    ++escrow_itr;

    adjust_balance( account, escrow.get_hive_balance() );
    adjust_balance( account, escrow.get_hbd_balance() );
    adjust_balance( account, escrow.get_fee() );

    modify( account, []( account_object& a )
    {
      a.pending_escrow_transfers--;
    } );

    remove( escrow );
  }
}

void database::get_escrow_totals( asset& total_hive, asset& total_hbd, uint64_t& escrow_count ) const
{
  const auto& escrow_idx = get_index< escrow_index >().indices().get< by_id >();

  for( auto itr = escrow_idx.begin(); itr != escrow_idx.end(); ++itr )
  {
    total_hive += itr->get_hive_balance();
    total_hbd += itr->get_hbd_balance();

    if( itr->get_fee().symbol == HIVE_SYMBOL )
      total_hive += itr->get_fee();
    else
    {
      FC_ASSERT( itr->get_fee().symbol == HBD_SYMBOL, "found escrow pending fee that is not HBD or HIVE" );
      total_hbd += itr->get_fee();
    }
    ++escrow_count;
  }
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
