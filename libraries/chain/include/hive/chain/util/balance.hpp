#pragma once

#include <hive/protocol/asset.hpp>
#include <exception>

namespace hive { namespace chain {
  class balance;
  template< uint32_t _SYMBOL >
  class tiny_balance;
} }

namespace fc {
  inline void from_variant( const fc::variant&, hive::chain::balance& );
  template< uint32_t _SYMBOL >
  inline void from_variant( const fc::variant&, hive::chain::tiny_balance<_SYMBOL>& );
}

namespace hive { namespace chain {

using hive::protocol::asset;
using hive::protocol::tiny_asset;
using hive::protocol::share_type;
using hive::protocol::asset_symbol_type;

using HBD_balance = tiny_balance< HIVE_ASSET_NUM_HBD >;
using HIVE_balance = tiny_balance< HIVE_ASSET_NUM_HIVE >;
using VEST_balance = tiny_balance< HIVE_ASSET_NUM_VESTS >;

class balance_base;
template< uint32_t _SYMBOL >
class tiny_balance_base;

using HBD_balance_base = tiny_balance_base< HIVE_ASSET_NUM_HBD >;
using HIVE_balance_base = tiny_balance_base< HIVE_ASSET_NUM_HIVE >;
using VEST_balance_base = tiny_balance_base< HIVE_ASSET_NUM_VESTS >;

class temp_balance;
template< uint32_t _SYMBOL >
class temp_tiny_balance;

using temp_HBD_balance = temp_tiny_balance< HIVE_ASSET_NUM_HBD >;
using temp_HIVE_balance = temp_tiny_balance< HIVE_ASSET_NUM_HIVE >;
using temp_VEST_balance = temp_tiny_balance< HIVE_ASSET_NUM_VESTS >;

// classes allowed to contain members of type balance:
class escrow_object;
class savings_withdraw_object;
class limit_order_object;
// classes allowed to contain members of type tiny_balance<>:
class account_object;
class escrow_object;
class convert_request_object;
class collateralized_convert_request_object;
class reward_fund_object;
class dynamic_global_property_object;

/**
 * Common base of balance and temp_balance.
 * Hides underlying asset as private to prevent modifications outside allowed
 * transfer_to/_from() methods. In particular it is to prevent abuse of friend
 * declarations inside balance derived class.
 * Objects of this class cannot be directly instantiated neither inside nor
 * outside chain objects - only their derived classes can in their specific
 * use fields.
 */
class balance_base
{
  friend class fc::reflector< balance_base >; // inspection
  friend class temp_balance; // access to 'funds'
  template< uint32_t >
  friend class tiny_balance_base; // access to 'funds' for cross transfers
  friend void fc::from_variant( const fc::variant&, hive::chain::balance& ); // deserialization

  asset funds;

protected:
  balance_base( asset_symbol_type id = HIVE_SYMBOL ) : funds( 0, id ) {}
  balance_base( balance_base&& b ) : funds( b.funds ) { b.funds.amount = 0; }
  balance_base& operator=( balance_base&& b )
  {
    if( this != &b )
    {
      // NOTE: during application of undo we might be overriding nonzero funds
      funds = b.funds;
      b.funds.amount = 0;
    }
    return *this;
  }
  balance_base( const balance_base& ) = default;
  balance_base& operator=( const balance_base& ) = delete;
  ~balance_base() {}

public:
  const asset& as_asset() const { return funds; }
  operator const asset&() const { return as_asset(); }

  // Tells if balance is zero
  bool is_empty() const { return funds.amount == 0; }
  // Tells if balance is valid (non-negative)
  bool is_valid() const { return funds.amount >= 0; }

  /// Transfer delta funds from source to this (positive delta moves from source to this).
  inline void transfer_from( balance_base& source, const asset& delta );
  /// Transfer delta funds from source to this (positive delta moves from source to this).
  template< uint32_t _SYMBOL >
  inline void transfer_from( tiny_balance_base<_SYMBOL>& source, const tiny_asset<_SYMBOL>& delta );
  // Moves all funds from source to this
  inline void transfer_from( balance_base& source );
  // Moves all funds from source to this
  template< uint32_t _SYMBOL >
  inline void transfer_from( tiny_balance_base< _SYMBOL >& source );

  // Moves delta funds from this to destination (positive delta moves from this to destination).
  inline void transfer_to( balance_base& destination, const asset& delta );
  // Moves delta funds from this to destination (positive delta moves from this to destination).
  template< uint32_t _SYMBOL >
  inline void transfer_to( tiny_balance_base< _SYMBOL >& destination, const tiny_asset< _SYMBOL >& delta );
  // Moves all funds from this to destination.
  inline void transfer_to( balance_base& destination );
  // Moves all funds from this to destination.
  template< uint32_t _SYMBOL >
  inline void transfer_to( tiny_balance_base< _SYMBOL >& destination );
};

/**
 * Common base of tiny_balance<> and temp_tiny_balance<>.
 * Hides underlying asset as private to prevent modifications outside allowed
 * transfer_to/_from() methods. In particular it is to prevent abuse of friend
 * declarations inside tiny_balance<> derived class.
 * Objects of this class cannot be directly instantiated neither inside nor
 * outside chain objects - only their derived classes can in their specific
 * use fields.
 */
template< uint32_t _SYMBOL >
class tiny_balance_base
{
public:
  typedef tiny_asset< _SYMBOL > matching_asset;

private:
  friend class fc::reflector< tiny_balance_base >; // inspection
  template< uint32_t >
  friend class temp_tiny_balance; // access to 'funds'
  friend class balance_base; // access to 'funds' for cross transfers
  friend void fc::from_variant<>( const fc::variant&, hive::chain::tiny_balance< _SYMBOL >& ); // deserialization

  tiny_asset< _SYMBOL > funds;

protected:
  tiny_balance_base() {}
  tiny_balance_base( tiny_balance_base&& b ) : funds( b.funds ) { b.funds.amount = 0; }
  tiny_balance_base& operator=( tiny_balance_base&& b )
  {
    if( this != &b )
    {
      // NOTE: during application of undo we might be overriding nonzero funds
      this->funds = b.funds;
      b.funds.amount = 0;
    }
    return *this;
  }
  tiny_balance_base( const tiny_balance_base& ) = default;
  tiny_balance_base& operator=( const tiny_balance_base& ) = delete;
  ~tiny_balance_base() {}

public:
  const matching_asset& as_asset() const { return funds; }
  operator const matching_asset&() const { return as_asset(); }

  // Tells if balance is zero
  bool is_empty() const { return funds.amount == 0; }
  // Tells if balance is valid (non-negative)
  bool is_valid() const { return funds.amount >= 0; }

  /// Transfer delta funds from source to this (positive delta moves from source to this).
  inline void transfer_from( balance_base& source, const asset& delta );
  /// Transfer delta funds from source to this (positive delta moves from source to this).
  inline void transfer_from( tiny_balance_base& source, const matching_asset& delta );
  // Moves all funds from source to this
  void transfer_from( balance_base& source ) { transfer_from( source, source.as_asset() ); }
  // Moves all funds from source to this
  void transfer_from( tiny_balance_base& source ) { transfer_from( source, source.as_asset() ); }

  // Moves delta funds from this to destination (positive delta moves from this to destination).
  void transfer_to( balance_base& destination, const asset& delta ) { destination.transfer_from( *this, matching_asset( delta ) ); }
  // Moves delta funds from this to destination (positive delta moves from this to destination).
  void transfer_to( tiny_balance_base& destination, const matching_asset& delta ) { destination.transfer_from( *this, delta ); }
  // Moves all funds from this to destination.
  void transfer_to( balance_base& destination ) { destination.transfer_from( *this, as_asset() ); }
  // Moves all funds from this to destination.
  void transfer_to( tiny_balance_base& destination ) { destination.transfer_from( *this, as_asset() ); }
};

inline void balance_base::transfer_from( balance_base& source, const asset& delta )
{
  funds += delta;
  source.funds -= delta;
  FC_ASSERT( is_valid() && source.is_valid() && "transfer asset to balance", "balance underflow" );
}

template< uint32_t _SYMBOL >
inline void balance_base::transfer_from( tiny_balance_base< _SYMBOL >& source, const tiny_asset< _SYMBOL >& delta )
{
  this->funds += delta.to_asset();
  source.funds -= delta;
  FC_ASSERT( this->is_valid() && source.is_valid() && "transfer tiny_asset to balance", "balance underflow" );
}

inline void balance_base::transfer_from( balance_base& source )
{
  transfer_from( source, source.as_asset() );
}

template< uint32_t _SYMBOL >
inline void balance_base::transfer_from( tiny_balance_base< _SYMBOL >& source )
{
  transfer_from( source, source.as_asset() );
}

inline void balance_base::transfer_to( balance_base& destination, const asset& delta )
{
  destination.transfer_from( *this, delta );
}

template< uint32_t _SYMBOL >
inline void balance_base::transfer_to( tiny_balance_base< _SYMBOL >& destination, const tiny_asset< _SYMBOL >& delta )
{
  destination.transfer_from( *this, delta.to_asset() );
}

inline void balance_base::transfer_to( balance_base& destination )
{
  destination.transfer_from( *this, as_asset() );
}

template< uint32_t _SYMBOL >
inline void balance_base::transfer_to( tiny_balance_base< _SYMBOL >& destination )
{
  destination.transfer_from( *this, as_asset() );
}

template< uint32_t _SYMBOL >
inline void tiny_balance_base< _SYMBOL >::transfer_from( balance_base& source, const asset& delta )
{
  this->funds += matching_asset( delta );
  source.funds -= delta;
  FC_ASSERT( this->is_valid() && source.is_valid() && "transfer asset to tiny_balance", "balance underflow" );
}

template< uint32_t _SYMBOL >
inline void tiny_balance_base< _SYMBOL >::transfer_from( tiny_balance_base& source, const matching_asset& delta )
{
  this->funds += delta;
  source.funds -= delta;
  FC_ASSERT( this->is_valid() && source.is_valid() && "transfer tiny_asset to tiny_balance", "balance underflow" );
}

/**
 * Represents actual funds held in a chain object (dynamic symbol version).
 * Unlike plain asset which is also used for accounting records, balance is
 * specifically for fields holding real funds subject to transfer_to/_from() semantics.
 * Note: explicit specification of all classes that can hold balance as members
 * prevents improper use (including adding such members to plugin related objects).
 */
class balance final : public balance_base
{
  friend class escrow_object;
  friend class savings_withdraw_object;
  friend class limit_order_object;

protected:
  balance( asset_symbol_type id = HIVE_SYMBOL ) : balance_base( id ) {} // default constructor is needed for deserialization
  balance( balance&& b ) = default;
  balance& operator=( balance&& b ) = default;
  balance( const balance& ) = default;
  balance& operator=( const balance& ) = delete;
  balance( balance_base&& b ) : balance_base( std::move( b ) ) {}
  ~balance() {}
};

/**
 * Represents actual funds held in a chain object (compile-time symbol version).
 * See also balance.
 */
template< uint32_t _SYMBOL >
class tiny_balance final : public tiny_balance_base< _SYMBOL >
{
  friend class account_object;
  friend class escrow_object;
  friend class convert_request_object;
  friend class collateralized_convert_request_object;
  friend class reward_fund_object;
  friend class dynamic_global_property_object;

protected:
  tiny_balance() {}
  tiny_balance( tiny_balance&& b ) = default;
  tiny_balance& operator=( tiny_balance&& b ) = default;
  tiny_balance( const tiny_balance& ) = default;
  tiny_balance& operator=( const tiny_balance& ) = delete;
  tiny_balance( tiny_balance_base< _SYMBOL >&& b ) : tiny_balance_base< _SYMBOL >( std::move( b ) ) {}
  ~tiny_balance() {}
};

/**
 * Move-only balance for temporary fund tracking in evaluators (dynamic symbol version).
 * Destructor asserts amount == 0 when not in exception unwinding (signals coding bug).
 * During exception-based stack unwinding (undo path), silently allows destruction.
 */
class temp_balance final : public balance_base
{
  temp_balance( const temp_balance& ) = delete;
  temp_balance& operator=( const temp_balance& ) = delete;

public:
  temp_balance( asset_symbol_type id ) : balance_base( id ) {} // no default argument, because it leads to easy to make bugs

  temp_balance( temp_balance&& b ) = default;
  temp_balance& operator=( temp_balance&& b ) = default;

  ~temp_balance() noexcept( false )
  {
    if( std::uncaught_exceptions() == 0 )
      FC_ASSERT( is_empty() && "temp_balance", "Destruction of funds" );
  }

  // can set or clear balance with arbitrary value; temporary - to be removed once balance_object.md task is fully done
  void set_from_asset( const asset& value ) { funds = value; }
};

/**
 * Move-only tiny_balance for temporary fund tracking in evaluators (compile-time symbol version).
 * Same destruction semantics as temp_balance.
 * This also offers creation and destruction of funds - can only be used by dynamic_global_property_object.
 */
template< uint32_t _SYMBOL >
class temp_tiny_balance final : public tiny_balance_base< _SYMBOL >
{
  friend class dynamic_global_property_object; // issuing and burning assets (also during conversions)
  using typename tiny_balance_base< _SYMBOL >::matching_asset;

  // creates asset out of thin air (remember to update total counters in dgpo after call)
  void issue_asset( const matching_asset& a )
  {
    FC_ASSERT( a.amount >= 0 && "issue", "Can't issue negative funds" );
    this->funds += a;
  }
  // burns asset into void (remember to update total counters in dgpo after call)
  void burn_asset( const matching_asset& a )
  {
    FC_ASSERT( a.amount >= 0 && "burn", "Can't burn negative funds" );
    this->funds -= a;
    FC_ASSERT( this->is_valid(), "Balance underflow" );
  }

  temp_tiny_balance( const temp_tiny_balance& ) = delete;
  temp_tiny_balance& operator=( const temp_tiny_balance& ) = delete;

public:
  temp_tiny_balance() {}

  temp_tiny_balance( temp_tiny_balance&& b ) = default;
  temp_tiny_balance& operator=( temp_tiny_balance&& b ) = default;

  ~temp_tiny_balance() noexcept( false )
  {
    if( std::uncaught_exceptions() == 0 )
      FC_ASSERT( this->is_empty() && "temp_tiny_balance", "Destruction of funds" );
  }

  // can set or clear balance with arbitrary value; temporary - to be removed once balance_object.md task is fully done
  void set_from_asset( const matching_asset& value )
  {
    this->funds = value;
  }
};

} } // hive::chain

// Variant conversions - delegate to base asset types
namespace fc {

inline void to_variant( const hive::chain::balance& b, fc::variant& v )
{
  to_variant( b.as_asset(), v );
}

inline void from_variant( const fc::variant& v, hive::chain::balance& b )
{
  from_variant( v, b.funds );
}

template< uint32_t _SYMBOL >
void to_variant( const hive::chain::tiny_balance< _SYMBOL >& tb, fc::variant& v )
{
  to_variant( tb.as_asset(), v );
}

template< uint32_t _SYMBOL >
void from_variant( const fc::variant& v, hive::chain::tiny_balance< _SYMBOL >& tb )
{
  from_variant( v, tb.funds );
}

} // fc

FC_REFLECT( hive::chain::balance_base, (funds) )

FC_REFLECT( hive::chain::HBD_balance_base, (funds) )
FC_REFLECT( hive::chain::HIVE_balance_base, (funds) )
FC_REFLECT( hive::chain::VEST_balance_base, (funds) )

FC_REFLECT_DERIVED( hive::chain::balance, (hive::chain::balance_base), BOOST_PP_SEQ_NIL )

FC_REFLECT_DERIVED( hive::chain::HBD_balance, (hive::chain::HBD_balance_base), BOOST_PP_SEQ_NIL )
FC_REFLECT_DERIVED( hive::chain::HIVE_balance, (hive::chain::HIVE_balance_base), BOOST_PP_SEQ_NIL )
FC_REFLECT_DERIVED( hive::chain::VEST_balance, (hive::chain::VEST_balance_base), BOOST_PP_SEQ_NIL )

FC_REFLECT_DERIVED( hive::chain::temp_balance, (hive::chain::balance_base), BOOST_PP_SEQ_NIL )

FC_REFLECT_DERIVED( hive::chain::temp_HBD_balance, (hive::chain::HBD_balance_base), BOOST_PP_SEQ_NIL )
FC_REFLECT_DERIVED( hive::chain::temp_HIVE_balance, (hive::chain::HIVE_balance_base), BOOST_PP_SEQ_NIL )
FC_REFLECT_DERIVED( hive::chain::temp_VEST_balance, (hive::chain::VEST_balance_base), BOOST_PP_SEQ_NIL )