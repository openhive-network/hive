#include <hive/protocol/asset.hpp>
#include <hive/protocol/misc_utilities.hpp>

#include <fc/io/json.hpp>

#include <boost/rational.hpp>
#include <boost/multiprecision/cpp_int.hpp>

#define ASSET_AMOUNT_KEY     "amount"
#define ASSET_PRECISION_KEY  "precision"
#define ASSET_NAI_KEY        "nai"

/*

The bounds on asset serialization are as follows:

index : field
0     : decimals
1..6  : symbol
  7  : \0
*/

namespace hive { namespace protocol {

std::string asset_symbol_type::to_string()const
{
  return fc::json::to_string( fc::variant( *this ) );
}

asset_symbol_type asset_symbol_type::from_string( const std::string& str )
{
  return fc::json::from_string( str ).as< asset_symbol_type >();
}

void asset_symbol_type::to_nai_string( char* buf )const
{
  static_assert( HIVE_ASSET_SYMBOL_NAI_STRING_LENGTH >= 12, "This code will overflow a short buffer" );
  uint32_t x = to_nai();
  buf[11] = '\0';
  buf[10] = ((x%10)+'0');  x /= 10;
  buf[ 9] = ((x%10)+'0');  x /= 10;
  buf[ 8] = ((x%10)+'0');  x /= 10;
  buf[ 7] = ((x%10)+'0');  x /= 10;
  buf[ 6] = ((x%10)+'0');  x /= 10;
  buf[ 5] = ((x%10)+'0');  x /= 10;
  buf[ 4] = ((x%10)+'0');  x /= 10;
  buf[ 3] = ((x%10)+'0');  x /= 10;
  buf[ 2] = ((x   )+'0');
  buf[ 1] = '@';
  buf[ 0] = '@';
}

asset_symbol_type asset_symbol_type::from_nai_string( const char* p, uint8_t decimal_places )
{
  try
  {
    FC_ASSERT( p != nullptr, "NAI string cannot be a null" );
    FC_ASSERT( std::strlen( p ) == HIVE_ASSET_SYMBOL_NAI_STRING_LENGTH - 1, "Incorrect NAI string length" );
    FC_ASSERT( p[0] == '@' && p[1] == '@', "Invalid NAI string prefix" );
    uint32_t nai = boost::lexical_cast< uint32_t >( p + 2 );
    return asset_symbol_type::from_nai( nai, decimal_places );
  } FC_CAPTURE_AND_RETHROW();
}

// Highly optimized implementation of Damm algorithm
// https://en.wikipedia.org/wiki/Damm_algorithm
uint8_t asset_symbol_type::damm_checksum_8digit(uint32_t value)
{
  FC_ASSERT( value < 100000000 );

  const uint8_t t[] = {
      0, 30, 10, 70, 50, 90, 80, 60, 40, 20,
    70,  0, 90, 20, 10, 50, 40, 80, 60, 30,
    40, 20,  0, 60, 80, 70, 10, 30, 50, 90,
    10, 70, 50,  0, 90, 80, 30, 40, 20, 60,
    60, 10, 20, 30,  0, 40, 50, 90, 70, 80,
    30, 60, 70, 40, 20,  0, 90, 50, 80, 10,
    50, 80, 60, 90, 70, 20,  0, 10, 30, 40,
    80, 90, 40, 50, 30, 60, 20,  0, 10, 70,
    90, 40, 30, 80, 60, 10, 70, 20,  0, 50,
    20, 50, 80, 10, 40, 30, 60, 70, 90, 0
  };

  uint32_t q0 = value/10;
  uint32_t d0 = value%10;
  uint32_t q1 = q0/10;
  uint32_t d1 = q0%10;
  uint32_t q2 = q1/10;
  uint32_t d2 = q1%10;
  uint32_t q3 = q2/10;
  uint32_t d3 = q2%10;
  uint32_t q4 = q3/10;
  uint32_t d4 = q3%10;
  uint32_t q5 = q4/10;
  uint32_t d5 = q4%10;
  uint32_t d6 = q5%10;
  uint32_t d7 = q5/10;

  uint8_t x = t[d7];
  x = t[x+d6];
  x = t[x+d5];
  x = t[x+d4];
  x = t[x+d3];
  x = t[x+d2];
  x = t[x+d1];
  x = t[x+d0];
  return x/10;
}

uint32_t asset_symbol_type::asset_num_from_nai( uint32_t nai, uint8_t decimal_places )
{
  // Can be replaced with some clever bitshifting
  uint32_t nai_check_digit = nai % 10;
  uint32_t nai_data_digits = nai / 10;

  FC_ASSERT( (nai_data_digits >= SMT_MIN_NAI) & (nai_data_digits <= SMT_MAX_NAI), "NAI out of range" );
  FC_ASSERT( nai_check_digit == damm_checksum_8digit(nai_data_digits), "Invalid check digit" );

  switch( nai_data_digits )
  {
    case HIVE_NAI_HIVE:
      FC_ASSERT( decimal_places == HIVE_PRECISION_HIVE );
      return HIVE_ASSET_NUM_HIVE;
    case HIVE_NAI_HBD:
      FC_ASSERT( decimal_places == HIVE_PRECISION_HBD );
      return HIVE_ASSET_NUM_HBD;
    case HIVE_NAI_VESTS:
      FC_ASSERT( decimal_places == HIVE_PRECISION_VESTS );
      return HIVE_ASSET_NUM_VESTS;
    default:
      FC_ASSERT( decimal_places <= HIVE_ASSET_MAX_DECIMALS, "Invalid decimal_places" );
      return (nai_data_digits << HIVE_NAI_SHIFT) | SMT_ASSET_NUM_CONTROL_MASK | decimal_places;
  }
}

uint32_t asset_symbol_type::to_nai()const
{
  uint32_t nai_data_digits = 0;

  // Can be replaced with some clever bitshifting
  switch( asset_num )
  {
    case HIVE_ASSET_NUM_HIVE:
      nai_data_digits = HIVE_NAI_HIVE;
      break;
    case HIVE_ASSET_NUM_HBD:
      nai_data_digits = HIVE_NAI_HBD;
      break;
    case HIVE_ASSET_NUM_VESTS:
      nai_data_digits = HIVE_NAI_VESTS;
      break;
    default:
      FC_ASSERT( space() == smt_nai_space );
      nai_data_digits = (asset_num >> HIVE_NAI_SHIFT);
  }

  uint32_t nai_check_digit = damm_checksum_8digit(nai_data_digits);
  return nai_data_digits * 10 + nai_check_digit;
}

bool asset_symbol_type::is_vesting() const
{
  switch( space() )
  {
    case legacy_space:
    {
      switch( asset_num )
      {
        case HIVE_ASSET_NUM_HIVE:
          return false;
        case HIVE_ASSET_NUM_HBD:
          // HBD is certainly liquid.
          return false;
        case HIVE_ASSET_NUM_VESTS:
          return true;
        default:
          FC_ASSERT( false, "Unknown asset symbol" );
      }
    }
    case smt_nai_space:
      // 6th bit of asset_num is used as vesting/liquid variant indicator.
      return asset_num & SMT_ASSET_NUM_VESTING_MASK;
    default:
      FC_ASSERT( false, "Unknown asset symbol" );
  }
}

asset_symbol_type asset_symbol_type::get_paired_symbol() const
{
  switch( space() )
  {
    case legacy_space:
    {
      switch( asset_num )
      {
        case HIVE_ASSET_NUM_HIVE:
          return from_asset_num( HIVE_ASSET_NUM_VESTS );
        case HIVE_ASSET_NUM_HBD:
          return *this;
        case HIVE_ASSET_NUM_VESTS:
          return from_asset_num( HIVE_ASSET_NUM_HIVE );
        default:
          FC_ASSERT( false, "Unknown asset symbol" );
      }
    }
    case smt_nai_space:
      {
      // Toggle 6th bit of this asset_num.
      auto paired_asset_num = asset_num ^ ( SMT_ASSET_NUM_VESTING_MASK );
      return from_asset_num( paired_asset_num );
      }
    default:
      FC_ASSERT( false, "Unknown asset symbol" );
  }
}

asset_symbol_type::asset_symbol_space asset_symbol_type::space()const
{
  asset_symbol_type::asset_symbol_space s = legacy_space;
  switch( asset_num )
  {
    case HIVE_ASSET_NUM_HIVE:
    case HIVE_ASSET_NUM_HBD:
    case HIVE_ASSET_NUM_VESTS:
      s = legacy_space;
      break;
    default:
      s = smt_nai_space;
  }
  return s;
}

void asset_symbol_type::validate()const
{
  switch( asset_num )
  {
    case HIVE_ASSET_NUM_HIVE:
    case HIVE_ASSET_NUM_HBD:
    case HIVE_ASSET_NUM_VESTS:
      break;
    default:
    {
      uint32_t nai_data_digits = (asset_num >> HIVE_NAI_SHIFT);
      uint32_t nai_1bit = (asset_num & SMT_ASSET_NUM_CONTROL_MASK);
      uint32_t nai_decimal_places = (asset_num & SMT_ASSET_NUM_PRECISION_MASK);
      FC_ASSERT( (nai_data_digits >= SMT_MIN_NAI) &
              (nai_data_digits <= SMT_MAX_NAI) &
              (nai_1bit == SMT_ASSET_NUM_CONTROL_MASK) &
              (nai_decimal_places <= HIVE_ASSET_MAX_DECIMALS),
              "Cannot determine space for asset ${n}", ("n", asset_num) );
    }
  }
  // this assert is duplicated by above code in all cases
  // FC_ASSERT( decimals() <= HIVE_ASSET_MAX_DECIMALS );
}

void asset::validate()const
{
  symbol.validate();
  FC_ASSERT( amount.value >= 0 );
  FC_ASSERT( amount.value <= HIVE_MAX_SATOSHIS );
}

#define BQ(a) \
  std::tie( a.base.symbol, a.quote.symbol )

#define DEFINE_PRICE_COMPARISON_OPERATOR( op ) \
bool operator op ( const price& a, const price& b ) \
{ \
  if( BQ(a) != BQ(b) ) \
    return BQ(a) op BQ(b); \
  \
  const uint128_t amult = uint128_t( b.quote.amount.value ) * a.base.amount.value; \
  const uint128_t bmult = uint128_t( a.quote.amount.value ) * b.base.amount.value; \
  \
  return amult op bmult;  \
}

DEFINE_PRICE_COMPARISON_OPERATOR( == )
DEFINE_PRICE_COMPARISON_OPERATOR( != )
DEFINE_PRICE_COMPARISON_OPERATOR( <  )
DEFINE_PRICE_COMPARISON_OPERATOR( <= )
DEFINE_PRICE_COMPARISON_OPERATOR( >  )
DEFINE_PRICE_COMPARISON_OPERATOR( >= )

asset operator * ( const asset& a, const price& b )
{
  bool is_negative = a.amount.value < 0;
  uint128_t result( is_negative ? -a.amount.value : a.amount.value );
  if( a.symbol == b.base.symbol )
  {
    result = ( result * b.quote.amount.value ) / b.base.amount.value;
    return asset( is_negative ? -result.to_uint64() : result.to_uint64(), b.quote.symbol );
  }
  else
  {
    FC_ASSERT( a.symbol == b.quote.symbol, "invalid ${asset} * ${price}", ( "asset", a )( "price", b ) );
    result = ( result * b.base.amount.value ) / b.quote.amount.value;
    return asset( is_negative ? -result.to_uint64() : result.to_uint64(), b.base.symbol );
  }
}

price price::max( asset_symbol_type base, asset_symbol_type quote )
{
  return price( asset( share_type(HIVE_MAX_SATOSHIS), base ), asset( share_type(1), quote) );
}

price price::min( asset_symbol_type base, asset_symbol_type quote )
{
  return price( asset( 1, base ), asset( HIVE_MAX_SATOSHIS, quote) );
}

bool price::is_null() const { return *this == price(); }

void price::validate() const
{ try {
  FC_ASSERT( base.amount > share_type(0) );
  FC_ASSERT( quote.amount > share_type(0) );
  FC_ASSERT( base.symbol != quote.symbol );
} FC_CAPTURE_AND_RETHROW( (base)(quote) ) }

asset multiply_with_fee( const asset& a, const price& p, uint16_t fee, asset_symbol_type apply_fee_to )
{
  bool is_negative = a.amount.value < 0;
  uint128_t result( is_negative ? -a.amount.value : a.amount.value );
  uint16_t scale_b = HIVE_100_PERCENT, scale_q = HIVE_100_PERCENT;
  if( apply_fee_to == p.base.symbol )
  {
    scale_b += fee;
  }
  else
  {
    FC_ASSERT( apply_fee_to == p.quote.symbol, "Invalid fee symbol ${at} for price ${p}", ( "at", apply_fee_to )( "p", p ) );
    scale_q += fee;
  }

  if( a.symbol == p.base.symbol )
  {
    result = ( result * p.quote.amount.value * scale_q ) / ( uint128_t( p.base.amount.value ) * scale_b );
    return asset( is_negative ? -result.to_uint64() : result.to_uint64(), p.quote.symbol );
  }
  else 
  {
    FC_ASSERT( a.symbol == p.quote.symbol, "invalid ${asset} * ${price}", ( "asset", a )( "price", p ) );
    result = ( result * p.base.amount.value * scale_b ) / ( uint128_t ( p.quote.amount.value ) * scale_q );
    return asset( is_negative ? -result.to_uint64() : result.to_uint64(), p.base.symbol );
  }
}

uint32_t string_to_asset_num( const char* p, uint8_t decimals )
{
  while( true )
  {
    switch( *p )
    {
      case ' ':  case '\t':  case '\n':  case '\r':
        ++p;
        continue;
      default:
        break;
    }
    break;
  }

  // [A-Z]
  uint32_t asset_num = 0;
  switch( *p )
  {
    case 'A': case 'B': case 'C': case 'D': case 'E': case 'F': case 'G': case 'H': case 'I':
    case 'J': case 'K': case 'L': case 'M': case 'N': case 'O': case 'P': case 'Q': case 'R':
    case 'S': case 'T': case 'U': case 'V': case 'W': case 'X': case 'Y': case 'Z':
    {
      // [A-Z]{1,6}
      int shift = 0;
      uint64_t name_u64 = 0;
      while( true )
      {
        if( ((*p) >= 'A') && ((*p) <= 'Z') )
        {
          FC_ASSERT( shift < 64, "Cannot parse asset symbol" );
          name_u64 |= uint64_t(*p) << shift;
          shift += 8;
          ++p;
          continue;
        }
        break;
      }
      switch( name_u64 )
      {
#ifndef IS_TEST_NET
        /// Has same value as HIVE_SYMBOL_U64
        case OBSOLETE_SYMBOL_U64:
#endif /// IS_TEST_NET
        case HIVE_SYMBOL_U64:
          FC_ASSERT( decimals == 3, "Incorrect decimal places" );
          asset_num = HIVE_ASSET_NUM_HIVE;
          break;
#ifndef IS_TEST_NET
        /// Has same value as HBD_SYMBOL_U64
        case OBD_SYMBOL_U64:
#endif ///IS_TEST_NET
        case HBD_SYMBOL_U64:
          FC_ASSERT( decimals == 3, "Incorrect decimal places" );
          asset_num = HIVE_ASSET_NUM_HBD;
          break;
        case VESTS_SYMBOL_U64:
          FC_ASSERT( decimals == 6, "Incorrect decimal places" );
          asset_num = HIVE_ASSET_NUM_VESTS;
          break;
        default:
          FC_ASSERT( false, "Cannot parse asset symbol" );
      }
      break;
    }
    default:
      FC_ASSERT( false, "Cannot parse asset symbol" );
  }

  // \s*\0
  while( true )
  {
    switch( *p )
    {
      case ' ':  case '\t':  case '\n':  case '\r':
        ++p;
        continue;
      case '\0':
        break;
      default:
        FC_ASSERT( false, "Cannot parse asset symbol" );
    }
    break;
  }

  return asset_num;
}

std::string asset_num_to_string( uint32_t asset_num )
{
  switch( asset_num )
  {
#ifdef IS_TEST_NET
    case HIVE_ASSET_NUM_HIVE:
      return "TESTS";
    case HIVE_ASSET_NUM_HBD:
      return "TBD";
#else
    case HIVE_ASSET_NUM_HIVE:
      return "HIVE";
    case HIVE_ASSET_NUM_HBD:
      return "HBD";
#endif
    case HIVE_ASSET_NUM_VESTS:
      return "VESTS";
    default:
      return "UNKN"; // SMTs will return this symbol if returned as a legacy asset
  }
}

int64_t precision( const asset_symbol_type& symbol )
{
  static int64_t table[] = {
              1, 10, 100, 1000, 10000,
              100000, 1000000, 10000000, 100000000ll,
              1000000000ll, 10000000000ll,
              100000000000ll, 1000000000000ll,
              10000000000000ll, 100000000000000ll
              };
  uint8_t d = symbol.decimals();
  return table[ d ];
}

string legacy_asset::to_string()const
{
  int64_t prec = precision(symbol);
  string result = fc::to_string(amount.value / prec);
  if( prec > 1 )
  {
    auto fract = amount.value % prec;
    // prec is a power of ten, so for example when working with
    // 7.005 we have fract = 5, prec = 1000.  So prec+fract=1005
    // has the correct number of zeros and we can simply trim the
    // leading 1.
    result += "." + fc::to_string(prec + fract).erase(0,1);
  }
  return result + " " + asset_num_to_string( symbol.asset_num );
}

legacy_asset legacy_asset::from_string( const string& from )
{
  try
  {
    string s = fc::trim( from );
    auto space_pos = s.find( ' ' );
    auto dot_pos = s.find( '.' );

    FC_ASSERT( space_pos != std::string::npos );

    legacy_asset result;

    string str_symbol = s.substr( space_pos + 1 );

    if( dot_pos != std::string::npos )
    {
      FC_ASSERT( space_pos > dot_pos );

      auto intpart = s.substr( 0, dot_pos );
      auto fractpart = "1" + s.substr( dot_pos + 1, space_pos - dot_pos - 1 );
      uint8_t decimals = uint8_t( fractpart.size() - 1 );

      result.symbol.asset_num = string_to_asset_num( str_symbol.c_str(), decimals );

      int64_t prec = precision( result.symbol );

      //Max amount = 9223372036854775.807 HIVE/HBD
      //`inpart` * `prec` can cause overflow, better is to emulate multiplication using additional zeros
      auto _prec = std::to_string( prec );
      if( !_prec.empty() )
        intpart += _prec.substr( 1 );

      result.amount = fc::to_int64( intpart );

      int64_t _new_value = fc::to_int64( fractpart ) - prec;

      //adding `_new_value` can cause overflow, better is to check sum before addition
      int64_t check = result.amount.value + _new_value;
      bool overflow_a = result.amount.value > 0 && _new_value > 0 && check < 0;
      bool overflow_b = result.amount.value < 0 && _new_value < 0 && check > 0;
      if( overflow_a || overflow_b )
        FC_THROW_EXCEPTION( fc::parse_error_exception, "Couldn't parse int64_t" );
      else
        result.amount.value += _new_value;
    }
    else
    {
      auto intpart = s.substr( 0, space_pos );
      result.amount = fc::to_int64( intpart );
      result.symbol.asset_num = string_to_asset_num( str_symbol.c_str(), 0 );
    }
    return result;
  }
  FC_CAPTURE_AND_RETHROW( (from) )
}

} } // hive::protocol

namespace fc {

  void to_variant( const hive::protocol::asset& var, fc::variant& vo )
  {
    if( hive::protocol::serialization_mode_controller::legacy_enabled() )
      to_variant( hive::protocol::legacy_asset( var ), vo );
    else
    {
      try
      {
        variant v = mutable_variant_object( ASSET_AMOUNT_KEY, boost::lexical_cast< std::string >( var.amount.value ) )
                                ( ASSET_PRECISION_KEY, uint64_t( var.symbol.decimals() ) )
                                ( ASSET_NAI_KEY, var.symbol.to_nai_string() );
        vo = v;
      } FC_CAPTURE_AND_RETHROW()
    }
  }

  void to_variant( const hive::protocol::legacy_asset& a, fc::variant& var )
  {
    var = a.to_string();
  }

  void from_variant( const fc::variant& var, hive::protocol::asset& vo )
  {
    try
    {
      if( hive::protocol::serialization_mode_controller::legacy_enabled() )
      {
        hive::protocol::legacy_asset a;
        from_variant( var, a );
        vo = a.to_asset();
      }
      else
      {
        FC_ASSERT( var.is_object(), "Asset has to be treated as object." );

        const auto& v_object = var.get_object();

        FC_ASSERT( v_object.contains( ASSET_AMOUNT_KEY ), "Amount field doesn't exist." );
        FC_ASSERT( v_object[ ASSET_AMOUNT_KEY ].is_string(), "Expected a string type for value '${key}'.", ("key", ASSET_AMOUNT_KEY) );
        vo.amount = boost::lexical_cast< int64_t >( v_object[ ASSET_AMOUNT_KEY ].as< std::string >() );
        FC_ASSERT( vo.amount >= 0, "Asset amount cannot be negative" );

        FC_ASSERT( v_object.contains( ASSET_PRECISION_KEY ), "Precision field doesn't exist." );
        FC_ASSERT( v_object[ ASSET_PRECISION_KEY ].is_uint64(), "Expected an unsigned integer type for value '${key}'.", ("key", ASSET_PRECISION_KEY) );

        FC_ASSERT( v_object.contains( ASSET_NAI_KEY ), "NAI field doesn't exist." );
        FC_ASSERT( v_object[ ASSET_NAI_KEY ].is_string(), "Expected a string type for value '${key}'.", ("key", ASSET_NAI_KEY) );

        vo.symbol = hive::protocol::asset_symbol_type::from_nai_string( v_object[ ASSET_NAI_KEY ].as< std::string >().c_str(), v_object[ ASSET_PRECISION_KEY ].as< uint8_t >() );
      }
    } FC_CAPTURE_AND_RETHROW()
  }

  void from_variant( const fc::variant& var, hive::protocol::legacy_asset& a )
  {
    a = hive::protocol::legacy_asset::from_string( var.as_string() );
  }

}
