#pragma once
#include <fc/uint128.hpp>

#include <hive/chain/hive_fwd.hpp>

#include <hive/protocol/fixed_string.hpp>
#include <hive/protocol/authority.hpp>
#include <hive/protocol/hive_operations.hpp>
#include <hive/chain/account_details.hpp>

#include <hive/chain/hive_object_types.hpp>
#include <hive/chain/witness_objects.hpp>
#include <hive/chain/shared_authority.hpp>

#include <hive/chain/util/delayed_voting_processor.hpp>

#include <numeric>

namespace hive { namespace chain {

  using chainbase::t_vector;
  using hive::protocol::authority;

  class account_object : public object< account_object_type, account_object, std::true_type >
  {
    CHAINBASE_OBJECT( account_object );

    private:

      account_details::recovery recovery;

    public:

      //tells if account has some designated account that can initiate recovery (if not, top witness can)
      bool has_recovery_account() const { return recovery.recovery_account != account_id_type(); }

      //account's recovery account (if any), that is, an account that can authorize request_account_recovery_operation
      account_id_type get_recovery_account() const { return recovery.recovery_account; }

      //sets new recovery account
      void set_recovery_account(const account_id_type& new_recovery_account)
      {
        recovery.recovery_account = new_recovery_account;
      }

      //timestamp of last time account owner authority was successfully recovered
      time_point_sec get_last_account_recovery_time() const { return recovery.last_account_recovery; }

      //sets time of owner authority recovery
      void set_last_account_recovery_time( time_point_sec recovery_time )
      {
        recovery.last_account_recovery = recovery_time;
      }

      //time from a current block
      time_point_sec get_block_last_account_recovery_time() const { return recovery.block_last_account_recovery; }

      void set_block_last_account_recovery_time( time_point_sec block_recovery_time )
      {
        recovery.block_last_account_recovery = block_recovery_time;
      }

    private:

      account_details::assets assets;

    public:

      //liquid HIVE balance
      const HIVE_asset& get_balance() const { return assets.balance; }
      void set_balance( const HIVE_asset& value ) { assets.balance = value; }
      //HIVE balance in savings
      const HIVE_asset& get_savings() const { return assets.savings_balance; }
      void set_savings( const HIVE_asset& value ) { assets.savings_balance = value; }
      //unclaimed HIVE rewards
      const HIVE_asset& get_rewards() const { return assets.reward_hive_balance; }
      const void set_rewards( const HIVE_asset& value ) { assets.reward_hive_balance = value; }

      //liquid HBD balance
      const HBD_asset& get_hbd_balance() const { return assets.hbd_balance; }
      void set_hbd_balance( const HBD_asset& value ) { assets.hbd_balance = value; }
      //HBD balance in savings
      const HBD_asset& get_hbd_savings() const { return assets.savings_hbd_balance; }
      const void set_hbd_savings( const HBD_asset& value ) { assets.savings_hbd_balance = value; }
      //unclaimed HBD rewards
      const HBD_asset& get_hbd_rewards() const { return assets.reward_hbd_balance; }
      const void set_hbd_rewards( const HBD_asset& value ) { assets.reward_hbd_balance = value; }

      //all VESTS held by the account - use other routines to get active VESTS for specific uses
      const VEST_asset& get_vesting() const { return assets.vesting_shares; }
      void set_vesting( const VEST_asset& value ) { assets.vesting_shares = value; }

      //VESTS that were delegated to other accounts
      const VEST_asset& get_delegated_vesting() const { return assets.delegated_vesting_shares; }
      const void set_delegated_vesting( const VEST_asset& value ) { assets.delegated_vesting_shares = value; }
      //VESTS that were borrowed from other accounts
      const VEST_asset& get_received_vesting() const { return assets.received_vesting_shares; }
      const void set_received_vesting( const VEST_asset& value ) { assets.received_vesting_shares = value; }
      //whole remainder of active power down (zero when not active)
      share_type get_total_vesting_withdrawal() const { return assets.to_withdraw.amount - assets.withdrawn.amount; }
      const VEST_asset& get_vesting_withdraw_rate() const { return assets.vesting_withdraw_rate; }
      const void set_vesting_withdraw_rate( const VEST_asset& value ) { assets.vesting_withdraw_rate = value; }

      //unclaimed VESTS rewards
      const VEST_asset& get_vest_rewards() const { return assets.reward_vesting_balance; }
      const void set_vest_rewards( const VEST_asset& value ) { assets.reward_vesting_balance = value; }
      //value of unclaimed VESTS rewards in HIVE (HIVE held on global balance)
      const HIVE_asset& get_vest_rewards_as_hive() const { return assets.reward_vesting_hive; }
      const void set_vest_rewards_as_hive( const HIVE_asset& value ) { assets.reward_vesting_hive = value; }

      const HIVE_asset& get_curation_rewards() const { return assets.curation_rewards; }
      void set_curation_rewards( const HIVE_asset& value ) { assets.curation_rewards = value; }
      const HIVE_asset& get_posting_rewards() const { return assets.posting_rewards; }
      void set_posting_rewards( const HIVE_asset& value ) { assets.posting_rewards = value; }

      const VEST_asset& get_withdrawn() const { return assets.withdrawn; }
      const void set_withdrawn( const VEST_asset& value ) { assets.withdrawn = value; }
      const VEST_asset& get_to_withdraw() const { return assets.to_withdraw; }
      const void set_to_withdraw( const VEST_asset& value ) { assets.to_withdraw = value; }

    private:

      account_details::manabars_rc mrc;

    public:

      share_type  get_rc_adjustment() const { return mrc.rc_adjustment; }
      void set_rc_adjustment( const share_type& value ) { mrc.rc_adjustment = value; }

      share_type  get_delegated_rc() const { return mrc.delegated_rc; }
      void set_delegated_rc( const share_type& value ) { mrc.delegated_rc = value; }

      share_type  get_received_rc() const { return mrc.received_rc; }
      void set_received_rc( const share_type& value ) { mrc.received_rc = value; }

      share_type  get_last_max_rc() const { return mrc.last_max_rc; }
      void set_last_max_rc( const share_type& value ) { mrc.last_max_rc = value; }

      //effective balance of VESTS for RC calculation optionally excluding part that cannot be delegated
      share_type get_maximum_rc( bool only_delegable = false ) const
      {
        return mrc.get_maximum_rc( get_effective_vesting_shares(), only_delegable );
      }

      util::manabar& get_rc_manabar() { return mrc.rc_manabar; };
      const util::manabar& get_rc_manabar() const { return mrc.rc_manabar; };

      util::manabar& get_voting_manabar() { return mrc.voting_manabar; };
      const util::manabar& get_voting_manabar() const { return mrc.voting_manabar; };

      util::manabar& get_downvote_manabar() { return mrc.downvote_manabar; };
      const util::manabar& get_downvote_manabar() const { return mrc.downvote_manabar; };

    private:

      account_details::time time;

    public:

      uint128_t get_hbd_seconds() const { return time.hbd_seconds; }
      void set_hbd_seconds( const uint128_t& value ) { time.hbd_seconds = value; }

      uint128_t get_savings_hbd_seconds() const { return time.savings_hbd_seconds; }
      void set_savings_hbd_seconds( const uint128_t& value ) { time.savings_hbd_seconds = value; }

      time_point_sec get_hbd_seconds_last_update() const { return time.hbd_seconds_last_update; }
      void set_hbd_seconds_last_update( const time_point_sec& value ) { time.hbd_seconds_last_update = value; }

      time_point_sec get_hbd_last_interest_payment() const { return time.hbd_last_interest_payment; }
      void set_hbd_last_interest_payment( const time_point_sec& value ) { time.hbd_last_interest_payment = value; }

      time_point_sec get_savings_hbd_seconds_last_update() const { return time.savings_hbd_seconds_last_update; }
      void set_savings_hbd_seconds_last_update( const time_point_sec& value ) { time.savings_hbd_seconds_last_update = value; }

      time_point_sec get_savings_hbd_last_interest_payment() const { return time.savings_hbd_last_interest_payment; }
      void set_savings_hbd_last_interest_payment( const time_point_sec& value ) { time.savings_hbd_last_interest_payment = value; }

      time_point_sec get_last_account_update() const { return time.last_account_update; }
      void set_last_account_update( const time_point_sec& value ) { time.last_account_update = value; }

      time_point_sec get_last_post() const { return time.last_post; }
      void set_last_post( const time_point_sec& value ) { time.last_post = value; }

      time_point_sec get_last_root_post() const { return time.last_root_post; }
      void set_last_root_post( const time_point_sec& value ) { time.last_root_post = value; }

      time_point_sec get_last_post_edit() const { return time.last_post_edit; }
      void set_last_post_edit( const time_point_sec& value ) { time.last_post_edit = value; }

      time_point_sec get_last_vote_time() const { return time.last_vote_time; }
      void set_last_vote_time( const time_point_sec& value ) { time.last_vote_time = value; }

      //tells if account has active power down
      bool has_active_power_down() const { return time.next_vesting_withdrawal != fc::time_point_sec::maximum(); }
      //value of active step of pending power down (or zero)
      share_type get_active_next_vesting_withdrawal() const
      {
        if( has_active_power_down() )
          return std::min( get_vesting_withdraw_rate().amount, get_total_vesting_withdrawal() );
        else
          return 0;
      }
      time_point_sec get_next_vesting_withdrawal() const { return time.next_vesting_withdrawal; }
      void set_next_vesting_withdrawal( const time_point_sec& value ) { time.next_vesting_withdrawal = value; }

    public:
      //constructor for creation of regular accounts
      template< typename Allocator >
      account_object( allocator< Allocator > a, uint64_t _id,
        const account_name_type& _name, const public_key_type& _memo_key,
        const time_point_sec& _creation_time, const time_point_sec& _block_creation_time, bool _mined,
        const account_object* _recovery_account,
        bool _fill_mana, const asset& incoming_delegation, int64_t _rc_adjustment = 0 )
      : id( _id ),
        recovery( _recovery_account ? _recovery_account->get_id() : account_id_type() ),
        assets( incoming_delegation ),
        mrc( _creation_time, _fill_mana, _rc_adjustment, get_effective_vesting_shares() ),
        time(),

        name( _name ), created( _creation_time ),
        block_created( _block_creation_time )/*_block_creation_time = time from a current block*/,
        mined( _mined ), memo_key( _memo_key ), delayed_votes( a )
      {
      }

      //minimal constructor used for creation of accounts at genesis and in tests
      template< typename Allocator >
      account_object( allocator< Allocator > a, uint64_t _id,
        const account_name_type& _name, const time_point_sec& _creation_time, const public_key_type& _memo_key = public_key_type() )
        : id( _id ), name( _name ), created( _creation_time ), block_created( _creation_time ), memo_key( _memo_key ), delayed_votes( a )
      {}

      //effective balance of VESTS including delegations and optionally excluding active step of pending power down
      share_type get_effective_vesting_shares( bool excludeWeeklyPowerDown = true ) const
      {
        share_type total = get_vesting().amount - get_delegated_vesting().amount + get_received_vesting().amount;
        if( excludeWeeklyPowerDown && time.next_vesting_withdrawal != fc::time_point_sec::maximum() )
          total -= get_active_next_vesting_withdrawal();
        return total;
      }

      //gives name of the account
      const account_name_type& get_name() const { return name; }
      //account creation time
      time_point_sec get_creation_time() const { return created; }
      //tells if account was created through pow/pow2 mining operation or is one of builtin accounts created during genesis
      //account creation time according to a block
      time_point_sec get_block_creation_time() const { return block_created; }
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

      //members are organized in such a way that the object takes up as little space as possible (note that object starts with 4byte id).

    private:
      account_id_type   proxy;

      account_name_type name;

    public:
      share_type        pending_claimed_accounts = 0; ///< claimed and not yet used account creation tokens (could be 32bit)

      ushare_type       sum_delayed_votes = 0; ///< sum of delayed_votes (should be changed to VEST_asset)

    private:
      time_point_sec    created; //(not read by consensus code)
      time_point_sec    block_created;
    public:

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
        FC_ASSERT( sum_delayed_votes.value <= get_vesting().amount, "",
                ( "sum_delayed_votes",     sum_delayed_votes )
                ( "vesting_shares.amount", get_vesting().amount )
                ( "account",               name ) );
  
        return asset( get_vesting().amount - sum_delayed_votes.value, VESTS_SYMBOL ).amount;
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

      size_t get_dynamic_alloc() const
      {
        size_t size = 0;
        size += delayed_votes.capacity() * sizeof( decltype( delayed_votes )::value_type );
        return size;
      }

    CHAINBASE_UNPACK_CONSTRUCTOR(account_object, (delayed_votes));
  };

  class account_metadata_object : public object< account_metadata_object_type, account_metadata_object, std::true_type >
  {
    CHAINBASE_OBJECT( account_metadata_object );
    public:
      CHAINBASE_DEFAULT_CONSTRUCTOR( account_metadata_object, (json_metadata)(posting_json_metadata) )

      template< typename Allocator >
      account_metadata_object( allocator< Allocator > a, account_metadata_id_type _id,
                                const std::string& account,
                                const std::string& json_metadata,
                                const std::string& posting_json_metadata )
      : id( _id ), account( account), json_metadata( a ), posting_json_metadata( a )
      {
        from_string( this->json_metadata, json_metadata );
        from_string( this->posting_json_metadata, posting_json_metadata );
      }

      account_name_type account;
      shared_string     json_metadata;
      shared_string     posting_json_metadata;

      size_t get_dynamic_alloc() const
      {
        size_t size = 0;
        size += json_metadata.capacity() * sizeof( decltype( json_metadata )::value_type );
        size += posting_json_metadata.capacity() * sizeof( decltype( posting_json_metadata )::value_type );
        return size;
      }

    CHAINBASE_UNPACK_CONSTRUCTOR(account_metadata_object, (json_metadata)(posting_json_metadata));
  };

  class account_authority_object : public object< account_authority_object_type, account_authority_object, std::true_type >
  {
    CHAINBASE_OBJECT( account_authority_object );
    public:
      CHAINBASE_DEFAULT_CONSTRUCTOR( account_authority_object, (owner)(active)(posting) )

      template< typename Allocator >
      account_authority_object( allocator< Allocator > a, account_authority_id_type _id,
                                const std::string& account,
                                const authority& owner,
                                const authority& active,
                                const authority& posting,
                                const time_point_sec& previous_owner_update,
                                const time_point_sec& last_owner_update )
      : id( _id ), account( account ), owner( a ), active( a ), posting( a ),
        previous_owner_update( previous_owner_update ), last_owner_update( last_owner_update )
      {
        this->owner = owner;
        this->active = active;
        this->posting = posting;
      }

      account_name_type account;

      shared_authority  owner;   ///< used for backup control, can set owner or active
      shared_authority  active;  ///< used for all monetary operations, can set active or posting
      shared_authority  posting; ///< used for voting and posting

      time_point_sec    previous_owner_update;
      time_point_sec    last_owner_update;

      size_t get_dynamic_alloc() const
      {
        size_t size = 0;
        size += owner.get_dynamic_alloc();
        size += active.get_dynamic_alloc();
        size += posting.get_dynamic_alloc();
        return size;
      }

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

  class owner_authority_history_object : public object< owner_authority_history_object_type, owner_authority_history_object, std::true_type >
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

      size_t get_dynamic_alloc() const
      {
        size_t size = 0;
        size += previous_owner_authority.get_dynamic_alloc();
        return size;
      }

    CHAINBASE_UNPACK_CONSTRUCTOR(owner_authority_history_object, (previous_owner_authority));
  };

  class account_recovery_request_object : public object< account_recovery_request_object_type, account_recovery_request_object, std::true_type >
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

    public:
      size_t get_dynamic_alloc() const
      {
        size_t size = 0;
        size += new_owner_authority.get_dynamic_alloc();
        return size;
      }
      
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
          const_mem_fun< account_object, time_point_sec, &account_object::get_next_vesting_withdrawal >,
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
    multi_index_allocator< account_object >
  > account_index;

  struct by_account {};

  typedef multi_index_container <
    account_metadata_object,
    indexed_by<
      ordered_unique< tag< by_id >,
        const_mem_fun< account_metadata_object, account_metadata_object::id_type, &account_metadata_object::get_id > >,
      ordered_unique< tag< by_account >,
        member< account_metadata_object, account_name_type, &account_metadata_object::account > >
    >,
    multi_index_allocator< account_metadata_object >
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
    multi_index_allocator< owner_authority_history_object >
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
    multi_index_allocator< account_authority_object >
  > account_authority_index;

  struct by_delegation {};

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
    multi_index_allocator< vesting_delegation_object >
  > vesting_delegation_index;

  struct by_expiration {};
  struct by_account_expiration {};

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
    multi_index_allocator< vesting_delegation_expiration_object >
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
    multi_index_allocator< account_recovery_request_object >
  > account_recovery_request_index;

  struct by_effective_date {};

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
    multi_index_allocator< change_recovery_account_request_object >
  > change_recovery_account_request_index;
} }

FC_REFLECT( hive::chain::account_object,
          (id)
          (recovery)(assets)(mrc)(time)
          (proxy)
          (name)
          (pending_claimed_accounts)(sum_delayed_votes)
          (created)(block_created)
          (governance_vote_expiration_ts)
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
