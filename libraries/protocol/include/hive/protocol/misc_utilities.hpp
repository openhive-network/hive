#pragma once

#include <fc/reflect/reflect.hpp>
#include <fc/variant.hpp>
#include <fc/exception/exception.hpp>

namespace hive { namespace protocol {

enum curve_id
{
  quadratic,
  bounded_curation,
  linear,
  square_root,
  convergent_linear,
  convergent_square_root
};

enum class transaction_serialization_type : uint8_t { legacy, hf26 };

struct dynamic_serializer
{
  static const transaction_serialization_type default_transaction_serialization = transaction_serialization_type::hf26;
  /*
    This switch is used for switching of serialization.
  */
  thread_local static transaction_serialization_type transaction_serialization;
};

struct legacy_switcher
{
  const transaction_serialization_type old_transaction_serialization = dynamic_serializer::default_transaction_serialization;

  legacy_switcher( transaction_serialization_type val );
  ~legacy_switcher();
};

std::string trim_legacy_typename_namespace( const std::string& name );

template<typename T>
struct serializer_wrapper
{
  T value;
  transaction_serialization_type transaction_serialization = dynamic_serializer::default_transaction_serialization;
};

} } // hive::protocol


FC_REFLECT_ENUM(
  hive::protocol::curve_id,
  (quadratic)
  (bounded_curation)
  (linear)
  (square_root)
  (convergent_linear)
  (convergent_square_root)
)


FC_REFLECT_ENUM(
  hive::protocol::transaction_serialization_type,
  (legacy)
  (hf26)
)

namespace fc {

  using hive::protocol::legacy_switcher;

  template<typename T>
  inline void to_variant( const hive::protocol::serializer_wrapper<T>& a, fc::variant& var )
  {
    try
    {
      legacy_switcher switcher( a.transaction_serialization );
      to_variant( a.value, var );
    } FC_CAPTURE_AND_RETHROW()
  }

  template<typename T>
  inline void from_variant( const fc::variant& var, hive::protocol::serializer_wrapper<T>& a )
  {
    try
    {
      legacy_switcher switcher( hive::protocol::transaction_serialization_type::legacy );
      from_variant( var, a.value );
    } FC_CAPTURE_AND_RETHROW()
  }

} // fc

FC_REFLECT_TEMPLATE( (typename T), hive::protocol::serializer_wrapper<T>, (value) )
