#pragma once

#include <hive/protocol/types.hpp>

#include <fc/uint128.hpp>

namespace hive { namespace chain { namespace util {

inline u256 to256( const fc::uint128& t )
{
  u256 v(fc::uint128_high_bits(t));
  v <<= 64;
  v |= fc::uint128_low_bits(t);
  return v;
}

} } }
