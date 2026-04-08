#pragma once

#include <hive/protocol/types.hpp>

#include <boost/multiprecision/cpp_int.hpp>
#include <fc/uint128.hpp>

namespace hive {
  typedef boost::multiprecision::uint256_t u256;
  typedef boost::multiprecision::uint512_t u512;
}

namespace hive { namespace chain { namespace util {

inline u256 to256( const fc::uint128& t )
{
  u256 v(fc::uint128_high_bits(t));
  v <<= 64;
  v |= fc::uint128_low_bits(t);
  return v;
}

} } }
