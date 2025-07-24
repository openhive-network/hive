#pragma once

#include <hive/protocol/asset.hpp>

#include <hive/chain/hive_object_types.hpp>

namespace hive { namespace chain { namespace account_details {

  using hive::protocol::HBD_asset;
  using hive::protocol::HIVE_asset;
  using hive::protocol::VEST_asset;
  using hive::protocol::asset;

  struct recovery
  {
    account_id_type   recovery_account;
    time_point_sec    last_account_recovery;
    time_point_sec    block_last_account_recovery;

    recovery() {}
    recovery( const account_id_type recovery_account )
            :recovery_account( recovery_account ){}
  };

  struct assets
  {
    HBD_asset         hbd_balance; ///< HBD liquid balance
    HBD_asset         savings_hbd_balance; ///< HBD balance guarded by 3 day withdrawal (also earns interest)
    HBD_asset         reward_hbd_balance; ///< HBD balance author rewards that can be claimed

    HIVE_asset        reward_hive_balance; ///< HIVE balance author rewards that can be claimed
    HIVE_asset        reward_vesting_hive; ///< HIVE counterweight to reward_vesting_balance
    HIVE_asset        balance;  ///< HIVE liquid balance
    HIVE_asset        savings_balance;  ///< HIVE balance guarded by 3 day withdrawal

    VEST_asset        reward_vesting_balance; ///< VESTS balance author/curation rewards that can be claimed
    VEST_asset        vesting_shares; ///< VESTS balance, controls governance voting power
    VEST_asset        delegated_vesting_shares; ///< VESTS delegated out to other accounts
    VEST_asset        received_vesting_shares; ///< VESTS delegated to this account
    VEST_asset        vesting_withdraw_rate; ///< weekly power down rate

    HIVE_asset        curation_rewards; ///< not used by consensus - sum of all curations (value before conversion to VESTS)
    HIVE_asset        posting_rewards; ///< not used by consensus - sum of all author rewards (value before conversion to VESTS/HBD)

    VEST_asset        withdrawn; ///< VESTS already withdrawn in currently active power down (why do we even need this?)
    VEST_asset        to_withdraw; ///< VESTS yet to be withdrawn in currently active power down (withdown should just be subtracted from this)

    assets(){}

    assets( const asset& incoming_delegation )
    {
      received_vesting_shares += incoming_delegation;
    }
  };

}}}

FC_REFLECT( hive::chain::account_details::recovery,
            (recovery_account)(last_account_recovery)(block_last_account_recovery)
          )

FC_REFLECT( hive::chain::account_details::assets,
            (hbd_balance)(savings_hbd_balance)
            (reward_hbd_balance)(reward_hive_balance)
            (reward_vesting_hive)(balance)
            (savings_balance)(reward_vesting_balance)
            (vesting_shares)(delegated_vesting_shares)
            (received_vesting_shares)(vesting_withdraw_rate)
            (curation_rewards)(posting_rewards)
            (withdrawn)(to_withdraw)
          )