#pragma once

#include <hive/protocol/misc_utilities.hpp>

#include <fc/variant.hpp>

namespace hive { namespace wallet {

struct wallet_transaction_serialization
{
  static hive::protocol::transaction_serialization_type transaction_serialization;
};

template<typename T>
struct wallet_serializer_wrapper
{
  T value;
};

} } //hive::wallet

namespace fc {

  using mode_guard = hive::protocol::serialization_mode_controller::mode_guard;

  template<typename T>
  inline void to_variant( const hive::wallet::wallet_serializer_wrapper<T>& a, fc::variant& var )
  {
    try
    {
      mode_guard guard( hive::wallet::wallet_transaction_serialization::transaction_serialization );
      to_variant( a.value, var );
    } FC_CAPTURE_AND_RETHROW()
  }

  template<typename T>
  inline void from_variant( const fc::variant& var, hive::wallet::wallet_serializer_wrapper<T>& a )
  {
    try
    {
      mode_guard guard( hive::wallet::wallet_transaction_serialization::transaction_serialization );
      from_variant( var, a.value );
    } FC_CAPTURE_AND_RETHROW()
  }

} // fc

FC_REFLECT_TEMPLATE( (typename T), hive::wallet::wallet_serializer_wrapper<T>, (value) )
