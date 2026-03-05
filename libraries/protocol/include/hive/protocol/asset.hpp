#pragma once
#include <hive/protocol/hive_specialised_exceptions.hpp>
#include <hive/protocol/types.hpp>
#include <hive/protocol/asset_symbol.hpp>

namespace hive { namespace protocol {

  template< uint32_t _SYMBOL >
  struct tiny_asset;

  using HBD_asset = tiny_asset< HIVE_ASSET_NUM_HBD >;
  using HIVE_asset = tiny_asset< HIVE_ASSET_NUM_HIVE >;
  using VEST_asset = tiny_asset< HIVE_ASSET_NUM_VESTS >;

  template< uint32_t _BASE_SYMBOL, uint32_t _QUOTE_SYMBOL >
  class tiny_price;

  using HBD_price = tiny_price< HIVE_ASSET_NUM_HBD, HIVE_ASSET_NUM_HIVE >;
  using VEST_price = tiny_price< HIVE_ASSET_NUM_VESTS, HIVE_ASSET_NUM_HIVE >;

  struct asset
  {
    share_type        amount; // TODO: make it private
    asset_symbol_type symbol; // TODO: make it private

    asset( share_type a, asset_symbol_type id )
      : amount( a ), symbol( id ) {}

    asset()
      : amount( 0 ), symbol( HIVE_SYMBOL ) {}

    int64_t get_amount() const { return amount.value; }
    asset_symbol_type get_symbol() const { return symbol; }
    asset& operator += ( const asset& o )
    {
      HIVE_PROTOCOL_ASSET_ASSERT( symbol == o.symbol && "operator+=", "asset symbol mismatch +=", ("subject", symbol)("other", o.symbol) );
      amount += o.amount;
      return *this;
    }

    asset& operator -= ( const asset& o )
    {
      HIVE_PROTOCOL_ASSET_ASSERT( symbol == o.symbol && "operator-=", "asset symbol mismatch -=", ("subject", symbol)("other", o.symbol) );
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
      HIVE_PROTOCOL_ASSET_ASSERT( a.symbol == b.symbol && "operator<", "asset symbol mismatch <", ("subject", a.symbol)("other", b.symbol) );
      return a.amount < b.amount;
    }

    friend bool operator <= ( const asset& a, const asset& b )
    {
      HIVE_PROTOCOL_ASSET_ASSERT( a.symbol == b.symbol && "operator<=", "asset symbol mismatch <=", ("subject", a.symbol)("other", b.symbol) );
      return a.amount <= b.amount;
    }

    friend bool operator != ( const asset& a, const asset& b )
    {
      HIVE_PROTOCOL_ASSET_ASSERT( a.symbol == b.symbol && "operator!=", "asset symbol mismatch !=", ("subject", a.symbol)("other", b.symbol) );
      return a.amount != b.amount;
    }

    friend bool operator > ( const asset& a, const asset& b )
    {
      HIVE_PROTOCOL_ASSET_ASSERT( a.symbol == b.symbol && "operator>", "asset symbol mismatch >", ("subject", a.symbol)("other", b.symbol) );
      return a.amount > b.amount;
    }

    friend bool operator >= ( const asset& a, const asset& b )
    {
      HIVE_PROTOCOL_ASSET_ASSERT( a.symbol == b.symbol && "operator>=", "asset symbol mismatch >=", ("subject", a.symbol)("other", b.symbol) );
      return a.amount >= b.amount;
    }

    friend asset operator - ( const asset& a, const asset& b )
    {
      HIVE_PROTOCOL_ASSET_ASSERT( a.symbol == b.symbol && "operator-", "asset symbol mismatch -", ("subject", a.symbol)("other", b.symbol) );
      return asset( a.amount - b.amount, a.symbol );
    }

    friend asset operator + ( const asset& a, const asset& b )
    {
      HIVE_PROTOCOL_ASSET_ASSERT( a.symbol == b.symbol && "operator+", "asset symbol mismatch +", ("subject", a.symbol)("other", b.symbol) );
      return asset( a.amount + b.amount, a.symbol );
    }
  };

  struct legacy_asset
  {
    legacy_asset() {}

    legacy_asset( const asset& a )
    {
      amount = a.get_amount();
      symbol = a.get_symbol();
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
    asset base; //TODO: make it private
    asset quote; //TODO: make it private

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

    static price max( asset_symbol_type base, asset_symbol_type quote )
    {
      return price( asset( max_satoshis(), base ), asset( 1, quote ) );
    }
    static price min( asset_symbol_type base, asset_symbol_type quote )
    {
      return price( asset( 1, base ), asset( max_satoshis(), quote ) );
    }

    price max() const { return price::max( base.get_symbol(), quote.get_symbol() ); }
    price min() const { return price::min( base.get_symbol(), quote.get_symbol() ); }

    bool is_null() const;
    void validate() const;
    const asset& get_base() const { return base; }
    const asset& get_quote() const { return quote; }

  private:
    template< uint32_t _BASE_SYMBOL, uint32_t _QUOTE_SYMBOL >
    friend class tiny_price;
    static share_type max_satoshis();
  }; /// price

  inline price operator~( const price& p ) { return price{ p.get_quote(), p.get_base() }; }

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
      NOTE: there are template versions of that function that are actually used in hive code
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
    share_type amount; // TODO: make it private

    tiny_asset() {}
    explicit tiny_asset( share_type _amount ) : amount( _amount ) {}
    tiny_asset( const tiny_asset& val ) = default;
    explicit tiny_asset( const asset& val ) { set( val ); }
    tiny_asset& operator=( const tiny_asset& val ) = default;

    asset to_asset() const { return asset( amount, get_symbol() ); }
    static bool is_compatible( const asset& a ) { return a.symbol.asset_num == _SYMBOL; }
    int64_t get_amount() const { return amount.value; }
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

  private:
    void set( const asset& val ) { check( val ); amount = val.amount; }
    void check( const asset& val ) const { HIVE_PROTOCOL_ASSET_ASSERT( val.symbol.asset_num == _SYMBOL, "asset symbol mismatch in tiny_asset", ("subject", *this)("incoming", val) ); }
  };

  /** Represents thin version of price.
   *  Just like tiny_asset, this one also MUST NOT be used in operations and its
   *  main purpose is to keep typical prices in form that is both smaller and
   *  typed enabling compiler to check for errors.
   */
  template< uint32_t _BASE_SYMBOL, uint32_t _QUOTE_SYMBOL >
  class tiny_price
  {
    static_assert( _BASE_SYMBOL != _QUOTE_SYMBOL );

    tiny_asset< _BASE_SYMBOL > base;
    tiny_asset< _QUOTE_SYMBOL > quote;

  public:
    tiny_price() {}
    tiny_price( share_type _base_amount, share_type _quote_amount )
      : base( _base_amount ), quote( _quote_amount ) { validate(); }
    tiny_price( const tiny_asset< _BASE_SYMBOL >& _base, const tiny_asset< _QUOTE_SYMBOL >& _quote )
      : base( _base ), quote( _quote ) { validate(); }
    tiny_price( const tiny_price& val ) = default;
    explicit tiny_price( const price& p )
    {
      if( is_compatible( p ) )
      {
        base = decltype( base )( p.base );
        quote = decltype( quote )( p.quote );
      }
      else // normalize price
      {
        base = decltype( base )( p.quote );
        quote = decltype( quote )( p.base );
      }
      validate();
    }

    tiny_price& operator=( const tiny_price& val ) = default;

    price to_price() const { return price( base.to_asset(), quote.to_asset() ); }
    static bool is_compatible( const price& p ) { return decltype( base )::is_compatible( p.base ) && decltype( quote )::is_compatible( p.quote ); }
    static bool is_assignable( const price& p ) { return is_compatible( p ) || ( decltype( base )::is_compatible( p.quote ) && decltype( quote )::is_compatible( p.base ) ); }
    const decltype( base )& get_base() const { return base; }
    const decltype( quote )& get_quote() const { return quote; }
    static asset_symbol_type get_base_symbol() { return decltype( base )::get_symbol(); }
    static asset_symbol_type get_quote_symbol() { return decltype( quote )::get_symbol(); }

    static tiny_price max() { return tiny_price( price::max_satoshis(), 1); }
    static tiny_price min() { return tiny_price( 1, price::max_satoshis() ); }

    bool is_null() const { return base.get_amount() == 0 || quote.get_amount() == 0; }
    void validate() const
    {
      try
      {
        HIVE_PROTOCOL_ASSET_ASSERT( base.get_amount() > 0, "tiny_price base amount must be > 0", ("subject", base) );
        HIVE_PROTOCOL_ASSET_ASSERT( quote.get_amount() > 0, "tiny_price quote amount must be > 0", ("subject", quote) );
      } FC_CAPTURE_AND_RETHROW( ( base )( quote ) )
    }

    friend class fc::reflector<tiny_price>;
  }; // tiny_price<>

  template< uint32_t _BASE_SYMBOL, uint32_t _QUOTE_SYMBOL >
  bool operator==( const tiny_price< _BASE_SYMBOL, _QUOTE_SYMBOL >& a,
                   const tiny_price< _BASE_SYMBOL, _QUOTE_SYMBOL >& b )
  {
    const uint128_t amult = uint128_t( b.get_quote().get_amount() ) * a.get_base().get_amount();
    const uint128_t bmult = uint128_t( a.get_quote().get_amount() ) * b.get_base().get_amount();
    return amult == bmult;
  }

  template< uint32_t _BASE_SYMBOL, uint32_t _QUOTE_SYMBOL >
  bool operator!=( const tiny_price< _BASE_SYMBOL, _QUOTE_SYMBOL >& a,
                   const tiny_price< _BASE_SYMBOL, _QUOTE_SYMBOL >& b )
  {
    return !( a == b );
  }

  template< uint32_t _BASE_SYMBOL, uint32_t _QUOTE_SYMBOL >
  bool operator<( const tiny_price< _BASE_SYMBOL, _QUOTE_SYMBOL >& a,
                  const tiny_price< _BASE_SYMBOL, _QUOTE_SYMBOL >& b )
  {
    const uint128_t amult = uint128_t( b.get_quote().get_amount() ) * a.get_base().get_amount();
    const uint128_t bmult = uint128_t( a.get_quote().get_amount() ) * b.get_base().get_amount();
    return amult < bmult;
  }

  template< uint32_t _BASE_SYMBOL, uint32_t _QUOTE_SYMBOL >
  bool operator<=( const tiny_price< _BASE_SYMBOL, _QUOTE_SYMBOL >& a,
                   const tiny_price< _BASE_SYMBOL, _QUOTE_SYMBOL >& b )
  {
    return !( b < a );
  }

  template< uint32_t _BASE_SYMBOL, uint32_t _QUOTE_SYMBOL >
  bool operator>( const tiny_price< _BASE_SYMBOL, _QUOTE_SYMBOL >& a,
                  const tiny_price< _BASE_SYMBOL, _QUOTE_SYMBOL >& b )
  {
    return b < a;
  }

  template< uint32_t _BASE_SYMBOL, uint32_t _QUOTE_SYMBOL >
  bool operator>=( const tiny_price< _BASE_SYMBOL, _QUOTE_SYMBOL >& a,
                   const tiny_price< _BASE_SYMBOL, _QUOTE_SYMBOL >& b )
  {
    return !( a < b );
  }

  template< uint32_t _BASE_SYMBOL, uint32_t _QUOTE_SYMBOL >
  tiny_asset< _BASE_SYMBOL > operator* ( const tiny_asset< _QUOTE_SYMBOL >& a, const tiny_price< _BASE_SYMBOL, _QUOTE_SYMBOL >& p )
  {
    HIVE_PROTOCOL_ASSET_ASSERT( p.get_quote().get_amount() > 0, "tiny_price quote amount must be > 0 for multiplication", ("subject", p.get_quote()) );
    bool is_negative = a.get_amount() < 0;
    uint128_t result( is_negative ? -a.get_amount() : a.get_amount() );
    result = ( result * p.get_base().get_amount() ) / p.get_quote().get_amount();
    return tiny_asset< _BASE_SYMBOL >( is_negative ? -fc::uint128_to_uint64( result ) : fc::uint128_to_uint64( result ) );
  }

  template< uint32_t _BASE_SYMBOL, uint32_t _QUOTE_SYMBOL >
  tiny_asset< _QUOTE_SYMBOL > operator* ( const tiny_asset< _BASE_SYMBOL >& a, const tiny_price< _BASE_SYMBOL, _QUOTE_SYMBOL >& p )
  {
    HIVE_PROTOCOL_ASSET_ASSERT( p.get_base().get_amount() > 0, "tiny_price base amount must be > 0 for multiplication", ("subject", p.get_base()) );
    bool is_negative = a.get_amount() < 0;
    uint128_t result( is_negative ? -a.get_amount() : a.get_amount() );
    result = ( result * p.get_quote().get_amount() ) / p.get_base().get_amount();
    return tiny_asset< _QUOTE_SYMBOL >( is_negative ? -fc::uint128_to_uint64( result ) : fc::uint128_to_uint64( result ) );
  }

  // Template version of multiply_with_fee for tiny_asset/tiny_price. Overload: tiny_asset<QUOTE> * tiny_price<BASE,QUOTE> -> tiny_asset<BASE>
  template< uint32_t _APPLY_FEE_TO, uint32_t _BASE_SYMBOL, uint32_t _QUOTE_SYMBOL >
  tiny_asset< _BASE_SYMBOL > multiply_with_fee( const tiny_asset< _QUOTE_SYMBOL >& a,
    const tiny_price< _BASE_SYMBOL, _QUOTE_SYMBOL >& p, uint16_t fee );

  // Template version of multiply_with_fee for tiny_asset/tiny_price. Overload: tiny_asset<BASE> * tiny_price<BASE,QUOTE> -> tiny_asset<QUOTE>
  template< uint32_t _APPLY_FEE_TO, uint32_t _BASE_SYMBOL, uint32_t _QUOTE_SYMBOL >
  tiny_asset< _QUOTE_SYMBOL > multiply_with_fee( const tiny_asset< _BASE_SYMBOL >& a,
    const tiny_price< _BASE_SYMBOL, _QUOTE_SYMBOL >& p, uint16_t fee );

} } // hive::protocol

namespace fc {

  void to_variant( const hive::protocol::asset& var,  fc::variant& vo );
  void from_variant( const fc::variant& var,  hive::protocol::asset& vo );

  void to_variant( const hive::protocol::legacy_asset& a, fc::variant& var );
  void from_variant( const fc::variant& var, hive::protocol::legacy_asset& a );

  template < uint32_t _SYMBOL >
  void to_variant( const hive::protocol::tiny_asset<_SYMBOL>& ta, fc::variant& var )
  {
    if( hive::protocol::serialization_mode_controller::legacy_enabled() )
      to_variant( hive::protocol::legacy_asset( ta.to_asset() ), var );
    else
      to_variant( ta.to_asset(), var );
  }
}

FC_REFLECT( hive::protocol::asset, (amount)(symbol) )
FC_REFLECT( hive::protocol::legacy_asset, (amount)(symbol) )
FC_REFLECT( hive::protocol::price, (base)(quote) )

FC_REFLECT( hive::protocol::HBD_asset, (amount) )
FC_REFLECT( hive::protocol::HIVE_asset, (amount) )
FC_REFLECT( hive::protocol::VEST_asset, (amount) )

FC_REFLECT( hive::protocol::HBD_price, (base)(quote) )
FC_REFLECT( hive::protocol::VEST_price, (base)(quote) )
