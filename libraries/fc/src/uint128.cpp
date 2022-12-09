#include <fc/uint128.hpp>
#include <fc/variant.hpp>
#include <fc/crypto/bigint.hpp>
#include <boost/multiprecision/cpp_int.hpp>

#include <stdexcept>
#include "byteswap.hpp"

namespace fc
{

uint128_t bigint_to_uint128( const bigint& bi )
{
  return string_to_uint128( std::string(bi) ); // TODO: optimize this...
}

uint128_t string_to_uint128( const std::string& s )
{
  uint128_t result = 0;

  if (s.empty())
    return 0;

  // make some reasonable assumptions
  int radix = 10;
  bool minus = false;

  std::string::const_iterator i = s.begin();

  // check for minus sign, i suppose technically this should only apply
  // to base 10, but who says that -0x1 should be invalid?
  if(*i == '-') {
    ++i;
    minus = true;
  }

  // check if there is radix changing prefix (0 or 0x)
  if(i != s.end()) {
    if(*i == '0') {
      radix = 8;
      ++i;
      if(i != s.end()) {
        if(*i == 'x') {
          radix = 16;
          ++i;
        }
      }
    }

    while(i != s.end()) {
      unsigned int n = 0;
      const char ch = *i;

      if(ch >= 'A' && ch <= 'Z') {
        if(((ch - 'A') + 10) < radix) {
          n = (ch - 'A') + 10;
        } else {
          break;
        }
      } else if(ch >= 'a' && ch <= 'z') {
        if(((ch - 'a') + 10) < radix) {
          n = (ch - 'a') + 10;
        } else {
          break;
        }
      } else if(ch >= '0' && ch <= '9') {
        if((ch - '0') < radix) {
          n = (ch - '0');
        } else {
          break;
        }
      } else {
        /* completely invalid character */
        break;
      }

      result *= radix;
      result += n;

      ++i;
    }
  }

  return minus ? -result : result;
}

bigint uint128_to_bigint( const uint128_t& u )
{
  const auto tmp  = to_uint128( bswap_64( uint128_high_bits(u) ), bswap_64( uint128_low_bits(u) ) );
  bigint bi( (char*)&tmp, sizeof(tmp) );
  return bi;
}

void uint128_full_product( const uint128_t& a, const uint128_t& b, uint128_t& result_hi, uint128_t& result_lo )
{
  //   (ah * 2**64 + al) * (bh * 2**64 + bl)
  // = (ah * bh * 2**128 + al * bh * 2**64 + ah * bl * 2**64 + al * bl
  // =  P * 2**128 + (Q + R) * 2**64 + S
  // = Ph * 2**192 + Pl * 2**128
  // + Qh * 2**128 + Ql * 2**64
  // + Rh * 2**128 + Rl * 2**64
  // + Sh * 2**64  + Sl
  //
       
  uint64_t ah = uint128_high_bits(a);
  uint64_t al = uint128_low_bits(a);
  uint64_t bh = uint128_high_bits(b);
  uint64_t bl = uint128_low_bits(b);

  uint128 s = al;
  s *= bl;
  uint128 r = ah;
  r *= bl;
  uint128 q = al;
  q *= bh;
  uint128 p = ah;
  p *= bh;
       
  uint64_t sl = uint128_low_bits(s);
  uint64_t sh = uint128_high_bits(s);
  uint64_t rl = uint128_low_bits(r);
  uint64_t rh = uint128_high_bits(r);
  uint64_t ql = uint128_low_bits(q);
  uint64_t qh = uint128_high_bits(q);
  uint64_t pl = uint128_low_bits(p);
  uint64_t ph = uint128_high_bits(p);

  uint64_t y[4];    // final result
  y[0] = sl;
       
  uint128_t acc = sh;
  acc += ql;
  acc += rl;
  y[1] = uint128_low_bits(acc);
  acc = uint128_high_bits(acc);
  acc += qh;
  acc += rh;
  acc += pl;
  y[2] = uint128_low_bits(acc);
  y[3] = uint128_high_bits(acc) + ph;
       
  result_hi = to_uint128( y[3], y[2] );
  result_lo = to_uint128( y[1], y[0] );
       
  return;
}

static uint8_t _popcount_64( uint64_t x )
{
  static const uint64_t m[] = {
      0x5555555555555555ULL,
      0x3333333333333333ULL,
      0x0F0F0F0F0F0F0F0FULL,
      0x00FF00FF00FF00FFULL,
      0x0000FFFF0000FFFFULL,
      0x00000000FFFFFFFFULL
  };
  // TODO future optimization:  replace slow, portable version
  // with fast, non-portable __builtin_popcountll intrinsic
  // (when available)

  for( int i=0, w=1; i<6; i++, w+=w )
  {
      x = (x & m[i]) + ((x >> w) & m[i]);
  }
  return uint8_t(x);
}

uint8_t uint128_popcount( const uint128_t& u )
{
  return _popcount_64( uint128_low_bits(u) ) + _popcount_64( uint128_high_bits(u) );
}

void to_variant( const uint128_t& var,  variant& vo )  { vo = std::to_string(var);                }
void from_variant( const variant& var,  uint128_t& vo ){ vo = string_to_uint128(var.as_string()); }

} // namespace fc

namespace std
{
  string to_string( const fc::uint128_t& u )
  {
    // based on idea from https://stackoverflow.com/questions/11656241/how-to-print-uint128-t-number-using-gcc/11660651#11660651
    constexpr auto p10 = 10000000000000000000ULL; /* 19 zeroes */
    constexpr auto e10 = 19; // max_digits-1 for uint64_t

    if (u > numeric_limits<uint64_t>::max())
    {
      __uint128_t hi = u / p10;
      uint64_t lo = u % p10;
      string lo_str = to_string(lo);
      return to_string(hi) + string(e10 - lo_str.size(), '0') + lo_str;
    }
    else
    {
      return to_string(static_cast<uint64_t>(u));
    }
  }
} // namespace std
