#pragma once
#include <hive/protocol/types.hpp>
#include <hive/protocol/config.hpp>
#include <hive/protocol/asset_symbol.hpp>

namespace hive { namespace protocol {

  template< uint32_t _SYMBOL >
  struct tiny_asset;

  using HBD_asset = tiny_asset< HIVE_ASSET_NUM_HBD >;
  using HIVE_asset = tiny_asset< HIVE_ASSET_NUM_HIVE >;
  using VEST_asset = tiny_asset< HIVE_ASSET_NUM_VESTS >;

  struct asset
  {
    asset( const asset& _asset, asset_symbol_type id )
      : amount( _asset.amount ), symbol( id ) {}

    asset( share_type a, asset_symbol_type id )
      : amount( a ), symbol( id ) {}

    asset()
      : amount( 0 ), symbol( HIVE_SYMBOL ) {}

    template< uint32_t _SYMBOL >
    inline explicit asset( const tiny_asset< _SYMBOL > _asset );

    template< uint32_t _SYMBOL >
    inline asset& operator = ( const tiny_asset< _SYMBOL > _asset );

    share_type        amount;
    asset_symbol_type symbol;

    inline HBD_asset to_HBD() const;
    inline HIVE_asset to_HIVE() const;
    inline VEST_asset to_VEST() const;

    void validate()const;

    asset& operator += ( const asset& o )
    {
      FC_ASSERT( symbol == o.symbol );
      amount += o.amount;
      return *this;
    }

    asset& operator -= ( const asset& o )
    {
      FC_ASSERT( symbol == o.symbol );
      amount -= o.amount;
      return *this;
    }

    asset operator -()const { return asset( -amount, symbol ); }

    friend bool operator == ( const asset& a, const asset& b )
    {
      return std::tie( a.symbol, a.amount ) == std::tie( b.symbol, b.amount );
    }

    friend bool operator < ( const asset& a, const asset& b )
    {
      FC_ASSERT( a.symbol == b.symbol );
      return a.amount < b.amount;
    }

    friend bool operator <= ( const asset& a, const asset& b )
    {
      FC_ASSERT( a.symbol == b.symbol );
      return a.amount <= b.amount;
    }

    friend bool operator != ( const asset& a, const asset& b )
    {
      FC_ASSERT( a.symbol == b.symbol );
      return a.amount != b.amount;
    }

    friend bool operator > ( const asset& a, const asset& b )
    {
      FC_ASSERT( a.symbol == b.symbol );
      return a.amount > b.amount;
    }

    friend bool operator >= ( const asset& a, const asset& b )
    {
      FC_ASSERT( a.symbol == b.symbol );
      return a.amount >= b.amount;
    }

    friend asset operator - ( const asset& a, const asset& b )
    {
      FC_ASSERT( a.symbol == b.symbol );
      return asset( a.amount - b.amount, a.symbol );
    }

    friend asset operator + ( const asset& a, const asset& b )
    {
      FC_ASSERT( a.symbol == b.symbol );
      return asset( a.amount + b.amount, a.symbol );
    }

    friend asset operator * ( const asset& a, const asset& b )
    {
      FC_ASSERT( a.symbol == b.symbol );
      return asset( a.amount * b.amount, a.symbol );
    }
  };

  /** Represents quotation of the relative value of asset against another asset.
      Similar to 'currency pair' used to determine value of currencies.

      For example:
      1 EUR / 1.25 USD where:
      1 EUR is an asset specified as a base
      1.25 USD us an asset specified as a qute

      can determine value of EUR against USD.
  */
  struct price
  {
    /** Even non-single argument, lets make it an explicit one to avoid implicit calls for
        initialization lists.

        \param base  - represents a value of the price object to be expressed relatively to quote
                  asset. Cannot have amount == 0 if you want to build valid price.
        \param quote - represents an relative asset. Cannot have amount == 0, otherwise
                  asertion fail.

      Both base and quote shall have different symbol defined, since it also results in
      creation of invalid price object. \see validate() method.
    */
    explicit price(const asset& base, const asset& quote) : base(base),quote(quote)
    {
        /// Call validate to verify passed arguments. \warning It throws on error.
        validate();
    }

    /** Default constructor is needed because of fc::variant::as method requirements.
    */
    price() = default;

    asset base;
    asset quote;

    static price max(asset_symbol_type base, asset_symbol_type quote );
    static price min(asset_symbol_type base, asset_symbol_type quote );

    price max()const { return price::max( base.symbol, quote.symbol ); }
    price min()const { return price::min( base.symbol, quote.symbol ); }

    bool is_null()const;
    void validate()const;

  }; /// price

  price operator / ( const asset& base, const asset& quote );
  inline price operator~( const price& p ) { return price{p.quote,p.base}; }

  bool  operator <  ( const asset& a, const asset& b );
  bool  operator <= ( const asset& a, const asset& b );
  bool  operator <  ( const price& a, const price& b );
  bool  operator <= ( const price& a, const price& b );
  bool  operator >  ( const price& a, const price& b );
  bool  operator >= ( const price& a, const price& b );
  bool  operator == ( const price& a, const price& b );
  bool  operator != ( const price& a, const price& b );
  asset operator *  ( const asset& a, const price& b );

  /** Represents thin version of asset with fixed symbol.
   *  MUST NOT be used in operations (it needs to be clear what asset is part of
   *  operation without access to its definition). Might be used in some custom binary
   *  API responses where performance is essential and trust is a non-issue.
   *  This struct is used primarily to hold asset values in chain objects in places
   *  where use of specific asset is guaranteed.
   *
   *  NOTE: all conversions between asset and tiny_asset are explicit because:
   *  - conversions to asset bring to life more costly object
   *  - conversions from asset can throw exception
   */
  template< uint32_t _SYMBOL >
  struct tiny_asset
  {
    tiny_asset() {}
    explicit tiny_asset( share_type _amount ) : amount( _amount ) {}
    tiny_asset( const tiny_asset& val ) = default;

    tiny_asset& operator=( const tiny_asset& val ) = default;

    share_type amount;

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

  template< uint32_t _SYMBOL >
  asset::asset( const tiny_asset< _SYMBOL > _asset )
    : amount( _asset.amount ), symbol( _asset.get_symbol() ) {}

  template< uint32_t _SYMBOL >
  asset& asset::operator= ( const tiny_asset< _SYMBOL > _asset )
  {
    amount = _asset.amount;
    symbol = _asset.get_symbol();
    return *this;
  }

  inline HBD_asset asset::to_HBD() const
  {
    FC_ASSERT( HBD_asset::is_compatible( *this ), "Expected HBD asset" );
    return HBD_asset( amount );
  }

  inline HIVE_asset asset::to_HIVE() const
  {
    FC_ASSERT( HIVE_asset::is_compatible( *this ), "Expected HIVE asset" );
    return HIVE_asset( amount );
  }

  inline VEST_asset asset::to_VEST() const
  {
    FC_ASSERT( VEST_asset::is_compatible( *this ), "Expected VESTS asset" );
    return VEST_asset( amount );
  }

} } // hive::protocol

namespace fc {
  void to_variant( const hive::protocol::asset& var,  fc::variant& vo );
  void from_variant( const fc::variant& var,  hive::protocol::asset& vo );

  template< uint32_t _SYMBOL >
  void to_variant( const hive::protocol::tiny_asset< _SYMBOL >& var, fc::variant& vo )
  {
    to_variant( var.to_asset(), vo );
  }

  template< uint32_t _SYMBOL >
  void from_variant( const fc::variant& var, hive::protocol::tiny_asset< _SYMBOL >& vo )
  {
    try
    {
      hive::protocol::asset a;
      from_variant( var, a );
      FC_ASSERT( vo.is_compatible( a ), "Different asset expected ${ex}, got ${got}", ( "ex", vo.get_symbol() )( "got", a.symbol ) );
      vo = hive::protocol::tiny_asset< _SYMBOL >( a.amount );
    } FC_CAPTURE_AND_RETHROW()
  }
}

FC_REFLECT( hive::protocol::asset, (amount)(symbol) )
FC_REFLECT( hive::protocol::price, (base)(quote) )

FC_REFLECT( hive::protocol::HBD_asset, (amount) )
FC_REFLECT( hive::protocol::HIVE_asset, (amount) )
FC_REFLECT( hive::protocol::VEST_asset, (amount) )

