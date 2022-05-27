#pragma once

#include <fc/reflect/reflect.hpp>
#include <fc/variant.hpp>
#include <fc/exception/exception.hpp>

#include <optional>

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

enum class serialization_type : uint8_t { legacy, hf26 };
using transaction_serialization_type  = serialization_type;
using pack_type                       = serialization_type;

struct serialization_mode_controller
{
  public:

    static const transaction_serialization_type default_transaction_serialization = transaction_serialization_type::hf26;
    static const pack_type default_pack = pack_type::legacy;

    struct mode_guard
    {
      const transaction_serialization_type old_transaction_serialization = serialization_mode_controller::default_transaction_serialization;

      mode_guard( transaction_serialization_type val );
      ~mode_guard();
    };

    struct pack_guard
    {
      private:

        const pack_type old_pack = serialization_mode_controller::default_pack;
        fc::optional<pack_type> previous_pack;

      public:

        pack_guard( pack_type new_pack );
        ~pack_guard();
    };

  private:
    /*
      This switch is used for switching of serialization.
    */
    thread_local static transaction_serialization_type transaction_serialization;
    static pack_type pack;
    thread_local static fc::optional<pack_type> current_pack;

  public:

    static bool legacy_enabled();
    static void set_pack( pack_type new_pack );
    static pack_type get_current_pack();
    static pack_type get_another_pack();
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
