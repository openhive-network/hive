#pragma once
#include <hive/chain/hive_fwd.hpp>
#include <hive/chain/util/balance.hpp>
#include <hive/chain/hive_object_types.hpp>

namespace hive { namespace chain {

using hive::protocol::asset;
using hive::protocol::price;
using hive::protocol::asset_symbol_type;
class account_object;

/**
  *  @brief an offer to sell a amount of a asset at a specified exchange rate by a certain time
  *  @ingroup object
  *  @ingroup protocol
  *  @ingroup market
  *
  *  This limit_order_objects are indexed by @ref expiration and is automatically deleted on the first block after expiration.
  */
class limit_order_object : public object< limit_order_object_type, limit_order_object >
{
  CHAINBASE_OBJECT( limit_order_object );

public:
  template< typename Allocator >
  limit_order_object( allocator< Allocator > a, uint64_t _id,
    const account_object& _seller, temp_balance&& _amount_to_sell, const price& _sell_price,
    const time_point_sec& _creation_time, const time_point_sec& _expiration_time, uint32_t _orderid )
  : id( _id ), created( _creation_time ), expiration( _expiration_time ), orderid( _orderid ),
    for_sale( std::move( _amount_to_sell ) ), sell_price( _sell_price )
  {
    init( _seller );
    FC_ASSERT( for_sale.as_asset().symbol == _sell_price.base.symbol );
  }

  pair< asset_symbol_type, asset_symbol_type > get_market() const
  {
    return sell_price.base.symbol < sell_price.quote.symbol ?
      std::make_pair( sell_price.base.symbol, sell_price.quote.symbol ) :
      std::make_pair( sell_price.quote.symbol, sell_price.base.symbol );
  }

// getters:

  const asset& amount_for_sale() const { return for_sale; }
  asset amount_to_receive() const { return for_sale * sell_price; }

  time_point_sec get_created() const { return created; }
  time_point_sec get_expiration() const { return expiration; }
  uint32_t get_orderid() const { return orderid; }
  const account_name_type& get_seller() const { return seller; }
  const price& get_sell_price() const { return sell_price; }

  void check_on_remove() const
  {
    FC_ASSERT( for_sale.is_empty(), "Removing limit_order_object with non-empty balance field" );
  }

// setters:

  balance& access_amount_for_sale() { return for_sale; }

private:
  time_point_sec    created;
  time_point_sec    expiration;
  uint32_t          orderid = 0;
  account_name_type seller; ///< changing to account_id_type is not possible - by_account is used by database_api.list_limit_orders
  balance           for_sale;
  price             sell_price;

  void init( const account_object& _seller );

  CHAINBASE_UNPACK_CONSTRUCTOR( limit_order_object );
};

} } // hive::chain

FC_REFLECT( hive::chain::limit_order_object,
          (id)(created)(expiration)(orderid)(seller)(for_sale)(sell_price) )
