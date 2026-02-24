#pragma once

#include <fc/uint128.hpp>

#include <hive/chain/hive_fwd.hpp>
#include <hive/chain/hive_object_types.hpp>
#include <hive/protocol/asset.hpp>

namespace hive { namespace chain {

  using hive::protocol::HBD_asset;
  using hive::protocol::HIVE_asset;
  using hive::protocol::VEST_asset;
  using hive::protocol::asset;

  class assets_object : public object< assets_object_type, assets_object, std::false_type /* no dynamic alloc */, std::true_type /* enable no undo remove */ >
  {
    CHAINBASE_OBJECT( assets_object );
    public:
      template< typename Allocator >
      assets_object( allocator< Allocator > a, uint64_t _id,
        const account_name_type& _name = account_name_type(),
        const asset& incoming_delegation = asset( 0, VESTS_SYMBOL ) )
        : id( _id ), name( _name )
      {
        received_vesting_shares += incoming_delegation;
      }

      // Account name (needed for by_next_vesting_withdrawal index to match old sort order)
      const account_name_type& get_name() const { return name; }

      // Liquid HIVE balance
      const HIVE_asset& get_balance() const { return balance; }
      void set_balance( const HIVE_asset& value ) { balance = value; }

      // HIVE balance in savings
      const HIVE_asset& get_savings() const { return savings_balance; }
      void set_savings( const HIVE_asset& value ) { savings_balance = value; }

      // Unclaimed HIVE rewards
      const HIVE_asset& get_rewards() const { return reward_hive_balance; }
      void set_rewards( const HIVE_asset& value ) { reward_hive_balance = value; }

      // Liquid HBD balance
      const HBD_asset& get_hbd_balance() const { return hbd_balance; }
      void set_hbd_balance( const HBD_asset& value ) { hbd_balance = value; }

      // HBD balance in savings
      const HBD_asset& get_hbd_savings() const { return savings_hbd_balance; }
      void set_hbd_savings( const HBD_asset& value ) { savings_hbd_balance = value; }

      // Unclaimed HBD rewards
      const HBD_asset& get_hbd_rewards() const { return reward_hbd_balance; }
      void set_hbd_rewards( const HBD_asset& value ) { reward_hbd_balance = value; }

      // All VESTS held by the account
      const VEST_asset& get_vesting() const { return vesting_shares; }
      void set_vesting( const VEST_asset& _new_amount ) { vesting_shares = _new_amount; }

      // VESTS that were delegated to other accounts
      const VEST_asset& get_delegated_vesting() const { return delegated_vesting_shares; }
      void set_delegated_vesting( const VEST_asset& value ) { delegated_vesting_shares = value; }

      // VESTS that were borrowed from other accounts
      const VEST_asset& get_received_vesting() const { return received_vesting_shares; }
      void set_received_vesting( const VEST_asset& value ) { received_vesting_shares = value; }

      // Whole remainder of active power down (zero when not active)
      share_type get_total_vesting_withdrawal() const { return to_withdraw.amount - withdrawn.amount; }

      // Unclaimed VESTS rewards
      const VEST_asset& get_vest_rewards() const { return reward_vesting_balance; }
      void set_vest_rewards( const VEST_asset& value ) { reward_vesting_balance = value; }

      // Value of unclaimed VESTS rewards in HIVE (HIVE held on global balance)
      const HIVE_asset& get_vest_rewards_as_hive() const { return reward_vesting_hive; }
      void set_vest_rewards_as_hive( const HIVE_asset& value ) { reward_vesting_hive = value; }

      // Withdrawn VESTS
      const VEST_asset& get_withdrawn() const { return withdrawn; }
      void set_withdrawn( const VEST_asset& value ) { withdrawn = value; }

      // VESTS yet to be withdrawn
      const VEST_asset& get_to_withdraw() const { return to_withdraw; }
      void set_to_withdraw( const VEST_asset& value ) { to_withdraw = value; }

      // Weekly power down rate
      const VEST_asset& get_vesting_withdraw_rate() const { return vesting_withdraw_rate; }
      void set_vesting_withdraw_rate( const VEST_asset& value ) { vesting_withdraw_rate = value; }

      // Sum of all curations (value before conversion to VESTS) - not used by consensus
      const HIVE_asset& get_curation_rewards() const { return curation_rewards; }
      void set_curation_rewards( const HIVE_asset& value ) { curation_rewards = value; }

      // Sum of all author rewards (value before conversion to VESTS/HBD) - not used by consensus
      const HIVE_asset& get_posting_rewards() const { return posting_rewards; }
      void set_posting_rewards( const HIVE_asset& value ) { posting_rewards = value; }

      // Savings HBD seconds
      uint128_t get_savings_hbd_seconds() const { return savings_hbd_seconds; }
      void set_savings_hbd_seconds( const uint128_t& value ) { savings_hbd_seconds = value; }

      // Last time savings HBD seconds was updated
      time_point_sec get_savings_hbd_seconds_last_update() const { return savings_hbd_seconds_last_update; }
      void set_savings_hbd_seconds_last_update( const time_point_sec& value ) { savings_hbd_seconds_last_update = value; }

      // Used to pay savings interest at most once per month
      time_point_sec get_savings_hbd_last_interest_payment() const { return savings_hbd_last_interest_payment; }
      void set_savings_hbd_last_interest_payment( const time_point_sec& value ) { savings_hbd_last_interest_payment = value; }

      // ===== Fields merged from time_object =====

      // Tells if account has active power down
      bool has_active_power_down() const { return next_vesting_withdrawal != fc::time_point_sec::maximum(); }

      // Next vesting withdrawal time
      time_point_sec get_next_vesting_withdrawal() const { return next_vesting_withdrawal; }
      void set_next_vesting_withdrawal( const time_point_sec& value ) { next_vesting_withdrawal = value; }

      // HBD seconds (liquid HBD * how long it has been held)
      uint128_t get_hbd_seconds() const { return hbd_seconds; }
      void set_hbd_seconds( const uint128_t& value ) { hbd_seconds = value; }

      // Last time HBD seconds was updated
      time_point_sec get_hbd_seconds_last_update() const { return hbd_seconds_last_update; }
      void set_hbd_seconds_last_update( const time_point_sec& value ) { hbd_seconds_last_update = value; }

      // Used to pay interest at most once per month
      time_point_sec get_hbd_last_interest_payment() const { return hbd_last_interest_payment; }
      void set_hbd_last_interest_payment( const time_point_sec& value ) { hbd_last_interest_payment = value; }

      // Only used by outdated consensus checks - up to HF17
      time_point_sec get_last_account_update() const { return last_account_update; }
      void set_last_account_update( const time_point_sec& value ) { last_account_update = value; }

      // Last post time
      time_point_sec get_last_post() const { return last_post; }
      void set_last_post( const time_point_sec& value ) { last_post = value; }

      // Influenced root comment reward between HF12 and HF17
      time_point_sec get_last_root_post() const { return last_root_post; }
      void set_last_root_post( const time_point_sec& value ) { last_root_post = value; }

      // Last post edit time
      time_point_sec get_last_post_edit() const { return last_post_edit; }
      void set_last_post_edit( const time_point_sec& value ) { last_post_edit = value; }

      // Only used by outdated consensus checks - up to HF26
      time_point_sec get_last_vote_time() const { return last_vote_time; }
      void set_last_vote_time( const time_point_sec& value ) { last_vote_time = value; }

    private:
      account_name_type name;                     // Account name (for index sort order compatibility)

      HBD_asset         hbd_balance;              ///< HBD liquid balance
      HBD_asset         savings_hbd_balance;      ///< HBD balance guarded by 3 day withdrawal (also earns interest)
      HBD_asset         reward_hbd_balance;       ///< HBD balance author rewards that can be claimed

      HIVE_asset        reward_hive_balance;      ///< HIVE balance author rewards that can be claimed
      HIVE_asset        reward_vesting_hive;      ///< HIVE counterweight to reward_vesting_balance
      HIVE_asset        balance;                  ///< HIVE liquid balance
      HIVE_asset        savings_balance;          ///< HIVE balance guarded by 3 day withdrawal

      VEST_asset        reward_vesting_balance;   ///< VESTS balance author/curation rewards that can be claimed
      VEST_asset        vesting_shares;           ///< VESTS balance, controls governance voting power
      VEST_asset        delegated_vesting_shares; ///< VESTS delegated out to other accounts
      VEST_asset        received_vesting_shares;  ///< VESTS delegated to this account
      VEST_asset        vesting_withdraw_rate;    ///< weekly power down rate

      HIVE_asset        curation_rewards;         ///< not used by consensus - sum of all curations
      HIVE_asset        posting_rewards;          ///< not used by consensus - sum of all author rewards

      VEST_asset        withdrawn;                ///< VESTS already withdrawn in currently active power down
      VEST_asset        to_withdraw;              ///< VESTS yet to be withdrawn in currently active power down

      uint128_t         savings_hbd_seconds = 0;  ///< savings HBD * how long it has been held
      time_point_sec    savings_hbd_seconds_last_update;
      time_point_sec    savings_hbd_last_interest_payment;

      // Fields merged from time_object
      uint128_t         hbd_seconds = 0;          ///< liquid HBD * how long it has been held

      time_point_sec    hbd_seconds_last_update;
      time_point_sec    hbd_last_interest_payment;

      time_point_sec    last_account_update;      //(only used by outdated consensus checks - up to HF17)
      time_point_sec    last_post;                //(we could probably remove limit on posting replies)
      time_point_sec    last_root_post;           //influenced root comment reward between HF12 and HF17
      time_point_sec    last_post_edit;           //(we could probably remove limit on post edits)
      time_point_sec    last_vote_time;           //(only used by outdated consensus checks - up to HF26)
      time_point_sec    next_vesting_withdrawal = fc::time_point_sec::maximum(); ///< after every withdrawal this is incremented by 1 week

    CHAINBASE_UNPACK_CONSTRUCTOR(assets_object);
  };



  typedef multi_index_container<
    assets_object,
    indexed_by<
      ordered_unique< tag< by_id >,
        const_mem_fun< assets_object, assets_object::id_type, &assets_object::get_id > >
    >,
    multi_index_allocator< assets_object >
  > assets_index;

} } // hive::chain

FC_REFLECT( hive::chain::assets_object,
          (id)(name)
          (hbd_balance)(savings_hbd_balance)(reward_hbd_balance)
          (reward_hive_balance)(reward_vesting_hive)(balance)(savings_balance)
          (reward_vesting_balance)(vesting_shares)(delegated_vesting_shares)
          (received_vesting_shares)(vesting_withdraw_rate)
          (curation_rewards)(posting_rewards)
          (withdrawn)(to_withdraw)
          (savings_hbd_seconds)(savings_hbd_seconds_last_update)
          (savings_hbd_last_interest_payment)
          (hbd_seconds)
          (hbd_seconds_last_update)(hbd_last_interest_payment)
          (last_account_update)(last_post)(last_root_post)
          (last_post_edit)(last_vote_time)(next_vesting_withdrawal)
        )

CHAINBASE_SET_INDEX_TYPE( hive::chain::assets_object, hive::chain::assets_index )
