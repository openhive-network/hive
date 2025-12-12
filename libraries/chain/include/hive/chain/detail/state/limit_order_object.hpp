#pragma once
#include <hive/chain/hive_fwd.hpp>
#include <hive/protocol/asset.hpp>
#include <hive/chain/hive_object_types.hpp>

namespace hive { namespace chain {

  using hive::protocol::asset;
  using hive::protocol::price;
  using hive::protocol::asset_symbol_type;

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
        const account_name_type& _seller, const asset& _amount_to_sell, const price& _sell_price,
        const time_point_sec& _creation_time, const time_point_sec& _expiration_time, uint32_t _orderid )
        : id( _id ), created( _creation_time ), expiration( _expiration_time ), seller( _seller ),
        orderid( _orderid ), for_sale( _amount_to_sell.amount ), sell_price( _sell_price )
      {
        FC_ASSERT( _amount_to_sell.symbol == _sell_price.base.symbol );
      }

      pair< asset_symbol_type, asset_symbol_type > get_market() const
      {
        return sell_price.base.symbol < sell_price.quote.symbol ?
          std::make_pair( sell_price.base.symbol, sell_price.quote.symbol ) :
          std::make_pair( sell_price.quote.symbol, sell_price.base.symbol );
      }

      asset amount_for_sale() const { return asset( for_sale, sell_price.base.symbol ); }
      asset amount_to_receive() const { return amount_for_sale() * sell_price; }

      time_point_sec    created;
      time_point_sec    expiration;
      account_name_type seller; //< TODO: can be replaced with account_id_type
      uint32_t          orderid = 0;
      share_type        for_sale; ///< asset id is sell_price.base.symbol
      price             sell_price;

      CHAINBASE_UNPACK_CONSTRUCTOR(limit_order_object);
  };

} } // hive::chain

FC_REFLECT( hive::chain::limit_order_object,
          (id)(created)(expiration)(seller)(orderid)(for_sale)(sell_price) )
