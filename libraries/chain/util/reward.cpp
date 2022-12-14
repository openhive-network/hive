
#include <hive/chain/util/reward.hpp>
#include <hive/chain/util/uint256.hpp>

namespace hive { namespace chain { namespace util {

uint8_t find_msb( const uint128_t& u )
{
  uint64_t x;
  uint8_t places;
  x      = (fc::uint128_low_bits(u) ? fc::uint128_low_bits(u) : 1);
  places = (fc::uint128_high_bits(u) ?   64 : 0);
  x      = (fc::uint128_high_bits(u) ? fc::uint128_high_bits(u) : x);
  return uint8_t( boost::multiprecision::detail::find_msb(x) + places );
}

uint64_t approx_sqrt( const uint128_t& x )
{
  if( x == 0 )
    return 0;

  uint8_t msb_x = find_msb(x);
  uint8_t msb_z = msb_x >> 1;

  uint128_t msb_x_bit = uint128_t(1) << msb_x;
  uint64_t  msb_z_bit = uint64_t (1) << msb_z;

  uint128_t mantissa_mask = msb_x_bit - 1;
  uint128_t mantissa_x = x & mantissa_mask;
  uint64_t mantissa_z_hi = (msb_x & 1) ? msb_z_bit : 0;
  uint64_t mantissa_z_lo = fc::uint128_low_bits(mantissa_x >> (msb_x - msb_z));
  uint64_t mantissa_z = (mantissa_z_hi | mantissa_z_lo) >> 1;
  uint64_t result = msb_z_bit | mantissa_z;

  return result;
}

uint64_t get_rshare_reward( const comment_reward_context& ctx )
{
  try
  {
  FC_ASSERT( ctx.rshares > 0 );
  FC_ASSERT( ctx.total_reward_shares2 > 0 );

  u256 rf(ctx.total_reward_fund_hive.amount.value);
  u256 total_claims = to256( ctx.total_reward_shares2 );

  //idump( (ctx) );

  u256 claim = to256( evaluate_reward_curve( ctx.rshares.value, ctx.reward_curve, ctx.content_constant ) );
  claim = ( claim * ctx.reward_weight ) / HIVE_100_PERCENT;

  u256 payout_u256 = ( rf * claim ) / total_claims;
  FC_ASSERT( payout_u256 <= u256( uint64_t( std::numeric_limits<int64_t>::max() ) ) );
  uint64_t payout = static_cast< uint64_t >( payout_u256 );

  if( is_comment_payout_dust( ctx.current_hive_price, payout ) )
    payout = 0;

  asset max_hive = to_hive( ctx.current_hive_price, ctx.max_hbd );

  payout = std::min( payout, uint64_t( max_hive.amount.value ) );

  return payout;
  } FC_CAPTURE_AND_RETHROW( (ctx) )
}

uint128_t evaluate_reward_curve( const uint128_t& rshares, const protocol::curve_id& curve, const uint128_t& var1 )
{
  uint128_t result = 0;

  switch( curve )
  {
    case protocol::quadratic:
      {
        const uint128_t& content_constant = var1;
        uint128_t rshares_plus_s = rshares + content_constant;
        result = rshares_plus_s * rshares_plus_s - content_constant * content_constant;
      }
      break;
    case protocol::bounded_curation:
      {
        const uint128_t& content_constant = var1;
        uint128_t two_alpha = content_constant * 2;
        result = fc::to_uint128( fc::uint128_low_bits(rshares), 0 ) / ( two_alpha + rshares );
      }
      break;
    case protocol::linear:
      result = rshares;
      break;
    case protocol::square_root:
      result = approx_sqrt( rshares );
      break;
    case protocol::convergent_linear:
      {
        const uint128_t& s = var1;
        result = ( ( rshares + s ) * ( rshares + s ) - s * s ) / ( rshares + 4 * s );
      }
      break;
    case protocol::convergent_square_root:
      {
        const uint128_t& s = var1;
        result = rshares / approx_sqrt( rshares + 2 * s );
      }
      break;
  }

  return result;
}

} } } // hive::chain::util
