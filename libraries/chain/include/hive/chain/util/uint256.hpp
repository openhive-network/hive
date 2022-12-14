#pragma once

#include <hive/protocol/types.hpp>

#include <fc/uint128.hpp>

namespace hive { namespace chain { namespace util {

inline u256 to256( const fc::uint128& t )
{
  u256 v(t);
  return v;
}

} } }
