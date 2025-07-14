#pragma once

#include <hive/protocol/asset.hpp>

#include <hive/chain/util/manabar.hpp>
#include <hive/chain/util/delayed_voting_processor.hpp>

namespace hive { namespace chain { namespace account_details {

  using hive::protocol::HBD_asset;
  using hive::protocol::HIVE_asset;
  using hive::protocol::VEST_asset;
  using hive::protocol::asset;
  using hive::protocol::public_key_type;

  using chainbase::t_vector;

  using t_delayed_votes = t_vector< delayed_votes_data >;

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

  struct manabars_rc
  {
    util::manabar     voting_manabar;
    util::manabar     downvote_manabar;
    util::manabar     rc_manabar;

    share_type        rc_adjustment; ///< compensation for account creation fee in form of extra RC
    share_type        delegated_rc; ///< RC delegated out to other accounts
    share_type        received_rc; ///< RC delegated to this account
    share_type        last_max_rc; ///< (for bug catching with RC code, can be removed once RC becomes part of consensus)

    manabars_rc(){}
    manabars_rc( const time_point_sec& _creation_time, bool _fill_mana, int64_t _rc_adjustment, share_type effective_vesting_shares )
      : rc_adjustment( _rc_adjustment )
    {
      /*
        Explanation:
          _creation_time = time is retrieved from a head block
      */
      voting_manabar.last_update_time = _creation_time.sec_since_epoch();
      downvote_manabar.last_update_time = _creation_time.sec_since_epoch();
      if( _fill_mana )
        voting_manabar.current_mana = HIVE_100_PERCENT; //looks like nonsense, but that's because pre-HF20 manabars carried percentage, not actual value
      if( rc_adjustment.value )
      {
        rc_manabar.last_update_time = _creation_time.sec_since_epoch();
        auto max_rc = get_maximum_rc( effective_vesting_shares ).value;
        rc_manabar.current_mana = max_rc;
        last_max_rc = max_rc;
      }
    }

    share_type get_maximum_rc( share_type effective_vesting_shares, bool only_delegable = false ) const
    {
      share_type total = effective_vesting_shares - delegated_rc;
      if( only_delegable == false )
        total += rc_adjustment + received_rc;
      return total;
    }
  };

  struct time
  {
    time(){}

    uint128_t         hbd_seconds = 0; ///< liquid HBD * how long it has been held
    uint128_t         savings_hbd_seconds = 0; ///< savings HBD * how long it has been held

    time_point_sec    hbd_seconds_last_update; ///< the last time the hbd_seconds was updated
    time_point_sec    hbd_last_interest_payment; ///< used to pay interest at most once per month
    time_point_sec    savings_hbd_seconds_last_update; ///< the last time the hbd_seconds was updated
    time_point_sec    savings_hbd_last_interest_payment; ///< used to pay interest at most once per month

    time_point_sec    last_account_update; //(only used by outdated consensus checks - up to HF17)
    time_point_sec    last_post; //(we could probably remove limit on posting replies)
    time_point_sec    last_root_post; //influenced root comment reward between HF12 and HF17
    time_point_sec    last_post_edit; //(we could probably remove limit on post edits)
    time_point_sec    last_vote_time; //(only used by outdated consensus checks - up to HF26)
    time_point_sec    next_vesting_withdrawal = fc::time_point_sec::maximum(); ///< after every withdrawal this is incremented by 1 week
  };

  struct misc
  {
    misc(){}

    misc( const account_name_type& _name, const time_point_sec& _creation_time, const time_point_sec& _block_creation_time,
          bool _mined, const public_key_type& _memo_key )
          : name( _name ), created( _creation_time ),
            block_created( _block_creation_time )/*_block_creation_time = time from a current block*/,
            mined( _mined ), memo_key( _memo_key )
    {
    }

    account_id_type   proxy;

    account_name_type name;

    share_type        pending_claimed_accounts = 0; ///< claimed and not yet used account creation tokens (could be 32bit)

    ushare_type       sum_delayed_votes = 0; ///< sum of delayed_votes (should be changed to VEST_asset)

    time_point_sec    created; //(not read by consensus code)
    time_point_sec    block_created;

    time_point_sec    governance_vote_expiration_ts = fc::time_point_sec::maximum();

    uint32_t          post_count = 0; //(not read by consensus code)
    uint32_t          post_bandwidth = 0; //influenced root comment reward between HF12 and HF17

    uint16_t          withdraw_routes = 0; //max 10, why is it 16bit?
    uint16_t          pending_escrow_transfers = 0; //for now max is 255, but it might change
    uint16_t          open_recurrent_transfers = 0; //for now max is 255, but it might change
    uint16_t          witnesses_voted_for = 0; //max 30, why is it 16bit?

    uint8_t           savings_withdraw_requests = 0;
    bool              can_vote = true;

    bool              mined = true; //(not read by consensus code)

    public_key_type   memo_key; //33 bytes with alignment of 1; (it belongs to metadata as it is not used by consensus, but witnesses need it here since they don't COLLECT_ACCOUNT_METADATA)

    fc::array<share_type, HIVE_MAX_PROXY_RECURSION_DEPTH> proxied_vsf_votes; ///< the total VFS votes proxied to this account
  };

  struct shared_delayed_votes
  {
    template< typename Allocator >
    shared_delayed_votes( allocator< Allocator > a ): delayed_votes( a ) {}

    /*
      Holds sum of VESTS per day.
      VESTS from day `X` will be matured after `X` + 30 days ( because `HIVE_DELAYED_VOTING_TOTAL_INTERVAL_SECONDS` == 30 days )
    */
    t_delayed_votes   delayed_votes;

    bool has_delayed_votes() const { return !delayed_votes.empty(); }

    // start time of oldest delayed vote bucket (the one closest to activation)
    time_point_sec get_oldest_delayed_vote_time() const
    {
      if( has_delayed_votes() )
        return ( delayed_votes.begin() )->time;
      else
        return time_point_sec::maximum();
    }

    size_t get_dynamic_alloc() const
    {
      size_t size = 0;
      size += delayed_votes.capacity() * sizeof( decltype( delayed_votes )::value_type );
      return size;
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

FC_REFLECT( hive::chain::account_details::manabars_rc,
            (voting_manabar)
            (downvote_manabar)
            (rc_manabar)
            (rc_adjustment)(delegated_rc)
            (received_rc)(last_max_rc)
          )

FC_REFLECT( hive::chain::account_details::time,
            (hbd_seconds)
            (savings_hbd_seconds)
            (hbd_seconds_last_update)(hbd_last_interest_payment)(savings_hbd_seconds_last_update)(savings_hbd_last_interest_payment)
            (last_account_update)(last_post)(last_root_post)
            (last_post_edit)(last_vote_time)(next_vesting_withdrawal)
          )

FC_REFLECT( hive::chain::account_details::misc,
          (proxy)
          (name)
          (pending_claimed_accounts)(sum_delayed_votes)
          (created)(block_created)
          (governance_vote_expiration_ts)
          (post_count)(post_bandwidth)(withdraw_routes)(pending_escrow_transfers)(open_recurrent_transfers)(witnesses_voted_for)
          (savings_withdraw_requests)(can_vote)(mined)
          (memo_key)
          (proxied_vsf_votes)
        )

FC_REFLECT( hive::chain::account_details::shared_delayed_votes,
          (delayed_votes)
        )
