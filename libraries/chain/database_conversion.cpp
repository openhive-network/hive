#include <hive/chain/detail/state/feed_history_object_multiindex.hpp>
#include <hive/chain/detail/state/convert_request_object_multiindex.hpp>
#include <hive/chain/detail/state/collateralized_convert_request_object_multiindex.hpp>

#include <hive/chain/database_virtual_operations.hpp>
#include <hive/chain/index.hpp>
#include <chainbase/chainbase.inl>

#include <hive/chain/util/type_registrar_definition.hpp>
#include <hive/chain/global_property_object_multiindex.hpp>

#include <hive/protocol/hive_operations.hpp>
#include <hive/protocol/hive_virtual_operations.hpp>

namespace hive { namespace chain {

using hive::protocol::fill_collateralized_convert_request_operation;
using hive::protocol::fill_convert_request_operation;
using hive::protocol::system_warning_operation;

void initialize_core_indexes_05( database& db )
{
  HIVE_ADD_CORE_INDEX(db, feed_history_index);
  HIVE_ADD_CORE_INDEX(db, convert_request_index);
  HIVE_ADD_CORE_INDEX(db, collateralized_convert_request_index);
}

/**
  *  Iterates over all [collateralized] conversion requests with a conversion date before
  *  the head block time and then converts them to/from HIVE/HBD at the
  *  current median price feed history price times the premium/fee
  *  Collateralized requests might also return excess collateral.
  */
void database::process_conversions()
{
  auto now = head_block_time();
  const auto& fhistory = get_feed_history();
  if( fhistory.current_median_history.is_null() )
    return; //current_median_history was null until block 933600

  HBD_asset net_hbd( 0 );
  HIVE_asset net_hive( 0 );

  //regular requests
  int count = 0;
  if( _benchmark_dumper.is_enabled() )
    _benchmark_dumper.begin();
  {
    const auto& request_by_date = get_index< convert_request_index, by_conversion_date >();
    auto itr = request_by_date.begin();

    while( itr != request_by_date.end() && itr->get_conversion_date() <= now )
    {
      HIVE_asset amount_to_issue = itr->get_convert_amount() * fhistory.current_median_history;
      const auto& owner = get_account( itr->get_owner() );

      adjust_balance( owner, amount_to_issue );

      net_hbd  -= itr->get_convert_amount();
      net_hive += amount_to_issue;

      push_virtual_operation( *this, fill_convert_request_operation( owner.get_name(), itr->get_request_id(),
        itr->get_convert_amount(), amount_to_issue ) );

      remove( *itr );
      itr = request_by_date.begin();

      ++count;
    }
  }
  if( _benchmark_dumper.is_enabled() && count )
    _benchmark_dumper.end( "processing", "hive::protocol::convert_operation", count );

  //collateralized requests
  count = 0;
  if( _benchmark_dumper.is_enabled() )
    _benchmark_dumper.begin();
  {
    const auto& request_by_date = get_index< collateralized_convert_request_index, by_conversion_date >();
    auto itr = request_by_date.begin();

    while( itr != request_by_date.end() && itr->get_conversion_date() <= now )
    {
      const auto& owner = get_account( itr->get_owner() );

      //calculate how much HIVE we'd need for already created HBD at current corrected price
      //note that we are using market median price instead of minimal as it was during immediate part of this
      //conversion or current median price as is used in other conversions; the former is by design - we are supposed
      //to use median price here, minimal during immediate conversion was to prevent gaming; the latter is for rare
      //case, when this conversion happens when median price does not reflect market conditions when hard limit was
      //hit - see update_median_feed()
      HIVE_asset required_hive = multiply_with_fee( itr->get_converted_amount(), fhistory.market_median_history.to_price(),
        HIVE_COLLATERALIZED_CONVERSION_FEE, HIVE_SYMBOL );
      HIVE_asset excess_collateral = itr->get_collateral_amount() - required_hive;
      if( excess_collateral.amount < 0 )
      {
        push_virtual_operation( *this, system_warning_operation( FC_LOG_MESSAGE( warn,
          "Insufficient collateral on conversion ${id} by ${o} - shortfall of ${ec}",
          ( "id", itr->get_request_id() )( "o", owner.get_name() )( "ec", -excess_collateral ) ).get_message() ) );

        required_hive = itr->get_collateral_amount();
        excess_collateral = HIVE_asset( 0 );
      }
      else
      {
        adjust_balance( owner, excess_collateral );
      }

      net_hive -= required_hive;
      //note that HBD was created immediately, so we don't need to correct its supply here
      push_virtual_operation( *this, fill_collateralized_convert_request_operation( owner.get_name(), itr->get_request_id(),
        required_hive, itr->get_converted_amount(), excess_collateral ) );

      remove( *itr );
      itr = request_by_date.begin();

      ++count;
    }
    if( _benchmark_dumper.is_enabled() && count )
      _benchmark_dumper.end( "processing", "hive::protocol::collateralized_convert_operation", count );
  }

  //correct global supply (if needed)
  if( net_hive.amount.value | net_hbd.amount.value )
  {
    modify( get_dynamic_global_properties(), [&]( dynamic_global_property_object& p )
    {
      p.current_supply += net_hive;
      p.current_hbd_supply += net_hbd;
      p.virtual_supply += net_hive;
      p.virtual_supply += net_hbd * fhistory.current_median_history;
    } );
  }
}

void database::remove_pending_conversion_requests( const account_object& account )
{
  // Remove pending convert requests (return balance to account)
  const auto& request_idx = get_index< convert_request_index, by_owner >();
  auto request_itr = request_idx.lower_bound( account.get_id() );
  while( request_itr != request_idx.end() && request_itr->get_owner() == account.get_id() )
  {
    auto& request = *request_itr;
    ++request_itr;

    adjust_balance( account, request.get_convert_amount() );
    remove( request );
  }

  // Make sure there are no pending collateralized convert requests
  // (if we decided to handle them anyway, in case we wanted to reuse this routine outside HF23 code,
  // we should most likely destroy collateral balance instead of putting it into treasury)
  const auto& collateralized_request_idx = get_index< collateralized_convert_request_index, by_owner >();
  auto collateralized_request_itr = collateralized_request_idx.lower_bound( account.get_id() );
  FC_ASSERT( collateralized_request_itr == collateralized_request_idx.end() || collateralized_request_itr->get_owner() != account.get_id(),
    "Collateralized convert requests not handled by clear_account" );
}

void database::get_convert_request_totals( HIVE_asset& total_collateral, HBD_asset& total_hbd, uint64_t& collateralized_count, uint64_t& convert_count ) const
{
  const auto& convert_request_idx = get_index< convert_request_index >().indices();
  for( auto itr = convert_request_idx.begin(); itr != convert_request_idx.end(); ++itr )
  {
    total_hbd += itr->get_convert_amount();
    ++convert_count;
  }

  const auto& collateralized_convert_request_idx = get_index< collateralized_convert_request_index >().indices();
  for( auto itr = collateralized_convert_request_idx.begin(); itr != collateralized_convert_request_idx.end(); ++itr )
  {
    total_collateral += itr->get_collateral_amount();
    // don't collect get_converted_amount() - it is not balance object; that tokens are already on owner's balance
    ++collateralized_count;
  }
}

} }

HIVE_DEFINE_TYPE_REGISTRAR_REGISTER_TYPE(hive::chain::feed_history_index)
HIVE_DEFINE_TYPE_REGISTRAR_REGISTER_TYPE(hive::chain::convert_request_index)
HIVE_DEFINE_TYPE_REGISTRAR_REGISTER_TYPE(hive::chain::collateralized_convert_request_index)

// Explicit template instantiations for chainbase::database methods
template const chainbase::generic_index<hive::chain::feed_history_index>& chainbase::database::get_index<hive::chain::feed_history_index>() const;
template chainbase::generic_index<hive::chain::feed_history_index>& chainbase::database::get_mutable_index<hive::chain::feed_history_index>();

template const chainbase::generic_index<hive::chain::convert_request_index>& chainbase::database::get_index<hive::chain::convert_request_index>() const;
template chainbase::generic_index<hive::chain::convert_request_index>& chainbase::database::get_mutable_index<hive::chain::convert_request_index>();

template const chainbase::generic_index<hive::chain::collateralized_convert_request_index>& chainbase::database::get_index<hive::chain::collateralized_convert_request_index>() const;
template chainbase::generic_index<hive::chain::collateralized_convert_request_index>& chainbase::database::get_mutable_index<hive::chain::collateralized_convert_request_index>();