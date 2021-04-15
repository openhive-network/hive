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
  static bool legacy_enabled;
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
