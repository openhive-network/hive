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

  struct changeable
  {
    bool changed = false;

    void maybe_changed()
    {
      changed = true;
    }

    void maybe_changed() const
    {
    }
  };

  struct recovery: public changeable
  {
    account_id_type   recovery_account;
    time_point_sec    last_account_recovery;
    time_point_sec    block_last_account_recovery;

    bool has_recovery_account() const { maybe_changed(); return recovery_account != account_id_type(); }

    account_id_type get_recovery_account() const { maybe_changed(); return recovery_account; }
    void set_recovery_account(const account_id_type& new_recovery_account) { maybe_changed(); recovery_account = new_recovery_account; }

    time_point_sec get_last_account_recovery_time() const { maybe_changed(); return last_account_recovery; }
    void set_last_account_recovery_time( time_point_sec recovery_time ) { maybe_changed(); last_account_recovery = recovery_time; }

    time_point_sec get_block_last_account_recovery_time() const { maybe_changed(); return block_last_account_recovery; }
    void set_block_last_account_recovery_time( time_point_sec block_recovery_time ) { maybe_changed(); block_last_account_recovery = block_recovery_time; }

    recovery() {}
    recovery( const account_id_type recovery_account )
            :recovery_account( recovery_account ){}
  };

  struct assets: public changeable
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

    const HIVE_asset& get_balance() const { maybe_changed(); return balance; }
    void set_balance( const HIVE_asset& value ) { maybe_changed(); balance = value; }

    const HIVE_asset& get_savings() const { maybe_changed(); return savings_balance; }
    void set_savings( const HIVE_asset& value ) { maybe_changed(); savings_balance = value; }

    const HIVE_asset& get_rewards() const { maybe_changed(); return reward_hive_balance; }
    void set_rewards( const HIVE_asset& value ) { maybe_changed(); reward_hive_balance = value; }

    const HBD_asset& get_hbd_balance() const { maybe_changed(); return hbd_balance; }
    void set_hbd_balance( const HBD_asset& value ) { maybe_changed(); hbd_balance = value; }

    const HBD_asset& get_hbd_savings() const { maybe_changed(); return savings_hbd_balance; }
    void set_hbd_savings( const HBD_asset& value ) { maybe_changed(); savings_hbd_balance = value; }

    const HBD_asset& get_hbd_rewards() const { maybe_changed(); return reward_hbd_balance; }
    void set_hbd_rewards( const HBD_asset& value ) { maybe_changed(); reward_hbd_balance = value; }

    const VEST_asset& get_vesting() const { maybe_changed(); return vesting_shares; }
    void set_vesting( const VEST_asset& value ) { maybe_changed(); vesting_shares = value; }

    const VEST_asset& get_delegated_vesting() const { maybe_changed(); return delegated_vesting_shares; }
    void set_delegated_vesting( const VEST_asset& value ) { maybe_changed(); delegated_vesting_shares = value; }

    const VEST_asset& get_received_vesting() const { maybe_changed(); return received_vesting_shares; }
    void set_received_vesting( const VEST_asset& value ) { maybe_changed(); received_vesting_shares = value; }

    share_type get_total_vesting_withdrawal() const { maybe_changed(); return to_withdraw.amount - withdrawn.amount; }
    const VEST_asset& get_vesting_withdraw_rate() const { maybe_changed(); return vesting_withdraw_rate; }
    void set_vesting_withdraw_rate( const VEST_asset& value ) { maybe_changed(); vesting_withdraw_rate = value; }

    const VEST_asset& get_vest_rewards() const { maybe_changed(); return reward_vesting_balance; }
    void set_vest_rewards( const VEST_asset& value ) { maybe_changed(); reward_vesting_balance = value; }

    const HIVE_asset& get_vest_rewards_as_hive() const { maybe_changed(); return reward_vesting_hive; }
    void set_vest_rewards_as_hive( const HIVE_asset& value ) { maybe_changed(); reward_vesting_hive = value; }

    const HIVE_asset& get_curation_rewards() const { maybe_changed(); return curation_rewards; }
    void set_curation_rewards( const HIVE_asset& value ) { maybe_changed(); curation_rewards = value; }
    const HIVE_asset& get_posting_rewards() const { maybe_changed(); return posting_rewards; }
    void set_posting_rewards( const HIVE_asset& value ) { maybe_changed(); posting_rewards = value; }

    const VEST_asset& get_withdrawn() const { maybe_changed(); return withdrawn; }
    void set_withdrawn( const VEST_asset& value ) { maybe_changed(); withdrawn = value; }
    const VEST_asset& get_to_withdraw() const { maybe_changed(); return to_withdraw; }
    void set_to_withdraw( const VEST_asset& value ) { maybe_changed(); to_withdraw = value; }

    assets(){}

    assets( const asset& incoming_delegation )
    {
      received_vesting_shares += incoming_delegation;
    }
  };

  struct manabars_rc: public changeable
  {
    util::manabar     voting_manabar;
    util::manabar     downvote_manabar;
    util::manabar     rc_manabar;

    share_type        rc_adjustment; ///< compensation for account creation fee in form of extra RC
    share_type        delegated_rc; ///< RC delegated out to other accounts
    share_type        received_rc; ///< RC delegated to this account
    share_type        last_max_rc; ///< (for bug catching with RC code, can be removed once RC becomes part of consensus)

    share_type get_rc_adjustment() const { maybe_changed(); return rc_adjustment; }
    void set_rc_adjustment( const share_type& value ) { maybe_changed(); rc_adjustment = value; }

    share_type get_delegated_rc() const { maybe_changed(); return delegated_rc; }
    void set_delegated_rc( const share_type& value ) { maybe_changed(); delegated_rc = value; }

    share_type get_received_rc() const { maybe_changed(); return received_rc; }
    void set_received_rc( const share_type& value ) { maybe_changed(); received_rc = value; }

    share_type get_last_max_rc() const { maybe_changed(); return last_max_rc; }
    void set_last_max_rc( const share_type& value ) { maybe_changed(); last_max_rc = value; }

    util::manabar& get_rc_manabar() { maybe_changed(); return rc_manabar; };
    const util::manabar& get_rc_manabar() const { maybe_changed(); return rc_manabar; };

    util::manabar& get_voting_manabar() { maybe_changed(); return voting_manabar; };
    const util::manabar& get_voting_manabar() const { maybe_changed(); return voting_manabar; };

    util::manabar& get_downvote_manabar() { maybe_changed(); return downvote_manabar; };
    const util::manabar& get_downvote_manabar() const { maybe_changed(); return downvote_manabar; };

    share_type get_maximum_rc( share_type effective_vesting_shares, bool only_delegable = false ) const
    {
      maybe_changed();
      share_type total = effective_vesting_shares - delegated_rc;
      if( only_delegable == false )
        total += rc_adjustment + received_rc;
      return total;
    }

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

  };

  struct time: public changeable
  {
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

    uint128_t get_hbd_seconds() const { maybe_changed(); return hbd_seconds; }
    void set_hbd_seconds( const uint128_t& value ) { maybe_changed(); hbd_seconds = value; }

    uint128_t get_savings_hbd_seconds() const { maybe_changed(); return savings_hbd_seconds; }
    void set_savings_hbd_seconds( const uint128_t& value ) { maybe_changed(); savings_hbd_seconds = value; }

    time_point_sec get_hbd_seconds_last_update() const { maybe_changed(); return hbd_seconds_last_update; }
    void set_hbd_seconds_last_update( const time_point_sec& value ) { maybe_changed(); hbd_seconds_last_update = value; }

    time_point_sec get_hbd_last_interest_payment() const { maybe_changed(); return hbd_last_interest_payment; }
    void set_hbd_last_interest_payment( const time_point_sec& value ) { maybe_changed(); hbd_last_interest_payment = value; }

    time_point_sec get_savings_hbd_seconds_last_update() const { maybe_changed(); return savings_hbd_seconds_last_update; }
    void set_savings_hbd_seconds_last_update( const time_point_sec& value ) { maybe_changed(); savings_hbd_seconds_last_update = value; }

    time_point_sec get_savings_hbd_last_interest_payment() const { maybe_changed(); return savings_hbd_last_interest_payment; }
    void set_savings_hbd_last_interest_payment( const time_point_sec& value ) { maybe_changed(); savings_hbd_last_interest_payment = value; }

    time_point_sec get_last_account_update() const { maybe_changed(); return last_account_update; }
    void set_last_account_update( const time_point_sec& value ) { maybe_changed(); last_account_update = value; }

    time_point_sec get_last_post() const { maybe_changed(); return last_post; }
    void set_last_post( const time_point_sec& value ) { maybe_changed(); last_post = value; }

    time_point_sec get_last_root_post() const { maybe_changed(); return last_root_post; }
    void set_last_root_post( const time_point_sec& value ) { maybe_changed(); last_root_post = value; }

    time_point_sec get_last_post_edit() const { maybe_changed(); return last_post_edit; }
    void set_last_post_edit( const time_point_sec& value ) { maybe_changed(); last_post_edit = value; }

    time_point_sec get_last_vote_time() const { maybe_changed(); return last_vote_time; }
    void set_last_vote_time( const time_point_sec& value ) { maybe_changed(); last_vote_time = value; }

    //tells if account has active power down
    bool has_active_power_down() const { maybe_changed(); return next_vesting_withdrawal != fc::time_point_sec::maximum(); }

    time_point_sec get_next_vesting_withdrawal() const { maybe_changed(); return next_vesting_withdrawal; }
    void set_next_vesting_withdrawal( const time_point_sec& value ) { maybe_changed(); next_vesting_withdrawal = value; }
  };

  struct misc: public changeable
  {
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
    bool              check_can_vote = true;

    bool              mined = true; //(not read by consensus code)

    public_key_type   memo_key; //33 bytes with alignment of 1; (it belongs to metadata as it is not used by consensus, but witnesses need it here since they don't COLLECT_ACCOUNT_METADATA)

    fc::array<share_type, HIVE_MAX_PROXY_RECURSION_DEPTH> proxied_vsf_votes; ///< the total VFS votes proxied to this account

    misc(){}

    misc( const account_name_type& _name, const time_point_sec& _creation_time, const time_point_sec& _block_creation_time,
          bool _mined, const public_key_type& _memo_key )
          : name( _name ), created( _creation_time ),
            block_created( _block_creation_time )/*_block_creation_time = time from a current block*/,
            mined( _mined ), memo_key( _memo_key )
    {
    }

    bool can_vote() const { maybe_changed(); return check_can_vote; }
    void set_can_vote( const bool& value ) { maybe_changed(); check_can_vote = value; }

    uint16_t get_witnesses_voted_for() const { maybe_changed(); return witnesses_voted_for; }
    void set_witnesses_voted_for( const uint16_t& value ) { maybe_changed(); witnesses_voted_for = value; }

    fc::array<share_type, HIVE_MAX_PROXY_RECURSION_DEPTH>& get_proxied_vsf_votes() { maybe_changed(); return proxied_vsf_votes; }
    const fc::array<share_type, HIVE_MAX_PROXY_RECURSION_DEPTH>& get_proxied_vsf_votes() const { maybe_changed(); return proxied_vsf_votes; }

    ushare_type get_sum_delayed_votes() const { maybe_changed(); return sum_delayed_votes; }
    ushare_type& get_sum_delayed_votes() { maybe_changed(); return sum_delayed_votes; }
    void set_sum_delayed_votes( const ushare_type& value ) { maybe_changed(); sum_delayed_votes = value; }

    uint8_t get_savings_withdraw_requests() const { maybe_changed(); return savings_withdraw_requests; }
    void set_savings_withdraw_requests( const uint8_t& value ) { maybe_changed(); savings_withdraw_requests = value; }

    uint16_t get_pending_escrow_transfers() const { maybe_changed(); return pending_escrow_transfers; }
    void set_pending_escrow_transfers( const uint16_t& value ) { maybe_changed(); pending_escrow_transfers = value; }

    uint32_t get_post_bandwidth() const { maybe_changed(); return post_bandwidth; }
    void set_post_bandwidth( const uint32_t& value ) { maybe_changed(); post_bandwidth = value; }

    share_type get_pending_claimed_accounts() const { maybe_changed(); return pending_claimed_accounts; }
    void set_pending_claimed_accounts( const share_type& value ) { maybe_changed(); pending_claimed_accounts = value; }

    uint16_t get_open_recurrent_transfers() const { maybe_changed(); return open_recurrent_transfers; }
    void set_open_recurrent_transfers( const uint16_t& value ) { maybe_changed(); open_recurrent_transfers = value; }

    uint32_t get_post_count() const { maybe_changed(); return post_count; }
    void set_post_count( const uint32_t& value ) { maybe_changed(); post_count = value; }

    uint16_t get_withdraw_routes() const { maybe_changed(); return withdraw_routes; }
    void set_withdraw_routes( const uint16_t& value ) { maybe_changed(); withdraw_routes = value; }

    public_key_type get_memo_key() const { maybe_changed(); return memo_key; }
    void set_memo_key( const public_key_type& value ) { maybe_changed(); memo_key = value; }

    //gives name of the account
    const account_name_type& get_name() const { maybe_changed(); return name; }
    //account creation time
    time_point_sec get_creation_time() const { maybe_changed(); return created; }
    //tells if account was created through pow/pow2 mining operation or is one of builtin accounts created during genesis
    //account creation time according to a block
    time_point_sec get_block_creation_time() const { maybe_changed(); return block_created; }
    bool was_mined() const { maybe_changed(); return mined; }

    //tells if account has some other account casting governance votes in its name
    bool has_proxy() const { maybe_changed(); return proxy != account_id_type(); }
    //account's proxy (if any)
    account_id_type get_proxy() const { maybe_changed(); return proxy; }
    //sets proxy to neutral (account will vote for itself)
    void clear_proxy() { maybe_changed(); proxy = account_id_type(); }
    //sets proxy to given account
    void set_proxy( const account_id_type& new_proxy ) { maybe_changed(); proxy = new_proxy; }

    time_point_sec get_governance_vote_expiration_ts() const { maybe_changed(); return governance_vote_expiration_ts; }

    void set_governance_vote_expired() { maybe_changed(); governance_vote_expiration_ts = time_point_sec::maximum(); }

    void update_governance_vote_expiration_ts(const time_point_sec vote_time)
    {
      maybe_changed(); 
      governance_vote_expiration_ts = vote_time + HIVE_GOVERNANCE_VOTE_EXPIRATION_PERIOD;
      if (governance_vote_expiration_ts < HARDFORK_1_25_FIRST_GOVERNANCE_VOTE_EXPIRE_TIMESTAMP)
      {
        const int64_t DIVIDER = HIVE_HARDFORK_1_25_MAX_OLD_GOVERNANCE_VOTE_EXPIRE_SHIFT.to_seconds();
        governance_vote_expiration_ts = HARDFORK_1_25_FIRST_GOVERNANCE_VOTE_EXPIRE_TIMESTAMP + fc::seconds(governance_vote_expiration_ts.sec_since_epoch() % DIVIDER);
      }
    }

    // governance vote power of this account does not include "delayed votes"
    share_type get_direct_governance_vote_power( const VEST_asset& vests ) const
    {
      maybe_changed(); 
      FC_ASSERT( sum_delayed_votes.value <= vests.amount, "",
              ( "sum_delayed_votes",     sum_delayed_votes )
              ( "vesting_shares.amount", vests.amount )
              ( "account",               name ) );

      return asset( vests.amount - sum_delayed_votes.value, VESTS_SYMBOL ).amount;
    }

    /// This function should be used only when the account votes for a witness directly
    share_type get_governance_vote_power( const VEST_asset& vests ) const
    {
      maybe_changed(); 
      return std::accumulate( proxied_vsf_votes.begin(), proxied_vsf_votes.end(),
        get_direct_governance_vote_power( vests ) );
    }
    share_type proxied_vsf_votes_total() const
    {
      maybe_changed(); 
      return std::accumulate( proxied_vsf_votes.begin(), proxied_vsf_votes.end(),
        share_type() );
    }

#ifdef IS_TEST_NET
    //ABW: it is needed for some low level tests (they would need to be rewritten)
    void set_name( const account_name_type& new_name ) { maybe_changed(); name = new_name; }
#endif

  };

  struct delayed_votes_wrapper: public changeable
  {
    t_delayed_votes delayed_votes;

    template< typename Allocator >
    delayed_votes_wrapper( allocator< Allocator > a )
      : delayed_votes( a ) {}

    account_details::t_delayed_votes& get_delayed_votes() { maybe_changed(); return delayed_votes; }
    const account_details::t_delayed_votes& get_delayed_votes() const { maybe_changed(); return delayed_votes; }

    bool has_delayed_votes() const { maybe_changed(); return !delayed_votes.empty(); }

    // start time of oldest delayed vote bucket (the one closest to activation)
    time_point_sec get_oldest_delayed_vote_time() const
    {
      maybe_changed(); 
      if( has_delayed_votes() )
        return ( delayed_votes.begin() )->time;
      else
        return time_point_sec::maximum();
    }

    void fill( const std::vector< delayed_votes_data >& src )
    {
      maybe_changed(); 
      if( src.size() )
      {
        delayed_votes.reserve( src.size() );
        for( const auto& item : src )
          delayed_votes.push_back( item );
      }
    }

    size_t get_dynamic_alloc() const
    {
      size_t size = 0;
      size += delayed_votes.capacity() * sizeof( decltype( delayed_votes )::value_type );
      return size;
    }
  };

}}}

FC_REFLECT( hive::chain::account_details::changeable,
            (changed)
          )

FC_REFLECT_DERIVED( hive::chain::account_details::recovery, (hive::chain::account_details::changeable),
            (recovery_account)(last_account_recovery)(block_last_account_recovery)
          )

FC_REFLECT_DERIVED( hive::chain::account_details::assets, (hive::chain::account_details::changeable),
            (hbd_balance)(savings_hbd_balance)
            (reward_hbd_balance)(reward_hive_balance)
            (reward_vesting_hive)(balance)
            (savings_balance)(reward_vesting_balance)
            (vesting_shares)(delegated_vesting_shares)
            (received_vesting_shares)(vesting_withdraw_rate)
            (curation_rewards)(posting_rewards)
            (withdrawn)(to_withdraw)
          )

FC_REFLECT_DERIVED( hive::chain::account_details::manabars_rc, (hive::chain::account_details::changeable),
            (voting_manabar)
            (downvote_manabar)
            (rc_manabar)
            (rc_adjustment)(delegated_rc)
            (received_rc)(last_max_rc)
          )

FC_REFLECT_DERIVED( hive::chain::account_details::time, (hive::chain::account_details::changeable),
            (hbd_seconds)
            (savings_hbd_seconds)
            (hbd_seconds_last_update)(hbd_last_interest_payment)(savings_hbd_seconds_last_update)(savings_hbd_last_interest_payment)
            (last_account_update)(last_post)(last_root_post)
            (last_post_edit)(last_vote_time)(next_vesting_withdrawal)
          )

FC_REFLECT_DERIVED( hive::chain::account_details::misc, (hive::chain::account_details::changeable),
          (proxy)
          (name)
          (pending_claimed_accounts)(sum_delayed_votes)
          (created)(block_created)
          (governance_vote_expiration_ts)
          (post_count)(post_bandwidth)(withdraw_routes)(pending_escrow_transfers)(open_recurrent_transfers)(witnesses_voted_for)
          (savings_withdraw_requests)(check_can_vote)(mined)
          (memo_key)
          (proxied_vsf_votes)
        )

FC_REFLECT_DERIVED( hive::chain::account_details::delayed_votes_wrapper, (hive::chain::account_details::changeable),
            (delayed_votes)
          )
