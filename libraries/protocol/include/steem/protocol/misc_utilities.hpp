#pragma once

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

} } // hive::utilities


FC_REFLECT_ENUM(
   hive::protocol::curve_id,
   (quadratic)
   (bounded_curation)
   (linear)
   (square_root)
   (convergent_linear)
   (convergent_square_root)
)
