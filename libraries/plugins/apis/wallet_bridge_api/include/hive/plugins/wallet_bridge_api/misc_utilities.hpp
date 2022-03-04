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
  template<typename T>
  static variant get_result( const std::stringstream& out_text, const T& out_json, format_type format )
  {
    if( format == format_type::text )
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

  static variant list_my_accounts( const serializer_wrapper<vector<database_api::api_account_object>>& accounts, format_type format )
  {
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

      if( format == format_type::text )
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

    if( format == format_type::text )
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

  static variant get_account_history( const hive::plugins::account_history::get_account_history_return& ops, format_type format )
  {
    std::stringstream               out_text;
    get_account_history_json_return out_json;

    if( format == format_type::text )
      create_get_account_history_header( out_text );

    for( const auto& item : ops.history )
    {
      if( format == format_type::text )
        create_get_account_history_body( out_text, item.first, item.second.block, item.second.trx_id, item.second.op );
      else
        out_json.ops.emplace_back( get_account_history_json_op{ item.first, item.second.block, item.second.trx_id, item.second.op } );
    }

    return get_result( out_text, out_json, format );
  }

  static variant get_account_history( const get_account_history_json_return& ops )
  {
    std::stringstream               out_text;
    get_account_history_json_return out_json;

    create_get_account_history_header( out_text );

    for( const auto& item : ops.ops )
      create_get_account_history_body( out_text, item.pos, item.block, item.id, item.op );

    return get_result( out_text, out_json, format_type::text );
  }

  static variant get_open_orders( serializer_wrapper<vector<database_api::api_limit_order_object>> orders, format_type format )
  {
    std::stringstream           out_text;
    get_open_orders_json_return out_json;

    if( format == format_type::text )
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

      double _real_price;
      if( o.sell_price.base.symbol == HIVE_SYMBOL )
        _real_price = ASSET_TO_REAL( o.sell_price.quote ) / ASSET_TO_REAL( o.sell_price.base );
      else
        _real_price =  ASSET_TO_REAL( o.sell_price.base ) / ASSET_TO_REAL( o.sell_price.quote );

      if( format == format_type::text )
      {
        out_text << ' ' << setw( 10 ) << o.orderid;
        out_text << ' ' << setw( 12 ) << _real_price;
        out_text << ' ' << setw( 14 ) << hive::protocol::legacy_asset::from_asset( _asset ).to_string();
        out_text << ' ' << setw( 10 ) << _type;
        out_text << "\n";
      }
      else
      {
        out_json.orders.emplace_back( get_open_orders_json_order{ o.orderid, _real_price, _asset, _type } );
      }
    }

    return get_result( out_text, out_json, format );
  }

  static variant get_order_book( const serializer_wrapper<market_history::get_order_book_return>& orders_in_wrapper, format_type format )
  {
    std::stringstream           out_text;
    get_order_book_json_return  out_json;

    auto orders = orders_in_wrapper.value;

    asset bid_sum = asset( 0, HBD_SYMBOL );
    asset ask_sum = asset( 0, HBD_SYMBOL );
    int spacing = 16;

    if( format == format_type::text )
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
        bid_sum += asset( orders.bids[i].hbd, HBD_SYMBOL );

        auto _hbd = asset( orders.bids[i].hbd, HBD_SYMBOL);
        auto _hive = asset( orders.bids[i].hive, HIVE_SYMBOL);

        if( format == format_type::text )
        {
          out_text
            << ' ' << setw( spacing ) << hive::protocol::legacy_asset::from_asset(bid_sum).to_string()
            << ' ' << setw( spacing ) << hive::protocol::legacy_asset::from_asset( _hbd ).to_string()
            << ' ' << setw( spacing ) << hive::protocol::legacy_asset::from_asset( _hive ).to_string();
        }
        else
        {
          out_json.bids.emplace_back( get_order_book_json_order{ _hive, _hbd, bid_sum } );
        }
      }
      else
      {
        if( format == format_type::text )
          out_text << setw( (spacing * 4 ) + 5 ) << ' ';
      }

      if( format == format_type::text )
        out_text << " |";

      if( i < orders.asks.size() )
      {
        ask_sum += asset(orders.asks[i].hbd, HBD_SYMBOL);

        auto _hbd = asset( orders.asks[i].hbd, HBD_SYMBOL);
        auto _hive = asset( orders.asks[i].hive, HIVE_SYMBOL);

        if( format == format_type::text )
        {
          out_text
            << ' ' << setw( spacing ) << hive::protocol::legacy_asset::from_asset( _hive ).to_string()
            << ' ' << setw( spacing ) << hive::protocol::legacy_asset::from_asset( _hbd ).to_string()
            << ' ' << setw( spacing ) << hive::protocol::legacy_asset::from_asset(ask_sum).to_string();
        }
        else
        {
          out_json.asks.emplace_back( get_order_book_json_order{ _hive, _hbd, ask_sum } );
        }
      }

      if( format == format_type::text )
        out_text << endl;
    }

    if( format == format_type::text )
    {
      out_text << endl
        << "Bid Total: " << hive::protocol::legacy_asset::from_asset(bid_sum).to_string() << endl
        << "Ask Total: " << hive::protocol::legacy_asset::from_asset(ask_sum).to_string() << endl;
    }
    else
    {
      out_json.bid_total = bid_sum;
      out_json.ask_total = ask_sum;
    }

    return get_result( out_text, out_json, format );
  }

  static string get_withdraw_routes( variant result )
  {
    auto routes = result.as< vector< database_api::api_withdraw_vesting_route_object > >();
    std::stringstream ss;

    ss << ' ' << std::left << setw( 18 ) << "From";
    ss << ' ' << std::left << setw( 18 ) << "To";
    ss << ' ' << std::right << setw( 8 ) << "Percent";
    ss << ' ' << std::right << setw( 9 ) << "Auto-Vest";
    ss << "\n=========================================================\n";

    for( auto& r : routes )
    {
      ss << ' ' << std::left << setw( 18 ) << string( r.from_account );
      ss << ' ' << std::left << setw( 18 ) << string( r.to_account );
      ss << ' ' << std::right << setw( 8 ) << std::setprecision( 2 ) << std::fixed << double( r.percent ) / 100;
      ss << ' ' << std::right << setw( 9 ) << ( r.auto_vest ? "true" : "false" ) << std::endl;
    }

    return ss.str();
  }

};

} } } //hive::plugins::wallet_bridge_api
