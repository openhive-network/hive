
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>

#include <boost/multiprecision/integer.hpp>

#include <fc/uint128.hpp>

using fc::uint128_t;

uint64_t maybe_sqrt( uint64_t x )
{
  if( x <= 1 )
    return 0;
  assert( x <= std::numeric_limits< uint64_t >::max()/2 );

  uint8_t n = uint8_t( boost::multiprecision::detail::find_msb(x) );
  uint8_t b = n&1;
  uint64_t y = x + (uint64_t(1) << (n-(1-b)));
  y = y >> ((n >> 1)+b+1);
  return y;
}

int main( int argc, char** argv, char** envp )
{
  uint128_t x = 0;
  while( true )
  {
    uint64_t y = fc::uint128_approx_sqrt(x);
    std::cout << std::to_string(x) << " " << y << std::endl;
    //uint128_t new_x = x + (x >> 10) + 1;
    uint128_t new_x = x + (x / 1000) + 1;
    if( new_x < x )
      break;
    x = new_x;
  }
  return 0;
}
