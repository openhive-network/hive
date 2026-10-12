#pragma once

#include <cstdint>

#include <hive/protocol/config.hpp>

#include <fc/saturation.hpp>
#include <fc/uint128.hpp>
#include <fc/time.hpp>
#include <fc/exception/exception.hpp>
#include <fc/reflect/reflect.hpp>

namespace hive { namespace chain { namespace util {

using fc::uint128_t;

struct manabar_params
{
  int64_t    max_mana         = 0;
  uint32_t   regen_time       = 0;

  manabar_params() {}
  manabar_params( int64_t mm, uint32_t rt )
    : max_mana(mm), regen_time(rt) {}

  void validate()const
  { try{
    FC_ASSERT( max_mana >= 0 );
  } FC_CAPTURE_AND_RETHROW( (max_mana) ) }
};

struct manabar
{
  int64_t    current_mana     = 0;
  uint32_t   last_update_time = 0;

  manabar() {}
  manabar( int64_t m, uint32_t t )
    : current_mana(m), last_update_time(t) {}

  template< bool skip_cap_regen = false >
  void regenerate_mana( const manabar_params& params, uint32_t now )
  {
    params.validate();

    FC_ASSERT( now >= last_update_time );
    uint32_t dt = now - last_update_time;
    if( current_mana >= params.max_mana )
    {
      current_mana = params.max_mana;
      last_update_time = now;
      return;
    }
    if( dt == 0 )
      return;

    if( !skip_cap_regen )
      dt = (dt > params.regen_time) ? params.regen_time : dt;

    uint128_t max_mana_dt = uint64_t( params.max_mana >= 0 ? params.max_mana : 0 );
    max_mana_dt *= dt;
    uint64_t u_regen = fc::uint128_to_uint64(max_mana_dt / params.regen_time);
    FC_ASSERT( u_regen <= static_cast<uint64_t>( std::numeric_limits< int64_t >::max() ) );
    int64_t new_current_mana = fc::signed_sat_add( current_mana, int64_t( u_regen ) );
    current_mana = (new_current_mana > params.max_mana) ? params.max_mana : new_current_mana;

    last_update_time = now;
  }

  template< bool skip_cap_regen = false >
  void regenerate_mana( const manabar_params& params, fc::time_point_sec now )
  {
    regenerate_mana< skip_cap_regen >( params, now.sec_since_epoch() );
  }

  bool has_mana( int64_t mana_needed )const
  {
    return (mana_needed <= 0) || (current_mana >= mana_needed);
  }

  bool has_mana( uint64_t mana_needed )const
  {
    FC_ASSERT( mana_needed <= static_cast<uint64_t>( std::numeric_limits< int64_t >::max() ) );
    return has_mana( (int64_t) mana_needed );
  }

  void use_mana( int64_t mana_used, int64_t min_mana = std::numeric_limits< uint64_t >::min() )
  {
    current_mana = fc::signed_sat_sub( current_mana, mana_used );

    if( current_mana < min_mana )
    {
      current_mana = min_mana;
    }
  }

  void use_mana( uint64_t mana_used, int64_t min_mana = std::numeric_limits< uint64_t >::min() )
  {
    FC_ASSERT( mana_used <= static_cast<uint64_t>( std::numeric_limits< int64_t >::max() ) );
    use_mana( (int64_t) mana_used, min_mana );
  }
};

/**
  * Regenerates voting and downvote manabars and adds new_mana to them (new_mana can be negative).
  * prevent_downvote_overflow should reflect HIVE_HARDFORK_1_29_DOWNVOTE_MANA_OVERFLOW (issue #858) -
  * before that hardfork scaling of new_mana for downvote manabar was done on int64_t and could overflow.
  */
template< typename PropType, typename AccountType >
void update_manabar( const PropType& gpo, AccountType& account, int64_t new_mana, bool prevent_downvote_overflow )
{
  auto effective_vests = account.get_effective_vesting_shares().get_amount();
  try {
  manabar_params params( effective_vests, HIVE_VOTING_MANA_REGENERATION_SECONDS );
  account.voting_manabar.regenerate_mana( params, gpo.get_head_block_time() );
  account.voting_manabar.use_mana( -new_mana );
  } FC_CAPTURE_LOG_AND_RETHROW( (account)(effective_vests) )

  try{
  if( gpo.get_downvote_pool_percent() )
  {
    manabar_params params;
    params.regen_time = HIVE_VOTING_MANA_REGENERATION_SECONDS;

#if defined(IS_TEST_NET) || defined(HIVE_CONVERTER_ICEBERG_PLUGIN_ENABLED)
    if( true )
#else //mainnet and regular mirrornet
    if( gpo.get_head_block_number() > HIVE_HF_21_STALL_BLOCK )
#endif
    {
      params.max_mana = fc::uint128_to_int64( ( uint128_t( effective_vests ) * gpo.get_downvote_pool_percent() ) / HIVE_100_PERCENT );
    }
    else
    {
      uint128_t numerator = uint128_t( effective_vests ) * gpo.get_downvote_pool_percent();
      if( fc::uint128_high_bits(numerator) != 0 )
        dlog( "NOTIFYALERT! max mana overflow made it in to the chain at block ${b}", ( "b", gpo.get_head_block_number() ) );

      params.max_mana = ( effective_vests * gpo.get_downvote_pool_percent() ) / HIVE_100_PERCENT;
    }

    account.downvote_manabar.regenerate_mana( params, gpo.get_head_block_time() );

    // magnitude of new_mana (computed on unsigned to be safe also for INT64_MIN)
    uint64_t abs_new_mana = new_mana < 0 ? uint64_t( 0 ) - uint64_t( new_mana ) : uint64_t( new_mana );
    uint128_t numerator = uint128_t( abs_new_mana ) * gpo.get_downvote_pool_percent();
    int64_t mana_used = 0;
    if( prevent_downvote_overflow )
    {
      // downvote_pool_percent <= HIVE_100_PERCENT so the result always fits in int64_t
      int64_t scaled = fc::uint128_to_int64( numerator / HIVE_100_PERCENT );
      mana_used = new_mana < 0 ? scaled : -scaled;
    }
    else
    {
      if( fc::uint128_high_bits(numerator) != 0 ||
        fc::uint128_low_bits(numerator) > static_cast<uint64_t>( std::numeric_limits< int64_t >::max() ) )
        dlog( "NOTIFYALERT! downvote mana overflow made it in to the chain at block ${b}", ( "b", gpo.get_head_block_number() ) );

      // historical behavior - wrap around like two's complement int64_t multiplication would (but without UB)
      int64_t product = static_cast<int64_t>( ( uint64_t( 0 ) - uint64_t( new_mana ) ) * uint64_t( gpo.get_downvote_pool_percent() ) );
      mana_used = product / HIVE_100_PERCENT;
    }
    account.downvote_manabar.use_mana( mana_used );
  }
  } FC_CAPTURE_LOG_AND_RETHROW( (account)(effective_vests) )
}

template< typename PropType, typename AccountType >
void update_manabar( const PropType& gpo, AccountType& account )
{
  // no new mana means no risk of overflow
  update_manabar( gpo, account, 0, true );
}

} } } // hive::chain::util

FC_REFLECT( hive::chain::util::manabar,
  (current_mana)
  (last_update_time)
  )
