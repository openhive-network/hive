#pragma once

#include <hive/protocol/asset.hpp>

#include <hive/chain/hive_object_types.hpp>

namespace hive { namespace chain { namespace account_details {

  using hive::protocol::HBD_asset;
  using hive::protocol::HIVE_asset;
  using hive::protocol::VEST_asset;
  using hive::protocol::asset;

  class recovery
  {
    public:

      account_id_type   recovery_account;
      time_point_sec    last_account_recovery;
      time_point_sec    block_last_account_recovery;

    public:

      recovery() {}
      recovery( const account_id_type recovery_account )
              :recovery_account( recovery_account )
      {}

      //tells if account has some designated account that can initiate recovery (if not, top witness can)
      bool has_recovery_account() const { return recovery_account != account_id_type(); }

      //account's recovery account (if any), that is, an account that can authorize request_account_recovery_operation
      account_id_type get_recovery_account() const { return recovery_account; }

      //sets new recovery account
      void set_recovery_account(const account_id_type& new_recovery_account)
      {
        recovery_account = new_recovery_account;
      }

      //timestamp of last time account owner authority was successfully recovered
      time_point_sec get_last_account_recovery_time() const { return last_account_recovery; }

      //sets time of owner authority recovery
      void set_last_account_recovery_time( time_point_sec recovery_time )
      {
        last_account_recovery = recovery_time;
      }

      //time from a current block
      time_point_sec get_block_last_account_recovery_time() const { return block_last_account_recovery; }

      void set_block_last_account_recovery_time( time_point_sec block_recovery_time )
      {
        block_last_account_recovery = block_recovery_time;
      }
  };

  class assets
  {
    public:

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

    public:

      assets(){}

      assets( const asset& incoming_delegation )
      {
        received_vesting_shares += incoming_delegation;
      }

      //liquid HIVE balance
      const HIVE_asset& get_balance() const { return balance; }
      void set_balance( const HIVE_asset& value ) { balance = value; }
      //HIVE balance in savings
      const HIVE_asset& get_savings() const { return savings_balance; }
      void set_savings( const HIVE_asset& value ) { savings_balance = value; }
      //unclaimed HIVE rewards
      const HIVE_asset& get_rewards() const { return reward_hive_balance; }
      const void set_rewards( const HIVE_asset& value ) { reward_hive_balance = value; }

      //liquid HBD balance
      const HBD_asset& get_hbd_balance() const { return hbd_balance; }
      void set_hbd_balance( const HBD_asset& value ) { hbd_balance = value; }
      //HBD balance in savings
      const HBD_asset& get_hbd_savings() const { return savings_hbd_balance; }
      const void set_hbd_savings( const HBD_asset& value ) { savings_hbd_balance = value; }
      //unclaimed HBD rewards
      const HBD_asset& get_hbd_rewards() const { return reward_hbd_balance; }
      const void set_hbd_rewards( const HBD_asset& value ) { reward_hbd_balance = value; }

      //all VESTS held by the account - use other routines to get active VESTS for specific uses
      const VEST_asset& get_vesting() const { return vesting_shares; }
      void set_vesting( const VEST_asset& value ) { vesting_shares = value; }

      //VESTS that were delegated to other accounts
      const VEST_asset& get_delegated_vesting() const { return delegated_vesting_shares; }
      const void set_delegated_vesting( const VEST_asset& value ) { delegated_vesting_shares = value; }
      //VESTS that were borrowed from other accounts
      const VEST_asset& get_received_vesting() const { return received_vesting_shares; }
      const void set_received_vesting( const VEST_asset& value ) { received_vesting_shares = value; }
      //whole remainder of active power down (zero when not active)
      share_type get_total_vesting_withdrawal() const { return to_withdraw.amount - withdrawn.amount; }
      const VEST_asset& get_vesting_withdraw_rate() const { return vesting_withdraw_rate; }
      const void set_vesting_withdraw_rate( const VEST_asset& value ) { vesting_withdraw_rate = value; }

      //unclaimed VESTS rewards
      const VEST_asset& get_vest_rewards() const { return reward_vesting_balance; }
      const void set_vest_rewards( const VEST_asset& value ) { reward_vesting_balance = value; }
      //value of unclaimed VESTS rewards in HIVE (HIVE held on global balance)
      const HIVE_asset& get_vest_rewards_as_hive() const { return reward_vesting_hive; }
      const void set_vest_rewards_as_hive( const HIVE_asset& value ) { reward_vesting_hive = value; }

      const HIVE_asset& get_curation_rewards() const { return curation_rewards; }
      void set_curation_rewards( const HIVE_asset& value ) { curation_rewards = value; }
      const HIVE_asset& get_posting_rewards() const { return posting_rewards; }
      void set_posting_rewards( const HIVE_asset& value ) { posting_rewards = value; }

      const VEST_asset& get_withdrawn() const { return withdrawn; }
      const void set_withdrawn( const VEST_asset& value ) { withdrawn = value; }
      const VEST_asset& get_to_withdraw() const { return to_withdraw; }
      const void set_to_withdraw( const VEST_asset& value ) { to_withdraw = value; }
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