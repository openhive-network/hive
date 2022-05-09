#pragma once

#include <fc/variant.hpp>

#include <hive/wallet/wallet_serializer_wrapper.hpp>
#include <hive/protocol/asset.hpp>
#include <hive/plugins/wallet_bridge_api/wallet_bridge_api_args.hpp>

#include<vector>
#include<string>
#include<functional>
#include <iomanip>

#include <boost/algorithm/string.hpp>

#define ASSET_TO_REAL( asset ) (double)( asset.amount.value )

namespace hive { namespace wallet {

using namespace hive::plugins::wallet_bridge_api;

enum class output_formatter_type : uint8_t { none, text, json };

using std::function;
using fc::variant;
using fc::variants;
using std::string;
using hive::protocol::asset;
using std::setiosflags;
using std::ios;
using std::setw;
using std::endl;

struct wallet_formatter
{

  template<typename T>
  static variant get_result( const std::stringstream& out_text, const T& out_json, output_formatter_type output_formatter )
  {
    if( output_formatter == output_formatter_type::text )
    {
      return variant( out_text.str() );
    }
    else//output_formatter_type::json
    {
      variant _result;
      to_variant( out_json, _result );
      return _result;
    }
  }

  static string general_formatter( variant v, const variants& )
  {
    if( v.is_string() )
      return v.as_string();
    else
      return fc::json::to_pretty_string( v );
  }

  static variant help( const std::vector<string>& info, output_formatter_type output_formatter )
  {
    std::stringstream _out_text;
    vector<string>    _out_json;

    if( output_formatter == output_formatter_type::text || output_formatter == output_formatter_type::none )
    {
      for( auto& item : info )
      {
        _out_text << item;
      }
      output_formatter = output_formatter_type::text;
    }
    else
    {
      for( auto& item : info )
      {
        if( !item.empty() && item[ item.size() - 1 ] == '\n' )
        {
          _out_json.emplace_back( item.substr(0, item.size() - 1 ) );
        }
        else
          _out_json.emplace_back( item );
      }
    }
    return get_result( _out_text, _out_json, output_formatter );
  }

  static variant gethelp( const string& info, output_formatter_type output_formatter )
  {
    if( output_formatter == output_formatter_type::none || output_formatter == output_formatter_type::text )
    {
      variant _result;
      to_variant( info, _result );
      return _result;
    }

    std::vector<string> _out_json;

    std::vector<string> _tmp;
    boost::split( _tmp, info, boost::is_any_of("\n") );
    for( auto& item : _tmp )
    {
      if( !item.empty() )
        _out_json.emplace_back( item );
    }

    return get_result( std::stringstream(), _out_json, output_formatter );
  }

  static variant list_my_accounts( const wallet_serializer_wrapper<list_my_accounts_return>& accounts, output_formatter_type output_formatter )
  {
    if( output_formatter == output_formatter_type::none )
    {
      variant _result;
      to_variant( accounts, _result );
      return _result;
    }

    std::stringstream             _out_text;
    list_my_accounts_json_return  _out_json;

    asset total_hive;
    asset total_vest(0, VESTS_SYMBOL );
    asset total_hbd(0, HBD_SYMBOL );
    for( const auto& account_in_wrapper : accounts.value ) {
      auto a = account_in_wrapper;
      total_hive += a.balance;
      total_vest += a.vesting_shares;
      total_hbd  += a.hbd_balance;

      if( output_formatter == output_formatter_type::text )
      {
        _out_text << std::left << setw(17) << string(a.name)
            << std::right << setw(18) << hive::protocol::legacy_asset::from_asset(a.balance).to_string() <<" "
            << std::right << setw(26) << hive::protocol::legacy_asset::from_asset(a.vesting_shares).to_string() <<" "
            << std::right << setw(16) << hive::protocol::legacy_asset::from_asset(a.hbd_balance).to_string() <<"\n";
      }
      else//output_formatter_type::json
      {
        _out_json.accounts.emplace_back( list_my_accounts_json_account{ a.name, a.balance, a.vesting_shares, a.hbd_balance } );
      }
    }

    if( output_formatter == output_formatter_type::text )
    {
      _out_text << "-------------------------------------------------------------------------------\n";
      _out_text << std::left << setw(17) << "TOTAL"
          << std::right << setw(18) << hive::protocol::legacy_asset::from_asset(total_hive).to_string() <<" "
          << std::right << setw(26) << hive::protocol::legacy_asset::from_asset(total_vest).to_string() <<" "
          << std::right << setw(16) << hive::protocol::legacy_asset::from_asset(total_hbd).to_string() <<"\n";
    }
    else//output_formatter_type::json
    {
      _out_json.total_hive = total_hive;
      _out_json.total_vest = total_vest;
      _out_json.total_hbd  = total_hbd;
    }

    return get_result( _out_text, _out_json, output_formatter );
  }

  static std::pair<string, string> get_name_content( const protocol::operation& op )
  {
    variant _v;
    to_variant( wallet_serializer_wrapper<protocol::operation>{ op }, _v );

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

    auto _name_content = get_name_content( op );
    out_text << std::left << setw(20) << _name_content.first << " ";
    out_text << std::left << setw(50) << _name_content.second << "\n";
  }

  static variant get_account_history( const wallet_serializer_wrapper<get_account_history_return>& history, output_formatter_type output_formatter )
  {
    if( output_formatter == output_formatter_type::none )
    {
      variant _result;
      to_variant( history, _result );
      return _result;
    }

    std::stringstream                   _out_text;
    vector<get_account_history_json_op> _out_json;

    if( output_formatter == output_formatter_type::text )
      create_get_account_history_header( _out_text ); 

    for( const auto& item : history.value )
    {
      if( output_formatter == output_formatter_type::text )
        create_get_account_history_body( _out_text, item.first, item.second.block, item.second.trx_id, item.second.op );
      else
        _out_json.emplace_back( get_account_history_json_op{ item.first, item.second.block, item.second.trx_id, item.second.op } );
    }

    return get_result( _out_text, _out_json, output_formatter );
  }

  static double calculate_price( const protocol::price& price )
  {
    if( price.base.symbol == HIVE_SYMBOL )
      return ASSET_TO_REAL( price.quote ) / ASSET_TO_REAL( price.base );
    else
      return ASSET_TO_REAL( price.base ) / ASSET_TO_REAL( price.quote );
  }

  static variant get_open_orders( const wallet_serializer_wrapper<get_open_orders_return>& orders, output_formatter_type output_formatter )
  {
    if( output_formatter == output_formatter_type::none )
    {
      variant _result;
      to_variant( orders, _result );
      return _result;
    }

    std::stringstream           _out_text;
    get_open_orders_json_return _out_json;

    if( output_formatter == output_formatter_type::text )
    {
      _out_text << setiosflags( ios::fixed ) << setiosflags( ios::left ) ;
      _out_text << ' ' << setw( 10 ) << "Order #";
      _out_text << ' ' << setw( 12 ) << "Price";
      _out_text << ' ' << setw( 14 ) << "Quantity";
      _out_text << ' ' << setw( 10 ) << "Type";
      _out_text << "\n===============================================================================\n";
    }

    for( const auto& order_in_wrapper : orders.value )
    {
      auto o = order_in_wrapper;

      auto _asset = asset( o.for_sale, o.sell_price.base.symbol );
      string _type = o.sell_price.base.symbol == HIVE_SYMBOL ? "SELL" : "BUY";

      double _price = calculate_price( o.sell_price );

      if( output_formatter == output_formatter_type::text )
      {
        _out_text << ' ' << setw( 10 ) << o.orderid;
        _out_text << ' ' << setw( 12 ) << _price;
        _out_text << ' ' << setw( 14 ) << hive::protocol::legacy_asset::from_asset( _asset ).to_string();
        _out_text << ' ' << setw( 10 ) << _type;
        _out_text << "\n";
      }
      else
      {
        _out_json.orders.emplace_back( get_open_orders_json_order{ o.orderid, _price, _asset, _type } );
      }
    }

    return get_result( _out_text, _out_json, output_formatter );
  }

  static variant get_order_book( const wallet_serializer_wrapper<get_order_book_return>& orders_in_wrapper, output_formatter_type output_formatter )
  {
    if( output_formatter == output_formatter_type::none )
    {
      variant _result;
      to_variant( orders_in_wrapper, _result );
      return _result;
    }

    std::stringstream           _out_text;
    get_order_book_json_return  _out_json;

    auto orders = orders_in_wrapper.value;

    asset _bid_sum = asset( 0, HBD_SYMBOL );
    asset _ask_sum = asset( 0, HBD_SYMBOL );
    int spacing = 16;

    if( output_formatter == output_formatter_type::text )
    {
      _out_text << setiosflags( ios::fixed ) << setiosflags( ios::left ) ;

      _out_text << ' ' << setw( ( spacing * 4 ) + 6 ) << "Bids" << "Asks\n"
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

        if( output_formatter == output_formatter_type::text )
        {
          _out_text
            << ' ' << setw( spacing ) << hive::protocol::legacy_asset::from_asset( _bid_sum ).to_string()
            << ' ' << setw( spacing ) << hive::protocol::legacy_asset::from_asset( _hbd ).to_string()
            << ' ' << setw( spacing ) << hive::protocol::legacy_asset::from_asset( _hive ).to_string()
            << ' ' << setw( spacing ) << std::to_string( _price );
        }
        else
        {
          _out_json.bids.emplace_back( get_order_book_json_order{ _hive, _hbd, _bid_sum, _price } );
        }
      }
      else
      {
        if( output_formatter == output_formatter_type::text )
          _out_text << setw( (spacing * 4 ) + 5 ) << ' ';
      }

      if( output_formatter == output_formatter_type::text )
        _out_text << " |";

      if( i < orders.asks.size() )
      {
        _ask_sum += asset(orders.asks[i].hbd, HBD_SYMBOL);

        auto _hbd = asset( orders.asks[i].hbd, HBD_SYMBOL);
        auto _hive = asset( orders.asks[i].hive, HIVE_SYMBOL);
        double _price = orders.asks[i].real_price;

        if( output_formatter == output_formatter_type::text )
        {
          _out_text
            << ' ' << setw( spacing ) << std::to_string( _price )
            << ' ' << setw( spacing ) << hive::protocol::legacy_asset::from_asset( _hive ).to_string()
            << ' ' << setw( spacing ) << hive::protocol::legacy_asset::from_asset( _hbd ).to_string()
            << ' ' << setw( spacing ) << hive::protocol::legacy_asset::from_asset( _ask_sum ).to_string();
        }
        else
        {
          _out_json.asks.emplace_back( get_order_book_json_order{ _hive, _hbd, _ask_sum, _price } );
        }
      }

      if( output_formatter == output_formatter_type::text )
        _out_text << endl;
    }

    if( output_formatter == output_formatter_type::text )
    {
      _out_text << endl
        << "Bid Total: " << hive::protocol::legacy_asset::from_asset(_bid_sum).to_string() << endl
        << "Ask Total: " << hive::protocol::legacy_asset::from_asset(_ask_sum).to_string() << endl;
    }
    else
    {
      _out_json.bid_total = _bid_sum;
      _out_json.ask_total = _ask_sum;
    }

    return get_result( _out_text, _out_json, output_formatter );
  }

  static variant get_withdraw_routes( const wallet_serializer_wrapper<get_withdraw_routes_return>& routes, output_formatter_type output_formatter )
  {
    if( output_formatter == output_formatter_type::none )
      return variant{ routes };

    std::stringstream                         _out_text;
    vector<find_withdraw_vesting_json_route>  _out_json;

    if( output_formatter == output_formatter_type::text )
    {
      _out_text << ' ' << std::left << setw( 18 ) << "From";
      _out_text << ' ' << std::left << setw( 18 ) << "To";
      _out_text << ' ' << std::right << setw( 8 ) << "Percent";
      _out_text << ' ' << std::right << setw( 9 ) << "Auto-Vest";
      _out_text << "\n=========================================================\n";
    }

    for( auto& r : routes.value )
    {
      if( output_formatter == output_formatter_type::text )
      {
        _out_text << ' ' << std::left << setw( 18 ) << string( r.from_account );
        _out_text << ' ' << std::left << setw( 18 ) << string( r.to_account );
        _out_text << ' ' << std::right << setw( 8 ) << std::setprecision( 2 ) << std::fixed << double( r.percent ) / 100;
        _out_text << ' ' << std::right << setw( 9 ) << ( r.auto_vest ? "true" : "false" ) << std::endl;
      }
      else
      {
        _out_json.emplace_back( find_withdraw_vesting_json_route{ r.from_account, r.to_account, r.percent, r.auto_vest } );
      }
    }

    return get_result( _out_text, _out_json, output_formatter );
  }

  static std::map<string, std::function<string(variant,const variants&)>> get_result_formatters()
  {
    static std::map<string, std::function<string(variant,const variants&)>> m = 
    {
      { "help",                 general_formatter },
      { "gethelp",              general_formatter },
      { "list_my_accounts",     general_formatter },
      { "get_account_history",  general_formatter },
      { "get_open_orders",      general_formatter },
      { "get_order_book",       general_formatter },
      { "get_withdraw_routes",  general_formatter }
    };

    return m;
  }
};

} } //hive::wallet

FC_REFLECT_ENUM( hive::wallet::output_formatter_type, (none)(text)(json) )
