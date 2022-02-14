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

struct dynamic_serializer
{
  /*
    This switch is used for switching of serialization.
  */
  thread_local static bool legacy_enabled;
};

struct legacy_switcher
{
  const bool old_legacy_enabled = false;

  legacy_switcher( bool val );
  ~legacy_switcher();
};

std::string trim_legacy_typename_namespace( const std::string& name );

template<typename T>
struct serializer_wrapper
{
  T value;
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

namespace fc {

  using hive::protocol::legacy_switcher;

  template<typename T>
  inline void to_variant( const hive::protocol::serializer_wrapper<T>& a, fc::variant& var )
  {
    try
    {
      legacy_switcher switcher( true );
      to_variant( a.value, var );
    } FC_CAPTURE_AND_RETHROW()
  }

  template<typename T>
  inline void from_variant( const fc::variant& var, hive::protocol::serializer_wrapper<T>& a )
  {
    try
    {
      legacy_switcher switcher( true );
      from_variant( var, a.value );
    } FC_CAPTURE_AND_RETHROW()
  }

} // fc

FC_REFLECT_TEMPLATE( (typename T), hive::protocol::serializer_wrapper<T>, (value) )
