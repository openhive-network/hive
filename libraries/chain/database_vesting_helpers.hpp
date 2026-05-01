#pragma once

#include <hive/chain/database.hpp>
#include <hive/chain/global_property_object.hpp>
#include <hive/chain/util/manabar.hpp>

namespace hive { namespace chain {

// Create vesting, then a caller-supplied callback after determining how many shares to create, but before
// we modify the database.
// This allows us to implement virtual op pre-notifications in the Before function.
template< typename Before >
VEST_asset create_vesting2( database& db, const account_object& to_account, temp_HIVE_balance& liquid, bool is_reward, Before&& before_vesting_callback )
{
  VEST_asset new_vesting = db.adjust_account_vesting_balance( to_account, liquid, is_reward, std::forward<Before>( before_vesting_callback ) );

  // Update witness voting numbers.
  if( !is_reward )
    db.adjust_proxied_witness_votes( to_account, new_vesting.amount );

  return new_vesting;
}

} } // hive::chain
