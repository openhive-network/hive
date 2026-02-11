#pragma once

#include <hive/chain/database.hpp>
#include <hive/chain/global_property_object.hpp>
#include <hive/chain/util/manabar.hpp>

namespace hive { namespace chain {

inline VEST_asset calculate_vesting( database& db, const HIVE_asset& liquid, bool to_reward_balance )
{
  // Get share price.
  const auto& cprops = db.get_dynamic_global_properties();
  VEST_price vesting_share_price = to_reward_balance ? cprops.get_reward_vesting_share_price() : cprops.get_vesting_share_price();
  // Calculate new vesting from provided liquid using share price.
  return liquid * vesting_share_price;
}

// Create vesting, then a caller-supplied callback after determining how many shares to create, but before
// we modify the database.
// This allows us to implement virtual op pre-notifications in the Before function.
template< typename Before >
VEST_asset create_vesting2( database& db, const account_object& to_account, const HIVE_asset& liquid, bool to_reward_balance, Before&& before_vesting_callback )
{
  try
  {
    VEST_asset new_vesting = db.adjust_account_vesting_balance( to_account, liquid, to_reward_balance, std::forward<Before>( before_vesting_callback ) );

    // Update witness voting numbers.
    if( !to_reward_balance )
      db.adjust_proxied_witness_votes( to_account, new_vesting.amount );

    return new_vesting;
  }
  FC_CAPTURE_AND_RETHROW( (to_account.get_name())(liquid) )
}

} } // hive::chain
