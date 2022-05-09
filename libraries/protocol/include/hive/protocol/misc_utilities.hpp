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

struct serialization_mode_controller
{
  public:

    static const transaction_serialization_type default_transaction_serialization = transaction_serialization_type::hf26;

    struct mode_guard
    {
      const transaction_serialization_type old_transaction_serialization = serialization_mode_controller::default_transaction_serialization;

      mode_guard( transaction_serialization_type val );
      ~mode_guard();
    };

  private:
    /*
      This switch is used for switching of serialization.
    */
    thread_local static transaction_serialization_type transaction_serialization;

  public:

    static bool legacy_enabled()
    {
      return transaction_serialization == hive::protocol::transaction_serialization_type::legacy;
    }

};

std::string trim_legacy_typename_namespace( const std::string& name );

template<typename T>
struct serializer_wrapper
{
  T value;
  transaction_serialization_type transaction_serialization = serialization_mode_controller::default_transaction_serialization;
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

  using mode_guard = hive::protocol::serialization_mode_controller::mode_guard;

  template<typename T>
  inline void to_variant( const hive::protocol::serializer_wrapper<T>& a, fc::variant& var )
  {
    try
    {
      mode_guard guard( a.transaction_serialization );
      to_variant( a.value, var );
    } FC_CAPTURE_AND_RETHROW()
  }

  template<typename T>
  inline void from_variant( const fc::variant& var, hive::protocol::serializer_wrapper<T>& a )
  {
    try
    {
      mode_guard guard( a.transaction_serialization );
      from_variant( var, a.value );
    } FC_CAPTURE_AND_RETHROW()
  }

} // fc

FC_REFLECT_TEMPLATE( (typename T), hive::protocol::serializer_wrapper<T>, (value) )
