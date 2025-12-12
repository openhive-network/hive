#pragma once

#include <hive/chain/database.hpp>
#include <hive/chain/global_property_object.hpp>
#include <hive/chain/util/manabar.hpp>

#ifdef HIVE_ENABLE_SMT
#include <hive/chain/smt_objects.hpp>
#endif

namespace hive { namespace chain {

inline asset calculate_vesting( database& db, const asset& liquid, bool to_reward_balance )
{
  auto calculate_new_vesting = [ liquid ] ( price vesting_share_price ) -> asset
  {
    /**
      *  The ratio of total_vesting_shares / total_vesting_fund_hive should not
      *  change as the result of the user adding funds
      *
      *  V / C  = (V+Vn) / (C+Cn)
      *
      *  Simplifies to Vn = (V * Cn ) / C
      *
      *  If Cn equals o.amount, then we must solve for Vn to know how many new vesting shares
      *  the user should receive.
      *
      *  128 bit math is requred due to multiplying of 64 bit numbers. This is done in asset and price.
      */
    asset new_vesting = liquid * ( vesting_share_price );
    return new_vesting;
  };

#ifdef HIVE_ENABLE_SMT
  if( liquid.symbol.space() == asset_symbol_type::smt_nai_space )
  {
    FC_ASSERT( liquid.symbol.is_vesting() == false );
    // Get share price.
    const auto& smt = db.get< smt_token_object, by_symbol >( liquid.symbol );
    FC_ASSERT( smt.allow_voting == to_reward_balance, "No voting - no rewards" );
    price vesting_share_price = to_reward_balance ? smt.get_reward_vesting_share_price() : smt.get_vesting_share_price();
    // Calculate new vesting from provided liquid using share price.
    return calculate_new_vesting( vesting_share_price );
  }
#endif

  FC_ASSERT( liquid.symbol == HIVE_SYMBOL );
  // ^ A novelty, needed but risky in case someone managed to slip HBD/TESTS here in blockchain history.
  // Get share price.
  const auto& cprops = db.get_dynamic_global_properties();
  price vesting_share_price = to_reward_balance ? cprops.get_reward_vesting_share_price() : cprops.get_vesting_share_price();
  // Calculate new vesting from provided liquid using share price.
  return calculate_new_vesting( vesting_share_price );
}

// Create vesting, then a caller-supplied callback after determining how many shares to create, but before
// we modify the database.
// This allows us to implement virtual op pre-notifications in the Before function.
template< typename Before >
asset create_vesting2( database& db, const account_object& to_account, const asset& liquid, bool to_reward_balance, Before&& before_vesting_callback )
{
  try
  {
    asset new_vesting = db.adjust_account_vesting_balance( to_account, liquid, to_reward_balance, std::forward<Before>( before_vesting_callback ) );

    // Update witness voting numbers.
    if( !to_reward_balance )
      db.adjust_proxied_witness_votes( to_account, new_vesting.amount );

    return new_vesting;
  }
  FC_CAPTURE_AND_RETHROW( (to_account.get_name())(liquid) )
}

} } // hive::chain
