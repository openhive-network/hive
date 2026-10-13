#include <hive/chain/util/manabar.hpp>

namespace hive { namespace chain { namespace util {

int64_t calculate_downvote_manabar( manabar_params& params, int64_t effective_vests, int64_t new_mana,
  uint16_t downvote_pool_percent, uint32_t head_block_number, bool prevent_downvote_overflow )
{
  params.regen_time = HIVE_VOTING_MANA_REGENERATION_SECONDS;

#if defined(IS_TEST_NET) || defined(HIVE_CONVERTER_ICEBERG_PLUGIN_ENABLED)
  if( true )
#else //mainnet and regular mirrornet
  if( head_block_number > HIVE_HF_21_STALL_BLOCK )
#endif
  {
    params.max_mana = fc::uint128_to_int64( ( uint128_t( effective_vests ) * downvote_pool_percent ) / HIVE_100_PERCENT );
  }
  else
  {
    uint128_t numerator = uint128_t( effective_vests ) * downvote_pool_percent;
    if( fc::uint128_high_bits(numerator) != 0 )
      dlog( "NOTIFYALERT! max mana overflow made it in to the chain at block ${b}", ( "b", head_block_number ) );

    params.max_mana = ( effective_vests * downvote_pool_percent ) / HIVE_100_PERCENT;
  }

  // magnitude of new_mana (computed on unsigned to be safe also for INT64_MIN)
  uint64_t abs_new_mana = new_mana < 0 ? uint64_t( 0 ) - uint64_t( new_mana ) : uint64_t( new_mana );
  uint128_t numerator = uint128_t( abs_new_mana ) * downvote_pool_percent;
  if( prevent_downvote_overflow )
  {
    // downvote_pool_percent <= HIVE_100_PERCENT so the result always fits in int64_t
    int64_t scaled = fc::uint128_to_int64( numerator / HIVE_100_PERCENT );
    return new_mana < 0 ? scaled : -scaled;
  }
  else
  {
    if( fc::uint128_high_bits(numerator) != 0 ||
      fc::uint128_low_bits(numerator) > static_cast<uint64_t>( std::numeric_limits< int64_t >::max() ) )
      dlog( "NOTIFYALERT! downvote mana overflow made it in to the chain at block ${b}", ( "b", head_block_number ) );

    // historical behavior - wrap around like two's complement int64_t multiplication would (but without UB)
    int64_t product = static_cast<int64_t>( ( uint64_t( 0 ) - uint64_t( new_mana ) ) * uint64_t( downvote_pool_percent ) );
    return product / HIVE_100_PERCENT;
  }
}

} } } // hive::chain::util
