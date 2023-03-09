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

    share_type        amount;
    asset_symbol_type symbol;

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
  };

  struct legacy_asset
  {
    public:
      legacy_asset() {}

      legacy_asset( const asset& a )
      {
        amount = a.amount;
        symbol = a.symbol;
      }

      asset to_asset()const
      {
        return asset( amount, symbol );
      }

      operator asset()const { return to_asset(); }

      static legacy_asset from_asset( const asset& a )
      {
        return legacy_asset( a );
      }

      std::string asset_num_to_string() const;

      string to_string()const;
      static legacy_asset from_string( const string& from );

      share_type                       amount;
      asset_symbol_type                symbol = HIVE_SYMBOL;
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
    price(const asset& _base, const asset& _quote) : base(_base),quote(_quote)
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

  /** Applies price to given asset in order to calculate its value in the second asset (like operator* ).
      Additionally applies fee scale factor to specific asset in price. Used f.e. to apply fee to
      collateralized conversions. Fee scale parameter in basis points.
    */
  asset multiply_with_fee( const asset& a, const price& p, uint16_t fee, asset_symbol_type apply_fee_to );

  /** Represents thin version of asset with fixed symbol.
   *  MUST NOT be used in operations (it needs to be clear what asset is part of
   *  operation without access to its definition). Might be used in some custom binary
   *  API responses where performance is essential and trust is a non-issue.
   *  This struct is used primarily to hold asset values in chain objects in places
   *  where use of specific asset is guaranteed.
   *
   * NOTE: all conversions between asset and tiny_asset should become explicit because:
   *  - conversions to asset bring to life more costly object
   *  - conversions from asset can throw exception
   */
  template< uint32_t _SYMBOL >
  struct tiny_asset
  {
    tiny_asset() {}
    explicit tiny_asset( share_type _amount ) : amount( _amount ) {}
    tiny_asset( const asset& val ) { set( val ); } //to be removed
    tiny_asset( asset&& val )      { set( val ); } //to be removed

    asset operator=( const asset& val )  { set( val ); return *this; } //to be removed
    asset operator=( asset&& val )       { set( val ); return *this; } //to be removed

    share_type amount;

    tiny_asset& operator+=( const asset& val ) { check( val ); amount += val.amount; return *this; } //to be removed
    tiny_asset& operator-=( const asset& val ) { check( val ); amount -= val.amount; return *this; } //to be removed

    operator asset() const               { return to_asset(); } //to be removed

    asset to_asset() const { return asset( amount, get_symbol() ); }
    static bool is_compatible( const asset& a ) { return a.symbol.asset_num == _SYMBOL; }
    static asset_symbol_type get_symbol() { return asset_symbol_type::from_asset_num( _SYMBOL ); }

    tiny_asset& operator+=( const tiny_asset& val ) { amount += val.amount; return *this; }
    tiny_asset& operator-=( const tiny_asset& val ) { amount -= val.amount; return *this; }
    tiny_asset& operator*=( const share_type& x ) { amount *= x; return *this; }
    tiny_asset& operator/=( const share_type& x ) { amount /= x; return *this; }
    tiny_asset& operator%=( const share_type& x ) { amount %= x; return *this; }

    friend bool operator==( const tiny_asset& a, const asset& b ) { return a.to_asset() == b; } //to be removed
    friend bool operator==( const asset& a, const tiny_asset& b ) { return a == b.to_asset(); } //to be removed
    friend bool operator!=( const tiny_asset& a, const asset& b ) { return a.to_asset() != b; } //to be removed
    friend bool operator!=( const asset& a, const tiny_asset& b ) { return a != b.to_asset(); } //to be removed
    friend bool operator<( const tiny_asset& a, const asset& b ) { return a.to_asset() < b; } //to be removed
    friend bool operator<( const asset& a, const tiny_asset& b ) { return a < b.to_asset(); } //to be removed
    friend bool operator<=( const tiny_asset& a, const asset& b ) { return a.to_asset() <= b; } //to be removed
    friend bool operator<=( const asset& a, const tiny_asset& b ) { return a <= b.to_asset(); } //to be removed
    friend bool operator>( const tiny_asset& a, const asset& b ) { return a.to_asset() > b; } //to be removed
    friend bool operator>( const asset& a, const tiny_asset& b ) { return a > b.to_asset(); } //to be removed
    friend bool operator>=( const tiny_asset& a, const asset& b ) { return a.to_asset() >= b; } //to be removed
    friend bool operator>=( const asset& a, const tiny_asset& b ) { return a >= b.to_asset(); } //to be removed
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

    friend asset operator* ( const tiny_asset& a, const price& p ) { return a.to_asset() * p; } //to be removed
    friend asset operator* ( const price& p, const tiny_asset& a ) { return a.to_asset() * p; } //to be removed
    friend asset operator+ ( const tiny_asset& a, const asset& b ) { return a.to_asset() + b; } //to be removed
    friend asset operator+ ( const asset& a, const tiny_asset& b ) { return a + b.to_asset(); } //to be removed
    friend asset operator- ( const tiny_asset& a, const asset& b ) { return a.to_asset() - b; } //to be removed
    friend asset operator- ( const asset& a, const tiny_asset& b ) { return a - b.to_asset(); } //to be removed
    friend tiny_asset operator* ( const tiny_asset& a, share_type b ) { return tiny_asset( a.amount * b ); }
    friend tiny_asset operator* ( share_type b, const tiny_asset& a ) { return tiny_asset( a.amount * b ); }
    friend tiny_asset operator/ ( const tiny_asset& a, share_type b ) { return tiny_asset( a.amount / b ); }
    friend tiny_asset operator% ( const tiny_asset& a, share_type b ) { return tiny_asset( a.amount % b ); }

  private:

    void set( const asset& val ) { check( val ); amount = val.amount; }
    void check( const asset& val ) const { FC_ASSERT( val.symbol.asset_num == _SYMBOL ); }
  };


} } // hive::protocol

namespace fc {
    void to_variant( const hive::protocol::asset& var,  fc::variant& vo );
    void from_variant( const fc::variant& var,  hive::protocol::asset& vo );

    void to_variant( const hive::protocol::legacy_asset& a, fc::variant& var );
    void from_variant( const fc::variant& var, hive::protocol::legacy_asset& a );
}

FC_REFLECT( hive::protocol::asset, (amount)(symbol) )
FC_REFLECT( hive::protocol::legacy_asset, (amount)(symbol) )
FC_REFLECT( hive::protocol::price, (base)(quote) )

FC_REFLECT( hive::protocol::HBD_asset, (amount) )
FC_REFLECT( hive::protocol::HIVE_asset, (amount) )
FC_REFLECT( hive::protocol::VEST_asset, (amount) )

