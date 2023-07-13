#include <core/utilities.hpp>

namespace fc
{
  void from_variant( const fc::variant& var, beekeeper::wallet_details& vo )
  {
   from_variant( var, vo.name );
   from_variant( var, vo.unlocked );
  }

  void to_variant( const beekeeper::wallet_details& var, fc::variant& vo )
  {
    to_variant( var.name, vo );
    to_variant( var.unlocked, vo );
  }

  void from_variant( const fc::variant& var, beekeeper::info& vo )
  {
   from_variant( var, vo.now );
   from_variant( var, vo.timeout_time );
  }

  void to_variant( const beekeeper::info& var, fc::variant& vo )
  {
    to_variant( var.now, vo );
    to_variant( var.timeout_time, vo );
  }
} // fc
