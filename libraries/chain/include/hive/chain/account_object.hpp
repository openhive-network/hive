#pragma once
#include <fc/uint128.hpp>

#include <hive/chain/hive_fwd.hpp>

#include <hive/protocol/fixed_string.hpp>
#include <hive/protocol/authority.hpp>
#include <hive/protocol/hive_operations.hpp>

#include <hive/chain/hive_object_types.hpp>
#include <hive/chain/witness_objects.hpp>
#include <hive/chain/shared_authority.hpp>
#include <hive/chain/util/manabar.hpp>

#include <hive/chain/util/delayed_voting_processor.hpp>

#include <numeric>

namespace hive { namespace chain {

  using chainbase::t_vector;
  using hive::protocol::authority;

  class account_object : public object< account_object_type, account_object >
  {
    CHAINBASE_OBJECT( account_object );
    public:
      //constructor for creation of regular accounts
      template< typename Allocator >
      account_object( allocator< Allocator > a, uint64_t _id,
        const account_name_type& _name, const public_key_type& _memo_key,
        const time_point_sec& _creation_time, bool _mined,
        const account_object* _recovery_account,
        bool _fill_mana, const asset& incoming_delegation, int64_t _rc_adjustment = 0 )
      : id( _id ), name( _name ), rc_adjustment( _rc_adjustment ), created( _creation_time ),
        mined( _mined ), memo_key( _memo_key ), delayed_votes( a )
      {
        if( _recovery_account != nullptr )
          recovery_account = _recovery_account->get_id();
        received_vesting_shares += incoming_delegation;
        voting_manabar.last_update_time = _creation_time.sec_since_epoch();
        downvote_manabar.last_update_time = _creation_time.sec_since_epoch();
        if( _fill_mana )
          voting_manabar.current_mana = HIVE_100_PERCENT; //nonsense, but that's what was in the original code
        if( rc_adjustment.value )
        {
          rc_manabar.last_update_time = _creation_time.sec_since_epoch();
          auto max_rc = get_maximum_rc().value;
          rc_manabar.current_mana = max_rc;
          if( _name == "tan.dev" )
            ilog( "RC bug: ${m}, creation", ( "m", max_rc ) );
          last_max_rc = max_rc;
        }
      }

      //minimal constructor used for creation of accounts at genesis and in tests
      template< typename Allocator >
      account_object( allocator< Allocator > a, uint64_t _id,
        const account_name_type& _name, const time_point_sec& _creation_time, const public_key_type& _memo_key = public_key_type() )
        : id( _id ), name( _name ), created( _creation_time ), memo_key( _memo_key ), delayed_votes( a )
      {}

      //liquid HIVE balance
      const HIVE_asset& get_balance() const { return balance; }
      //HIVE balance in savings
      const HIVE_asset& get_savings() const { return savings_balance; }
      //unclaimed HIVE rewards
      const HIVE_asset& get_rewards() const { return reward_hive_balance; }

      //liquid HBD balance
      const HBD_asset& get_hbd_balance() const { return hbd_balance; }
      //HBD balance in savings
      const HBD_asset& get_hbd_savings() const { return savings_hbd_balance; }
      //unclaimed HBD rewards
      const HBD_asset& get_hbd_rewards() const { return reward_hbd_balance; }

      //all VESTS held by the account - use other routines to get active VESTS for specific uses
      const VEST_asset& get_vesting() const { return vesting_shares; }
      //VESTS that were delegated to other accounts
      const VEST_asset& get_delegated_vesting() const { return delegated_vesting_shares; }
      //VESTS that were borrowed from other accounts
      const VEST_asset& get_received_vesting() const { return received_vesting_shares; }
      //whole remainder of active power down (zero when not active)
      share_type get_total_vesting_withdrawal() const { return to_withdraw.amount - withdrawn.amount; }
      //value of active step of pending power down (or zero)
      share_type get_next_vesting_withdrawal() const
      {
        if( next_vesting_withdrawal != fc::time_point_sec::maximum() )
          return std::min( vesting_withdraw_rate.amount, get_total_vesting_withdrawal() );
        else
          return 0;
      }
      //effective balance of VESTS including delegations and optionally excluding active step of pending power down
      share_type get_effective_vesting_shares( bool excludeWeeklyPowerDown = true ) const
      {
        share_type total = vesting_shares.amount - delegated_vesting_shares.amount + received_vesting_shares.amount;
        if( excludeWeeklyPowerDown && next_vesting_withdrawal != fc::time_point_sec::maximum() )
          total -= get_next_vesting_withdrawal();
        return total;
      }
      //unclaimed VESTS rewards
      const VEST_asset& get_vest_rewards() const { return reward_vesting_balance; }
      //value of unclaimed VESTS rewards in HIVE (HIVE held on global balance)
      const HIVE_asset& get_vest_rewards_as_hive() const { return reward_vesting_hive; }

      //effective balance of VESTS for RC calculation optionally excluding part that cannot be delegated
      share_type get_maximum_rc( bool only_delegable = false ) const
      {
        share_type total = get_effective_vesting_shares() - delegated_rc;
        if( only_delegable == false )
          total += rc_adjustment + received_rc;
        return total;
      }
      //RC compensation for account creation fee
      share_type get_rc_adjustment() const { return rc_adjustment; }
      //RC that were delegated to other accounts
      share_type get_delegated_rc() const { return delegated_rc; }
      //RC that were borrowed from other accounts
      share_type get_received_rc() const { return received_rc; }

      //gives name of the account
      const account_name_type& get_name() const { return name; }
      //account creation time
      time_point_sec get_creation_time() const { return created; }
      //tells if account was created through pow/pow2 mining operation or is one of builtin accounts created during genesis
      bool was_mined() const { return mined; }

      //tells if account has some other account casting governance votes in its name
      bool has_proxy() const { return proxy != account_id_type(); }
      //account's proxy (if any)
      account_id_type get_proxy() const { return proxy; }
      //sets proxy to neutral (account will vote for itself)
      void clear_proxy() { proxy = account_id_type(); }
      //sets proxy to given account
      void set_proxy(const account_object& new_proxy)
      {
        FC_ASSERT( &new_proxy != this );
        proxy = new_proxy.get_id();
      }

      //tells if account has some designated account that can initiate recovery (if not, top witness can)
      bool has_recovery_account() const { return recovery_account != account_id_type(); }
      //account's recovery account (if any), that is, an account that can authorize request_account_recovery_operation
      account_id_type get_recovery_account() const { return recovery_account; }
      //sets new recovery account
      void set_recovery_account(const account_object& new_recovery_account)
      {
        recovery_account = new_recovery_account.get_id();
      }
      //timestamp of last time account owner authority was successfully recovered
      time_point_sec get_last_account_recovery_time() const { return last_account_recovery; }
      //sets time of owner authority recovery
      void set_last_account_recovery_time( time_point_sec recovery_time )
      {
        last_account_recovery = recovery_time;
      }

      //members are organized in such a way that the object takes up as little space as possible (note that object starts with 4byte id).

    private:
      account_id_type   proxy;

      account_id_type   recovery_account;
      time_point_sec    last_account_recovery;

      account_name_type name;

    public:
      uint128_t         hbd_seconds = 0; ///< liquid HBD * how long it has been held
      uint128_t         savings_hbd_seconds = 0; ///< savings HBD * how long it has been held

      util::manabar     voting_manabar;
      util::manabar     downvote_manabar;
      util::manabar     rc_manabar;

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

      share_type        rc_adjustment; ///< compensation for account creation fee in form of extra RC
      share_type        delegated_rc; ///< RC delegated out to other accounts
      share_type        received_rc; ///< RC delegated to this account
      share_type        last_max_rc; ///< (for bug catching with RC code, can be removed once RC becomes part of consensus)

      share_type        pending_claimed_accounts = 0; ///< claimed and not yet used account creation tokens (could be 32bit)

      ushare_type       sum_delayed_votes = 0; ///< sum of delayed_votes (should be changed to VEST_asset)

      time_point_sec    hbd_seconds_last_update; ///< the last time the hbd_seconds was updated
      time_point_sec    hbd_last_interest_payment; ///< used to pay interest at most once per month
      time_point_sec    savings_hbd_seconds_last_update; ///< the last time the hbd_seconds was updated
      time_point_sec    savings_hbd_last_interest_payment; ///< used to pay interest at most once per month
    private:
      time_point_sec    created; //(not read by consensus code)
    public:
      time_point_sec    last_account_update; //(only used by outdated consensus checks - up to HF17)
      time_point_sec    last_post; //(we could probably remove limit on posting replies)
      time_point_sec    last_root_post; //influenced root comment reward between HF12 and HF17
      time_point_sec    last_post_edit; //(we could probably remove limit on post edits)
      time_point_sec    last_vote_time; //(only used by outdated consensus checks - up to HF26)
      time_point_sec    next_vesting_withdrawal = fc::time_point_sec::maximum(); ///< after every withdrawal this is incremented by 1 week

    private:
      time_point_sec    governance_vote_expiration_ts = fc::time_point_sec::maximum();

    public:
      uint32_t          post_count = 0; //(not read by consensus code)
      uint32_t          post_bandwidth = 0; //influenced root comment reward between HF12 and HF17

      uint16_t          withdraw_routes = 0; //max 10, why is it 16bit?
      uint16_t          pending_escrow_transfers = 0; //for now max is 255, but it might change
      uint16_t          open_recurrent_transfers = 0; //for now max is 255, but it might change
      uint16_t          witnesses_voted_for = 0; //max 30, why is it 16bit?

      uint8_t           savings_withdraw_requests = 0;
      bool              can_vote = true;
    private:
      bool              mined = true; //(not read by consensus code)

    public:
      public_key_type   memo_key; //33 bytes with alignment of 1; (it belongs to metadata as it is not used by consensus, but witnesses need it here since they don't COLLECT_ACCOUNT_METADATA)

      fc::array<share_type, HIVE_MAX_PROXY_RECURSION_DEPTH> proxied_vsf_votes; ///< the total VFS votes proxied to this account

      using t_delayed_votes = t_vector< delayed_votes_data >;
      /*
        Holds sum of VESTS per day.
        VESTS from day `X` will be matured after `X` + 30 days ( because `HIVE_DELAYED_VOTING_TOTAL_INTERVAL_SECONDS` == 30 days )
      */
      t_delayed_votes   delayed_votes;

      //methods

      time_point_sec get_governance_vote_expiration_ts() const
      {
        return governance_vote_expiration_ts;
      }

      void set_governance_vote_expired()
      {
        governance_vote_expiration_ts = time_point_sec::maximum();
      }

      void update_governance_vote_expiration_ts(const time_point_sec vote_time)
      {
        governance_vote_expiration_ts = vote_time + HIVE_GOVERNANCE_VOTE_EXPIRATION_PERIOD;
        if (governance_vote_expiration_ts < HARDFORK_1_25_FIRST_GOVERNANCE_VOTE_EXPIRE_TIMESTAMP)
        {
          const int64_t DIVIDER = HIVE_HARDFORK_1_25_MAX_OLD_GOVERNANCE_VOTE_EXPIRE_SHIFT.to_seconds();
          governance_vote_expiration_ts = HARDFORK_1_25_FIRST_GOVERNANCE_VOTE_EXPIRE_TIMESTAMP + fc::seconds(governance_vote_expiration_ts.sec_since_epoch() % DIVIDER);
        }
      }

      bool has_delayed_votes() const { return !delayed_votes.empty(); }

      // start time of oldest delayed vote bucket (the one closest to activation)
      time_point_sec get_oldest_delayed_vote_time() const
      {
        if( has_delayed_votes() )
          return ( delayed_votes.begin() )->time;
        else
          return time_point_sec::maximum();
      }

      // governance vote power of this account does not include "delayed votes"
      share_type get_direct_governance_vote_power() const
      {
        FC_ASSERT( sum_delayed_votes.value <= vesting_shares.amount, "",
                ( "sum_delayed_votes",     sum_delayed_votes )
                ( "vesting_shares.amount", vesting_shares.amount )
                ( "account",               name ) );
  
        return asset( vesting_shares.amount - sum_delayed_votes.value, VESTS_SYMBOL ).amount;
      }

      /// This function should be used only when the account votes for a witness directly
      share_type get_governance_vote_power() const
      {
        return std::accumulate( proxied_vsf_votes.begin(), proxied_vsf_votes.end(),
          get_direct_governance_vote_power() );
      }
      share_type proxied_vsf_votes_total() const
      {
        return std::accumulate( proxied_vsf_votes.begin(), proxied_vsf_votes.end(),
          share_type() );
      }

#ifdef IS_TEST_NET
      //ABW: it is needed for some low level tests (they would need to be rewritten)
      void set_name( const account_name_type& new_name ) { name = new_name; }
#endif

    CHAINBASE_UNPACK_CONSTRUCTOR(account_object, (delayed_votes));
  };

  class account_metadata_object : public object< account_metadata_object_type, account_metadata_object >
  {
    CHAINBASE_OBJECT( account_metadata_object );
    public:
      CHAINBASE_DEFAULT_CONSTRUCTOR( account_metadata_object, (json_metadata)(posting_json_metadata) )

      account_id_type   account;
      shared_string     json_metadata;
      shared_string     posting_json_metadata;

    CHAINBASE_UNPACK_CONSTRUCTOR(account_metadata_object, (json_metadata)(posting_json_metadata));
  };

  class account_authority_object : public object< account_authority_object_type, account_authority_object >
  {
    CHAINBASE_OBJECT( account_authority_object );
    public:
      CHAINBASE_DEFAULT_CONSTRUCTOR( account_authority_object, (owner)(active)(posting) )

      account_name_type account;

      shared_authority  owner;   ///< used for backup control, can set owner or active
      shared_authority  active;  ///< used for all monetary operations, can set active or posting
      shared_authority  posting; ///< used for voting and posting

      time_point_sec    previous_owner_update;
      time_point_sec    last_owner_update;

    CHAINBASE_UNPACK_CONSTRUCTOR(account_authority_object, (owner)(active)(posting));
  };

  class vesting_delegation_object : public object< vesting_delegation_object_type, vesting_delegation_object >
  {
    CHAINBASE_OBJECT( vesting_delegation_object );
    public:
      template< typename Allocator >
      vesting_delegation_object( allocator< Allocator > a, uint64_t _id,
        const account_object& _from, const account_object& _to,
        const VEST_asset& _amount, const time_point_sec& _min_delegation_time )
      : id( _id ), delegator( _from.get_id() ), delegatee( _to.get_id() ),
        min_delegation_time( _min_delegation_time ), vesting_shares( _amount )
      {}

      //id of "delegation sender"
      account_id_type get_delegator() const { return delegator; }
      //id of "delegation receiver"
      account_id_type get_delegatee() const { return delegatee; }
      //amount of delegated VESTS
      const VEST_asset& get_vesting() const { return vesting_shares; }
      //minimal time when delegation will not be returned to the delegator (can be taken from delegatee though)
      time_point_sec get_min_delegation_time() const { return min_delegation_time; }

      void set_vesting( const VEST_asset& _new_amount )
      {
        FC_ASSERT( _new_amount.amount > 0 );
        vesting_shares = _new_amount;
      }

    private:
      account_id_type delegator;
      account_id_type delegatee;
      time_point_sec  min_delegation_time;
      VEST_asset      vesting_shares;

    CHAINBASE_UNPACK_CONSTRUCTOR(vesting_delegation_object);
  };

  class vesting_delegation_expiration_object : public object< vesting_delegation_expiration_object_type, vesting_delegation_expiration_object >
  {
    CHAINBASE_OBJECT( vesting_delegation_expiration_object );
    public:
      template< typename Allocator >
      vesting_delegation_expiration_object( allocator< Allocator > a, uint64_t _id,
        const account_object& _delegator, const VEST_asset& _amount, const time_point_sec& _expiration )
      : id( _id ), delegator( _delegator.get_id() ), vesting_shares( _amount ), expiration( _expiration )
      {}

      //id of "delegation sender" where VESTs are to be returned
      account_id_type get_delegator() const { return delegator; }
      //amount of expiring delegated VESTs
      const VEST_asset& get_vesting() const { return vesting_shares; }
      //time when VESTs are to be returned to delegator
      time_point_sec get_expiration_time() const { return expiration; }

    private:
      account_id_type delegator;
      VEST_asset      vesting_shares;
      time_point_sec  expiration;

    CHAINBASE_UNPACK_CONSTRUCTOR(vesting_delegation_expiration_object);
  };

  class owner_authority_history_object : public object< owner_authority_history_object_type, owner_authority_history_object >
  {
    CHAINBASE_OBJECT( owner_authority_history_object );
    public:
      template< typename Allocator >
      owner_authority_history_object( allocator< Allocator > a, uint64_t _id,
        const account_object& _account, const shared_authority& _previous_owner, const time_point_sec& _creation_time )
        : id( _id ), account( _account.get_name() ), previous_owner_authority( allocator< shared_authority >( a ) ),
        last_valid_time( _creation_time )
      {
        previous_owner_authority = _previous_owner;
      }

      account_name_type account;
      shared_authority  previous_owner_authority;
      time_point_sec    last_valid_time;
    CHAINBASE_UNPACK_CONSTRUCTOR(owner_authority_history_object, (previous_owner_authority));
  };

  class account_recovery_request_object : public object< account_recovery_request_object_type, account_recovery_request_object >
  {
    CHAINBASE_OBJECT( account_recovery_request_object );
    public:
      template< typename Allocator >
      account_recovery_request_object( allocator< Allocator > a, uint64_t _id,
        const account_object& _account_to_recover, const authority& _new_owner_authority, const time_point_sec& _expiration_time )
        : id( _id ), expires( _expiration_time ), account_to_recover( _account_to_recover.get_name() ),
        new_owner_authority( allocator< shared_authority >( a ) )
      {
        new_owner_authority = _new_owner_authority;
      }

      //account whos owner authority is being modified
      const account_name_type& get_account_to_recover() const { return account_to_recover; }

      //new owner authority requested to be set during recovery operation
      const shared_authority& get_new_owner_authority() const { return new_owner_authority; }
      //sets different new owner authority (also moves time when request will expire)
      void set_new_owner_authority( const authority& _new_owner_authority, const time_point_sec& _new_expiration_time )
      {
        new_owner_authority = _new_owner_authority;
        expires = _new_expiration_time;
      }

      //time when the request will be automatically removed if not used
      time_point_sec get_expiration_time() const { return expires; }

    private:
      time_point_sec    expires;
      account_name_type account_to_recover;
      shared_authority  new_owner_authority;
      
    CHAINBASE_UNPACK_CONSTRUCTOR(account_recovery_request_object, (new_owner_authority));
  };

  class change_recovery_account_request_object : public object< change_recovery_account_request_object_type, change_recovery_account_request_object >
  {
    CHAINBASE_OBJECT( change_recovery_account_request_object );
    public:
      template< typename Allocator >
      change_recovery_account_request_object( allocator< Allocator > a, uint64_t _id,
        const account_object& _account_to_recover, const account_object& _recovery_account, const time_point_sec& _effect_time )
        : id( _id ), effective_on( _effect_time ), account_to_recover( _account_to_recover.get_name() ), recovery_account( _recovery_account.get_name() )
      {}

      //account whos recovery account is being modified
      const account_name_type& get_account_to_recover() const { return account_to_recover; }

      //new recovery account being set
      const account_name_type& get_recovery_account() const { return recovery_account; }
      //sets different new recovery account (also moves time when actual change will take place)
      void set_recovery_account( const account_object& new_recovery_account, const time_point_sec& _new_effect_time )
      {
        recovery_account = new_recovery_account.get_name();
        effective_on = _new_effect_time;
      }

      //time when actual change of recovery account is to be executed
      time_point_sec get_execution_time() const { return effective_on; }

    private:
      time_point_sec    effective_on;
      account_name_type account_to_recover; //changing it to id would influence response from database_api.list_change_recovery_account_requests
      account_name_type recovery_account; //could be changed to id
      
    CHAINBASE_UNPACK_CONSTRUCTOR(change_recovery_account_request_object);
  };

  struct by_proxy;
  struct by_next_vesting_withdrawal;
  struct by_delayed_voting;
  struct by_governance_vote_expiration_ts;
  /**
    * @ingroup object_index
    */
  typedef multi_index_container<
    account_object,
    indexed_by<
      ordered_unique< tag< by_id >,
        const_mem_fun< account_object, account_object::id_type, &account_object::get_id > >,
      ordered_unique< tag< by_name >,
        const_mem_fun< account_object, const account_name_type&, &account_object::get_name > >,
      ordered_unique< tag< by_proxy >,
        composite_key< account_object,
          const_mem_fun< account_object, account_id_type, &account_object::get_proxy >,
          const_mem_fun< account_object, const account_name_type&, &account_object::get_name >
        > /// composite key by proxy
      >,
      ordered_unique< tag< by_next_vesting_withdrawal >,
        composite_key< account_object,
          member< account_object, time_point_sec, &account_object::next_vesting_withdrawal >,
          const_mem_fun< account_object, const account_name_type&, &account_object::get_name >
        > /// composite key by_next_vesting_withdrawal
      >,
      ordered_unique< tag< by_delayed_voting >,
        composite_key< account_object,
          const_mem_fun< account_object, time_point_sec, &account_object::get_oldest_delayed_vote_time >,
          const_mem_fun< account_object, account_object::id_type, &account_object::get_id >
        >
      >,
      ordered_unique< tag< by_governance_vote_expiration_ts >,
        composite_key< account_object,
          const_mem_fun< account_object, time_point_sec, &account_object::get_governance_vote_expiration_ts >,
          const_mem_fun< account_object, account_object::id_type, &account_object::get_id >
        >
      >
    >,
    allocator< account_object >
  > account_index;

  struct by_account;

  typedef multi_index_container <
    account_metadata_object,
    indexed_by<
      ordered_unique< tag< by_id >,
        const_mem_fun< account_metadata_object, account_metadata_object::id_type, &account_metadata_object::get_id > >,
      ordered_unique< tag< by_account >,
        member< account_metadata_object, account_id_type, &account_metadata_object::account > >
    >,
    allocator< account_metadata_object >
  > account_metadata_index;

  typedef multi_index_container <
    owner_authority_history_object,
    indexed_by <
      ordered_unique< tag< by_id >,
        const_mem_fun< owner_authority_history_object, owner_authority_history_object::id_type, &owner_authority_history_object::get_id > >,
      ordered_unique< tag< by_account >,
        composite_key< owner_authority_history_object,
          member< owner_authority_history_object, account_name_type, &owner_authority_history_object::account >,
          member< owner_authority_history_object, time_point_sec, &owner_authority_history_object::last_valid_time >,
          const_mem_fun< owner_authority_history_object, owner_authority_history_object::id_type, &owner_authority_history_object::get_id >
        >,
        composite_key_compare< std::less< account_name_type >, std::less< time_point_sec >, std::less< owner_authority_history_id_type > >
      >
    >,
    allocator< owner_authority_history_object >
  > owner_authority_history_index;

  typedef multi_index_container <
    account_authority_object,
    indexed_by <
      ordered_unique< tag< by_id >,
        const_mem_fun< account_authority_object, account_authority_object::id_type, &account_authority_object::get_id > >,
      ordered_unique< tag< by_account >,
        composite_key< account_authority_object,
          member< account_authority_object, account_name_type, &account_authority_object::account >,
          const_mem_fun< account_authority_object, account_authority_object::id_type, &account_authority_object::get_id >
        >,
        composite_key_compare< std::less< account_name_type >, std::less< account_authority_id_type > >
      >
    >,
    allocator< account_authority_object >
  > account_authority_index;

  struct by_delegation;

  typedef multi_index_container <
    vesting_delegation_object,
    indexed_by <
      ordered_unique< tag< by_id >,
        const_mem_fun< vesting_delegation_object, vesting_delegation_object::id_type, &vesting_delegation_object::get_id > >,
      ordered_unique< tag< by_delegation >,
        composite_key< vesting_delegation_object,
          const_mem_fun< vesting_delegation_object, account_id_type, &vesting_delegation_object::get_delegator >,
          const_mem_fun< vesting_delegation_object, account_id_type, &vesting_delegation_object::get_delegatee >
        >
      >
    >,
    allocator< vesting_delegation_object >
  > vesting_delegation_index;

  struct by_expiration;
  struct by_account_expiration;

  typedef multi_index_container <
    vesting_delegation_expiration_object,
    indexed_by <
      ordered_unique< tag< by_id >,
        const_mem_fun< vesting_delegation_expiration_object, vesting_delegation_expiration_object::id_type, &vesting_delegation_expiration_object::get_id > >,
      ordered_unique< tag< by_expiration >,
        composite_key< vesting_delegation_expiration_object,
          const_mem_fun< vesting_delegation_expiration_object, time_point_sec, &vesting_delegation_expiration_object::get_expiration_time >,
          const_mem_fun< vesting_delegation_expiration_object, vesting_delegation_expiration_object::id_type, &vesting_delegation_expiration_object::get_id >
        >
      >,
      ordered_unique< tag< by_account_expiration >,
        composite_key< vesting_delegation_expiration_object,
          const_mem_fun< vesting_delegation_expiration_object, account_id_type, &vesting_delegation_expiration_object::get_delegator >,
          const_mem_fun< vesting_delegation_expiration_object, time_point_sec, &vesting_delegation_expiration_object::get_expiration_time >,
          const_mem_fun< vesting_delegation_expiration_object, vesting_delegation_expiration_object::id_type, &vesting_delegation_expiration_object::get_id >
        >
      >
    >,
    allocator< vesting_delegation_expiration_object >
  > vesting_delegation_expiration_index;

  struct by_expiration;

  typedef multi_index_container <
    account_recovery_request_object,
    indexed_by <
      ordered_unique< tag< by_id >,
        const_mem_fun< account_recovery_request_object, account_recovery_request_object::id_type, &account_recovery_request_object::get_id > >,
      ordered_unique< tag< by_account >,
        const_mem_fun< account_recovery_request_object, const account_name_type&, &account_recovery_request_object::get_account_to_recover >
      >,
      ordered_unique< tag< by_expiration >,
        composite_key< account_recovery_request_object,
          const_mem_fun< account_recovery_request_object, time_point_sec, &account_recovery_request_object::get_expiration_time >,
          const_mem_fun< account_recovery_request_object, const account_name_type&, &account_recovery_request_object::get_account_to_recover >
        >,
        composite_key_compare< std::less< time_point_sec >, std::less< const account_name_type& > >
      >
    >,
    allocator< account_recovery_request_object >
  > account_recovery_request_index;

  struct by_effective_date;

  typedef multi_index_container <
    change_recovery_account_request_object,
    indexed_by <
      ordered_unique< tag< by_id >,
        const_mem_fun< change_recovery_account_request_object, change_recovery_account_request_object::id_type, &change_recovery_account_request_object::get_id > >,
      ordered_unique< tag< by_account >,
        const_mem_fun< change_recovery_account_request_object, const account_name_type&, &change_recovery_account_request_object::get_account_to_recover >
      >,
      ordered_unique< tag< by_effective_date >,
        composite_key< change_recovery_account_request_object,
          const_mem_fun< change_recovery_account_request_object, time_point_sec, &change_recovery_account_request_object::get_execution_time >,
          const_mem_fun< change_recovery_account_request_object, const account_name_type&, &change_recovery_account_request_object::get_account_to_recover >
        >,
        composite_key_compare< std::less< time_point_sec >, std::less< const account_name_type& > >
      >
    >,
    allocator< change_recovery_account_request_object >
  > change_recovery_account_request_index;
} }

FC_REFLECT( hive::chain::account_object,
          (id)(proxy)(recovery_account)(last_account_recovery)
          (name)
          (hbd_seconds)
          (savings_hbd_seconds)
          (voting_manabar)
          (downvote_manabar)
          (rc_manabar)
          (hbd_balance)(savings_hbd_balance)
          (reward_hbd_balance)(reward_hive_balance)
          (reward_vesting_hive)(balance)
          (savings_balance)(reward_vesting_balance)
          (vesting_shares)(delegated_vesting_shares)
          (received_vesting_shares)(vesting_withdraw_rate)
          (curation_rewards)(posting_rewards)
          (withdrawn)(to_withdraw)
          (rc_adjustment)(delegated_rc)
          (received_rc)(last_max_rc)
          (pending_claimed_accounts)(sum_delayed_votes)
          (hbd_seconds_last_update)(hbd_last_interest_payment)(savings_hbd_seconds_last_update)(savings_hbd_last_interest_payment)
          (created)(last_account_update)(last_post)(last_root_post)
          (last_post_edit)(last_vote_time)(next_vesting_withdrawal)(governance_vote_expiration_ts)
          (post_count)(post_bandwidth)(withdraw_routes)(pending_escrow_transfers)(open_recurrent_transfers)(witnesses_voted_for)
          (savings_withdraw_requests)(can_vote)(mined)
          (memo_key)
          (proxied_vsf_votes)
          (delayed_votes)
        )

CHAINBASE_SET_INDEX_TYPE( hive::chain::account_object, hive::chain::account_index )

FC_REFLECT( hive::chain::account_metadata_object,
          (id)(account)(json_metadata)(posting_json_metadata) )
CHAINBASE_SET_INDEX_TYPE( hive::chain::account_metadata_object, hive::chain::account_metadata_index )

FC_REFLECT( hive::chain::account_authority_object,
          (id)(account)(owner)(active)(posting)(previous_owner_update)(last_owner_update)
)
CHAINBASE_SET_INDEX_TYPE( hive::chain::account_authority_object, hive::chain::account_authority_index )

FC_REFLECT( hive::chain::vesting_delegation_object,
        (id)(delegator)(delegatee)(min_delegation_time)(vesting_shares) )
CHAINBASE_SET_INDEX_TYPE( hive::chain::vesting_delegation_object, hive::chain::vesting_delegation_index )

FC_REFLECT( hive::chain::vesting_delegation_expiration_object,
        (id)(delegator)(vesting_shares)(expiration) )
CHAINBASE_SET_INDEX_TYPE( hive::chain::vesting_delegation_expiration_object, hive::chain::vesting_delegation_expiration_index )

FC_REFLECT( hive::chain::owner_authority_history_object,
          (id)(account)(previous_owner_authority)(last_valid_time)
        )
CHAINBASE_SET_INDEX_TYPE( hive::chain::owner_authority_history_object, hive::chain::owner_authority_history_index )

FC_REFLECT( hive::chain::account_recovery_request_object,
          (id)(account_to_recover)(new_owner_authority)(expires)
        )
CHAINBASE_SET_INDEX_TYPE( hive::chain::account_recovery_request_object, hive::chain::account_recovery_request_index )

FC_REFLECT( hive::chain::change_recovery_account_request_object,
          (id)(account_to_recover)(recovery_account)(effective_on)
        )
CHAINBASE_SET_INDEX_TYPE( hive::chain::change_recovery_account_request_object, hive::chain::change_recovery_account_request_index )

namespace helpers
{
  template <>
  class index_statistic_provider<hive::chain::account_index>
  {
  public:
    typedef hive::chain::account_index IndexType;
    typedef typename hive::chain::account_object::t_delayed_votes t_delayed_votes;

    index_statistic_info gather_statistics(const IndexType& index, bool onlyStaticInfo) const
    {
      index_statistic_info info;
      gather_index_static_data(index, &info);

      if(onlyStaticInfo == false)
      {
        for(const auto& o : index)
        {
          info._item_additional_allocation += o.delayed_votes.capacity()*sizeof(t_delayed_votes::value_type);
        }
      }

      return info;
    }
  };

} /// namespace helpers
