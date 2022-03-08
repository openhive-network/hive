#pragma once

#include <fc/variant.hpp>

#include <hive/protocol/asset.hpp>
#include <hive/plugins/wallet_bridge_api/wallet_bridge_api_args.hpp>

#include<vector>
#include<string>
#include<functional>
#include <iomanip>

#define ASSET_TO_REAL( asset ) (double)( asset.amount.value )

namespace hive { namespace plugins { namespace wallet_bridge_api {

using std::function;
using fc::variant;
using std::string;
using hive::protocol::asset;
using std::setiosflags;
using std::ios;
using std::setw;
using std::endl;

struct wallet_formatter
{
  using function_type = std::function<variant(const variant&)>;

  template<typename T>
  static variant get_result( const std::stringstream& out_text, const T& out_json, format_type format )
  {
    if( format == format_type::textformat )
    {
      return variant( out_text.str() );
    }
    else//format_type::json
    {
      variant _result;
      to_variant( out_json, _result );
      return _result;
    }
  }

  static variant help( const variant& result )
  {
    return result;
  }

  static variant gethelp( const variant& result )
  {
    return result;
  }

  static variant list_my_accounts( const variant& result )
  {
    return list_my_accounts_impl( result.as<serializer_wrapper<vector<database_api::api_account_object>>>(), format_type::textformat );
  }

  static variant list_my_accounts_impl( const serializer_wrapper<vector<database_api::api_account_object>>& accounts, format_type format )
  {
    if( format == format_type::noformat )
    {
      variant _result;
      to_variant( accounts, _result );
      return _result;
    }

    std::stringstream             out_text;
    list_my_accounts_json_return  out_json;

    asset total_hive;
    asset total_vest(0, VESTS_SYMBOL );
    asset total_hbd(0, HBD_SYMBOL );
    for( const auto& account_in_wrapper : accounts.value ) {
      auto a = account_in_wrapper;
      total_hive += a.balance;
      total_vest += a.vesting_shares;
      total_hbd  += a.hbd_balance;

      if( format == format_type::textformat )
      {
        out_text << std::left << setw(17) << string(a.name)
            << std::right << setw(18) << hive::protocol::legacy_asset::from_asset(a.balance).to_string() <<" "
            << std::right << setw(26) << hive::protocol::legacy_asset::from_asset(a.vesting_shares).to_string() <<" "
            << std::right << setw(16) << hive::protocol::legacy_asset::from_asset(a.hbd_balance).to_string() <<"\n";
      }
      else//format_type::json
      {
        out_json.accounts.emplace_back( list_my_accounts_json_account{ a.name, a.balance, a.vesting_shares, a.hbd_balance } );
      }
    }

    if( format == format_type::textformat )
    {
      out_text << "-------------------------------------------------------------------------------\n";
      out_text << std::left << setw(17) << "TOTAL"
          << std::right << setw(18) << hive::protocol::legacy_asset::from_asset(total_hive).to_string() <<" "
          << std::right << setw(26) << hive::protocol::legacy_asset::from_asset(total_vest).to_string() <<" "
          << std::right << setw(16) << hive::protocol::legacy_asset::from_asset(total_hbd).to_string() <<"\n";
    }
    else//format_type::json
    {
      out_json.total_hive = total_hive;
      out_json.total_vest = total_vest;
      out_json.total_hbd  = total_hbd;
    }

    return get_result( out_text, out_json, format );
  }

  static std::pair<string, string> get_name_content( const serializer_wrapper<protocol::operation>& op )
  {
    variant _v;
    to_variant( op, _v );

    if( _v.is_array() )
      return std::make_pair( _v.get_array()[0].as_string(), fc::json::to_string( _v.get_array()[1] ) );
    else
      return std::make_pair( _v.get_object()["type"].as_string(), fc::json::to_string( _v.get_object()["value"] ) );
  };

  static void create_get_account_history_header( std::stringstream& out_text )
  {
    out_text << std::left << setw( 8 )  << "#" << " ";
    out_text << std::left << setw( 10 ) << "BLOCK #" << " ";
    out_text << std::left << setw( 50 ) << "TRX ID" << " ";
    out_text << std::left << setw( 20 ) << "OPERATION" << " ";
    out_text << std::left << setw( 50 ) << "DETAILS" << "\n";
    out_text << "---------------------------------------------------------------------------------------------------\n";
  }

  static void create_get_account_history_body( std::stringstream& out_text, uint32_t pos, uint32_t block, const protocol::transaction_id_type& trx_id, const protocol::operation& op )
  {
    out_text << std::left << setw(8) << pos << " ";
    out_text << std::left << setw(10) << block << " ";
    out_text << std::left << setw(50) << trx_id.str() << " ";

    serializer_wrapper<protocol::operation> _op = { op };
    auto _name_content = get_name_content( _op );
    out_text << std::left << setw(20) << _name_content.first << " ";
    out_text << std::left << setw(50) << _name_content.second << "\n";
  }

  static variant get_account_history( const variant& result )
  {
    return get_account_history_impl( result.as<serializer_wrapper<std::map<uint32_t, account_history::api_operation_object>>>(), format_type::textformat );
  }

  static variant get_account_history_impl( const serializer_wrapper<std::map<uint32_t, account_history::api_operation_object>>& history, format_type format )
  {
    if( format == format_type::noformat )
    {
      variant _result;
      to_variant( history, _result );
      return _result;
    }

    std::stringstream               out_text;
    get_account_history_json_return out_json;

    if( format == format_type::textformat )
      create_get_account_history_header( out_text );

    for( const auto& item : history.value )
    {
      if( format == format_type::textformat )
        create_get_account_history_body( out_text, item.first, item.second.block, item.second.trx_id, item.second.op );
      else
        out_json.ops.emplace_back( get_account_history_json_op{ item.first, item.second.block, item.second.trx_id, item.second.op } );
    }

    return get_result( out_text, out_json, format );
  }

  static double calculate_price( const protocol::price& price )
  {
    if( price.base.symbol == HIVE_SYMBOL )
      return ASSET_TO_REAL( price.quote ) / ASSET_TO_REAL( price.base );
    else
      return ASSET_TO_REAL( price.base ) / ASSET_TO_REAL( price.quote );
  }

  static variant get_open_orders( const variant& result )
  {
    return get_open_orders_impl( result.as<serializer_wrapper<vector<database_api::api_limit_order_object>>>(), format_type::textformat );
  }

  static variant get_open_orders_impl( const serializer_wrapper<vector<database_api::api_limit_order_object>>& orders, format_type format )
  {
    if( format == format_type::noformat )
    {
      variant _result;
      to_variant( orders, _result );
      return _result;
    }

    std::stringstream           out_text;
    get_open_orders_json_return out_json;

    if( format == format_type::textformat )
    {
      out_text << setiosflags( ios::fixed ) << setiosflags( ios::left ) ;
      out_text << ' ' << setw( 10 ) << "Order #";
      out_text << ' ' << setw( 12 ) << "Price";
      out_text << ' ' << setw( 14 ) << "Quantity";
      out_text << ' ' << setw( 10 ) << "Type";
      out_text << "\n===============================================================================\n";
    }

    for( const auto& order_in_wrapper : orders.value )
    {
      auto o = order_in_wrapper;

      auto _asset = asset( o.for_sale, o.sell_price.base.symbol );
      string _type = o.sell_price.base.symbol == HIVE_SYMBOL ? "SELL" : "BUY";

      double _price = calculate_price( o.sell_price );

      if( format == format_type::textformat )
      {
        out_text << ' ' << setw( 10 ) << o.orderid;
        out_text << ' ' << setw( 12 ) << _price;
        out_text << ' ' << setw( 14 ) << hive::protocol::legacy_asset::from_asset( _asset ).to_string();
        out_text << ' ' << setw( 10 ) << _type;
        out_text << "\n";
      }
      else
      {
        out_json.orders.emplace_back( get_open_orders_json_order{ o.orderid, _price, _asset, _type } );
      }
    }

    return get_result( out_text, out_json, format );
  }

  static variant get_order_book( const variant& result )
  {
    return get_order_book_impl( result.as<serializer_wrapper<market_history::get_order_book_return>>(), format_type::textformat );
  }

  static variant get_order_book_impl( const serializer_wrapper<market_history::get_order_book_return>& orders_in_wrapper, format_type format )
  {
    if( format == format_type::noformat )
    {
      variant _result;
      to_variant( orders_in_wrapper, _result );
      return _result;
    }

    std::stringstream           out_text;
    get_order_book_json_return  out_json;

    auto orders = orders_in_wrapper.value;

    asset _bid_sum = asset( 0, HBD_SYMBOL );
    asset _ask_sum = asset( 0, HBD_SYMBOL );
    int spacing = 16;

    if( format == format_type::textformat )
    {
      out_text << setiosflags( ios::fixed ) << setiosflags( ios::left ) ;

      out_text << ' ' << setw( ( spacing * 4 ) + 6 ) << "Bids" << "Asks\n"
        << ' '
        << setw( spacing + 3 ) << "Sum(HBD)"
        << setw( spacing + 1 ) << "HBD"
        << setw( spacing + 1 ) << "HIVE"
        << setw( spacing + 1 ) << "Price"
        << setw( spacing + 1 ) << "Price"
        << setw( spacing + 1 ) << "HIVE "
        << setw( spacing + 1 ) << "HBD " << "Sum(HBD)"
        << "\n====================================================================="
        << "|=====================================================================\n";
    }

    for( size_t i = 0; i < orders.bids.size() || i < orders.asks.size(); i++ )
    {
      if( i < orders.bids.size() )
      {
        _bid_sum += asset( orders.bids[i].hbd, HBD_SYMBOL );

        auto _hbd = asset( orders.bids[i].hbd, HBD_SYMBOL);
        auto _hive = asset( orders.bids[i].hive, HIVE_SYMBOL);
        double _price = orders.bids[i].real_price;

        if( format == format_type::textformat )
        {
          out_text
            << ' ' << setw( spacing ) << hive::protocol::legacy_asset::from_asset( _bid_sum ).to_string()
            << ' ' << setw( spacing ) << hive::protocol::legacy_asset::from_asset( _hbd ).to_string()
            << ' ' << setw( spacing ) << hive::protocol::legacy_asset::from_asset( _hive ).to_string()
            << ' ' << setw( spacing ) << std::to_string( _price );
        }
        else
        {
          out_json.bids.emplace_back( get_order_book_json_order{ _hive, _hbd, _bid_sum, _price } );
        }
      }
      else
      {
        if( format == format_type::textformat )
          out_text << setw( (spacing * 4 ) + 5 ) << ' ';
      }

      if( format == format_type::textformat )
        out_text << " |";

      if( i < orders.asks.size() )
      {
        _ask_sum += asset(orders.asks[i].hbd, HBD_SYMBOL);

        auto _hbd = asset( orders.asks[i].hbd, HBD_SYMBOL);
        auto _hive = asset( orders.asks[i].hive, HIVE_SYMBOL);
        double _price = orders.asks[i].real_price;

        if( format == format_type::textformat )
        {
          out_text
            << ' ' << setw( spacing ) << std::to_string( _price )
            << ' ' << setw( spacing ) << hive::protocol::legacy_asset::from_asset( _hive ).to_string()
            << ' ' << setw( spacing ) << hive::protocol::legacy_asset::from_asset( _hbd ).to_string()
            << ' ' << setw( spacing ) << hive::protocol::legacy_asset::from_asset( _ask_sum ).to_string();
        }
        else
        {
          out_json.asks.emplace_back( get_order_book_json_order{ _hive, _hbd, _ask_sum, _price } );
        }
      }

      if( format == format_type::textformat )
        out_text << endl;
    }

    if( format == format_type::textformat )
    {
      out_text << endl
        << "Bid Total: " << hive::protocol::legacy_asset::from_asset(_bid_sum).to_string() << endl
        << "Ask Total: " << hive::protocol::legacy_asset::from_asset(_ask_sum).to_string() << endl;
    }
    else
    {
      out_json.bid_total = _bid_sum;
      out_json.ask_total = _ask_sum;
    }

    return get_result( out_text, out_json, format );
  }

  static variant get_withdraw_routes( const variant& result )
  {
    return get_withdraw_routes_impl( result.as<serializer_wrapper<vector<database_api::api_withdraw_vesting_route_object>>>(), format_type::textformat );
  }

  static variant get_withdraw_routes_impl( const serializer_wrapper<vector<database_api::api_withdraw_vesting_route_object>>& routes, format_type format )
  {
    if( format == format_type::noformat )
      return variant{ routes };

    std::stringstream                         out_text;
    find_withdraw_vesting_routes_json_return  out_json;

    if( format == format_type::textformat )
    {
      out_text << ' ' << std::left << setw( 18 ) << "From";
      out_text << ' ' << std::left << setw( 18 ) << "To";
      out_text << ' ' << std::right << setw( 8 ) << "Percent";
      out_text << ' ' << std::right << setw( 9 ) << "Auto-Vest";
      out_text << "\n=========================================================\n";
    }

    for( auto& r : routes.value )
    {
      if( format == format_type::textformat )
      {
        out_text << ' ' << std::left << setw( 18 ) << string( r.from_account );
        out_text << ' ' << std::left << setw( 18 ) << string( r.to_account );
        out_text << ' ' << std::right << setw( 8 ) << std::setprecision( 2 ) << std::fixed << double( r.percent ) / 100;
        out_text << ' ' << std::right << setw( 9 ) << ( r.auto_vest ? "true" : "false" ) << std::endl;
      }
      else
      {
        out_json.routes.emplace_back( find_withdraw_vesting_json_route{ r.from_account, r.to_account, r.percent, r.auto_vest } );
      }
    }

    return get_result( out_text, out_json, format );
  }

  static std::map<string, function_type> get_result_formatters()
  {
    static std::map<string, function_type> _formatters =
    {
      {"help",                help},
      {"gethelp",             gethelp},
      {"list_my_accounts",    list_my_accounts},
      {"get_account_history", get_account_history},
      {"get_open_orders",     get_open_orders},
      {"get_order_book",      get_order_book},
      {"get_withdraw_routes", get_withdraw_routes}
    };

    return _formatters;
  }

};

} } } //hive::plugins::wallet_bridge_api
