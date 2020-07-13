#pragma once

#include <hive/chain/hive_object_types.hpp>
#include <hive/protocol/asset.hpp>

namespace hive { namespace chain {

  template< uint32_t _SYMBOL >
  class tiny_asset
  {
    using asset = protocol::asset;
    using asset_symbol_type = protocol::asset_symbol_type;

  public:

    share_type amount;

    tiny_asset() {}
    explicit tiny_asset( share_type _amount ) : amount( _amount ) {}
    tiny_asset( const tiny_asset& val ) = default;

    tiny_asset& operator=( const tiny_asset& val ) = default;

    asset to_asset() const { return asset( amount, get_symbol() ); }
    static bool is_compatible( const asset& a ) { return a.symbol.asset_num == _SYMBOL; }
    static asset_symbol_type get_symbol() { return asset_symbol_type::from_asset_num( _SYMBOL ); }

    tiny_asset& operator+=( const tiny_asset& val ) { amount += val.amount; return *this; }
    tiny_asset& operator-=( const tiny_asset& val ) { amount -= val.amount; return *this; }
    tiny_asset& operator*=( const share_type& x ) { amount *= x; return *this; }
    tiny_asset& operator/=( const share_type& x ) { amount /= x; return *this; }
    tiny_asset& operator%=( const share_type& x ) { amount %= x; return *this; }

    bool operator==( const tiny_asset& other ) const { return amount == other.amount; }
    bool operator!=( const tiny_asset& other ) const { return amount != other.amount; }
    bool operator<( const tiny_asset& other ) const { return amount < other.amount; }
    bool operator<=( const tiny_asset& other ) const { return amount <= other.amount; }
    bool operator>( const tiny_asset& other ) const { return amount > other.amount; }
    bool operator>=( const tiny_asset& other ) const { return amount >= other.amount; }

    tiny_asset operator+() const { return tiny_asset( amount ); }
    tiny_asset operator-() const { return tiny_asset( -amount ); }

    tiny_asset operator+( const tiny_asset& other ) const { return tiny_asset( amount + other.amount ); }
    tiny_asset operator-( const tiny_asset& other ) const { return tiny_asset( amount - other.amount ); }

    friend tiny_asset operator* ( const tiny_asset& a, share_type b ) { return tiny_asset( a.amount * b ); }
    friend tiny_asset operator* ( share_type b, const tiny_asset& a ) { return tiny_asset( a.amount * b ); }
    friend tiny_asset operator/ ( const tiny_asset& a, share_type b ) { return tiny_asset( a.amount / b ); }
    friend tiny_asset operator% ( const tiny_asset& a, share_type b ) { return tiny_asset( a.amount % b ); }
  };

  using HBD_asset  = tiny_asset< HIVE_ASSET_NUM_HBD >;
  using HIVE_asset = tiny_asset< HIVE_ASSET_NUM_HIVE >;
  using VEST_asset = tiny_asset< HIVE_ASSET_NUM_VESTS >;

  inline HIVE_asset to_HIVE( const protocol::asset& a )
  {
    FC_ASSERT( HIVE_asset::is_compatible( a ), "Expected HIVE asset" );
    return HIVE_asset( a.amount );
  }

  inline HBD_asset to_HBD( const protocol::asset& a )
  {
    FC_ASSERT( HBD_asset::is_compatible( a ), "Expected HBD asset" );
    return HBD_asset( a.amount );
  }

  inline VEST_asset to_VEST( const protocol::asset& a )
  {
    FC_ASSERT( VEST_asset::is_compatible( a ), "Expected VESTS asset" );
    return VEST_asset( a.amount );
  }

}}

FC_REFLECT( hive::chain::HBD_asset,  (amount) )
FC_REFLECT( hive::chain::HIVE_asset, (amount) )
FC_REFLECT( hive::chain::VEST_asset, (amount) )

namespace fc {

  template< uint32_t _SYMBOL >
  void to_variant( const hive::chain::tiny_asset< _SYMBOL >& var, fc::variant& vo )
  {
    to_variant( var.to_asset(), vo );
  }

  template< uint32_t _SYMBOL >
  void from_variant( const fc::variant& var, hive::chain::tiny_asset< _SYMBOL >& vo )
  {
    try
    {
      hive::protocol::asset a;
      from_variant( var, a );
      FC_ASSERT( vo.is_compatible( a ), "Different asset expected ${ex}, got ${got}", ( "ex", vo.get_symbol() )( "got", a.symbol ) );
      vo = hive::chain::tiny_asset< _SYMBOL >( a.amount );
    } FC_CAPTURE_AND_RETHROW()
  }

}
