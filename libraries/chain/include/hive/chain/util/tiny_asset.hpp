#pragma once

#include <hive/chain/hive_object_types.hpp>
#include <hive/protocol/asset.hpp>

namespace hive
{
  namespace chain
  {
    template< uint32_t _SYMBOL >
    class tiny_asset
    {
      using asset = protocol::asset;

    public:

      share_type amount;

      tiny_asset() {}
      tiny_asset( const asset& val )       { set( val ); }
      tiny_asset( asset&& val )            { set( val ); }
      tiny_asset( const share_type& amount )
        : amount {amount}
      {}

      asset operator=( const asset& val )  { set( val ); return to_asset(); }
      asset operator=( asset&& val )       { set( val ); return to_asset(); }

      asset operator+=( const asset& val ) { check( val ); amount += val.amount; return to_asset(); }
      asset operator-=( const asset& val ) { check( val ); amount -= val.amount; return to_asset(); }

      tiny_asset< _SYMBOL > operator+=( const tiny_asset< _SYMBOL >& val )
      {
        return amount += val.amount;
      }
      tiny_asset< _SYMBOL > operator-=( const tiny_asset< _SYMBOL >& val )
      {
        return amount -= val.amount;
      }

      friend tiny_asset< _SYMBOL > operator+( const tiny_asset< _SYMBOL >& obj1, const tiny_asset< _SYMBOL >& obj2)
      {
        return tiny_asset< _SYMBOL >( obj1.amount + obj2.amount );
      }

      friend tiny_asset< _SYMBOL > operator-( const tiny_asset< _SYMBOL >& obj1, const tiny_asset< _SYMBOL >& obj2)
      {
        return tiny_asset< _SYMBOL >( obj1.amount - obj2.amount );
      }

      explicit operator asset() const               { return to_asset(); }

      asset to_asset() const               { return asset( amount, protocol::asset_symbol_type::from_asset_num( _SYMBOL ) ); }
      
    private:

      void set( const asset& val )         { check( val ); amount = val.amount; }
      void check( const asset& val ) const { FC_ASSERT( val.symbol.asset_num == _SYMBOL ); }
    };

    using HBD_asset  = tiny_asset< HIVE_ASSET_NUM_HBD >;
    using HIVE_asset = tiny_asset< HIVE_ASSET_NUM_HIVE >;
    using VEST_asset = tiny_asset< HIVE_ASSET_NUM_VESTS >;

    template< uint32_t _SYMBOL >
    bool operator==( const tiny_asset< _SYMBOL >& obj1, const tiny_asset< _SYMBOL >& obj2 ) { return obj1.amount == obj2.amount; }

    template< uint32_t _SYMBOL >
    bool operator!=( const tiny_asset< _SYMBOL >& obj1, const tiny_asset< _SYMBOL >& obj2 ) { return obj1.amount != obj2.amount; }

    template< uint32_t _SYMBOL >
    bool operator<( const tiny_asset< _SYMBOL >& obj1, const tiny_asset< _SYMBOL >& obj2 ) { return obj1.amount < obj2.amount; }

    template< uint32_t _SYMBOL >
    bool operator<=( const tiny_asset< _SYMBOL >& obj1, const tiny_asset< _SYMBOL >& obj2 ) { return obj1.amount <= obj2.amount; }

    template< uint32_t _SYMBOL >
    bool operator>=( const tiny_asset< _SYMBOL >& obj1, const tiny_asset< _SYMBOL >& obj2 ) { return obj1.amount >= obj2.amount; }

    template< uint32_t _SYMBOL >
    bool operator>( const tiny_asset< _SYMBOL >& obj1, const tiny_asset< _SYMBOL >& obj2 ) { return obj1.amount > obj2.amount; }

    template< uint32_t _SYMBOL >
    protocol::asset operator-( const tiny_asset< _SYMBOL >& obj1) { return -static_cast< protocol::asset >( obj1 ); }

  }
}

FC_REFLECT( hive::chain::HBD_asset,  (amount) )
FC_REFLECT( hive::chain::HIVE_asset, (amount) )
FC_REFLECT( hive::chain::VEST_asset, (amount) )
