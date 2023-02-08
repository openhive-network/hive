#pragma once

#if !defined(__SIZEOF_INT128__)
#pragma error("Missing __uint128_t support!")
#endif

#ifdef _MSC_VER
  #pragma warning (push)
  #pragma warning (disable : 4244)
#endif //// _MSC_VER

#include <fc/exception/exception.hpp>

#include <string>

namespace fc
{
using uint128_t = __uint128_t;
using uint128 = uint128_t;

class bigint;

size_t city_hash_size_t(const char *buf, size_t len);

inline uint128_t to_uint128( const uint64_t& hi, const uint64_t& lo ) { return (static_cast<uint128_t>(hi) << 64) | lo; }

uint128_t bigint_to_uint128( const fc::bigint& bi );

uint128_t string_to_uint128( const std::string& s );

fc::bigint uint128_to_bigint( const uint128_t& u );

inline std::size_t uint128_hash_value( const uint128_t& v ) { return city_hash_size_t((const char*)&v, sizeof(v)); }

inline uint32_t uint128_to_integer( const uint128_t& u )
{
  FC_ASSERT( u <= std::numeric_limits<uint32_t>::max() );
  return static_cast<uint32_t>(u);
}

inline uint64_t uint128_to_uint64( const uint128_t& u )
{
  FC_ASSERT( u <= std::numeric_limits<uint64_t>::max() );
  return static_cast<uint64_t>(u);
}

inline uint32_t uint128_low_32_bits( const uint128_t& u ) { return static_cast<uint32_t>(u); }
inline uint64_t uint128_low_bits( const uint128_t& u ) { return static_cast<uint64_t>(u); }
inline uint64_t uint128_high_bits( const uint128_t& u ) { return static_cast<uint64_t>(u >> 64); }

inline int64_t uint128_to_int64( const uint128_t& u )
{
  FC_ASSERT( u <= std::numeric_limits<int64_t>::max() );
  return static_cast<int64_t>(u);
}

inline uint128_t uint128_max_value()
{
  constexpr uint64_t max64 = std::numeric_limits<uint64_t>::max();
  return (static_cast<uint128_t>(max64) << 64) | max64;
}

void uint128_full_product( const uint128_t& a, const uint128_t& b, uint128_t& result_hi, uint128_t& result_lo );

uint8_t uint128_popcount( const uint128_t& u );

class variant;

void to_variant( const uint128_t& var,  variant& vo );
void from_variant( const variant& var,  uint128_t& vo );

namespace raw
{
  template<typename Stream>
  inline void pack( Stream& s, const uint128_t& u ) { s.write( (char*)&u, sizeof(u) ); }
  template<typename Stream>
  inline void unpack( Stream& s, uint128_t& u, uint32_t ) { s.read( (char*)&u, sizeof(u) ); }
}

template<> struct get_typename<uint128_t> { static const char* name() { return "uint128_t"; } };

} // namespace fc

namespace std
{
  string to_string( const fc::uint128_t& u );
}


#ifdef _MSC_VER
  #pragma warning (pop)
#endif ///_MSC_VER
