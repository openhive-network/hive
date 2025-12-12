#include <hive/chain/detail/state/feed_history_object_multiindex.hpp>
#include <hive/chain/detail/state/convert_request_object_multiindex.hpp>
#include <hive/chain/detail/state/collateralized_convert_request_object_multiindex.hpp>

#include <hive/chain/index.hpp>
#include <chainbase/chainbase.inl>

#include <hive/chain/util/type_registrar_definition.hpp>
#include <hive/chain/global_property_object.hpp>

#include <hive/protocol/hive_operations.hpp>

namespace hive { namespace chain {

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
    return;

  asset net_hbd( 0, HBD_SYMBOL );
  asset net_hive( 0, HIVE_SYMBOL );

  //regular requests
  int count = 0;
  if( _benchmark_dumper.is_enabled() )
    _benchmark_dumper.begin();
  {
    const auto& request_by_date = get_index< convert_request_index, by_conversion_date >();
    auto itr = request_by_date.begin();

    while( itr != request_by_date.end() && itr->get_conversion_date() <= now )
    {
      auto amount_to_issue = itr->get_convert_amount() * fhistory.current_median_history;
      const auto& owner = get_account( itr->get_owner() );

      adjust_balance( owner, amount_to_issue );

      net_hbd  -= itr->get_convert_amount();
      net_hive += amount_to_issue;

      push_virtual_operation( fill_convert_request_operation( owner.get_name(), itr->get_request_id(),
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
      auto required_hive = multiply_with_fee( itr->get_converted_amount(), fhistory.market_median_history,
        HIVE_COLLATERALIZED_CONVERSION_FEE, HIVE_SYMBOL );
      auto excess_collateral = itr->get_collateral_amount() - required_hive;
      if( excess_collateral.amount < 0 )
      {
        push_virtual_operation( system_warning_operation( FC_LOG_MESSAGE( warn,
          "Insufficient collateral on conversion ${id} by ${o} - shortfall of ${ec}",
          ( "id", itr->get_request_id() )( "o", owner.get_name() )( "ec", -excess_collateral ) ).get_message() ) );

        required_hive = itr->get_collateral_amount();
        excess_collateral.amount = 0;
      }
      else
      {
        adjust_balance( owner, excess_collateral );
      }

      net_hive -= required_hive;
      //note that HBD was created immediately, so we don't need to correct its supply here
      push_virtual_operation( fill_collateralized_convert_request_operation( owner.get_name(), itr->get_request_id(),
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