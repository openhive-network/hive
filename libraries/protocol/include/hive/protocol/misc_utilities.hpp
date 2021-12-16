#pragma once

#include <fc/reflect/reflect.hpp>

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

  legacy_switcher();
  legacy_switcher( bool val );
  ~legacy_switcher();
};

std::string trim_legacy_typename_namespace( const std::string& name );

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
