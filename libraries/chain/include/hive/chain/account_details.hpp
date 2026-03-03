#pragma once

#include <hive/protocol/asset.hpp>

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

  // NOTE: manabars_rc and time structs were removed — their fields were merged into assets_object
  // (commits 16 and 18). The assets_object now contains all balance, manabar, RC, and timestamp fields.

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

    time_point_sec    created; //(not read by consensus code)
    time_point_sec    block_created;

    time_point_sec    governance_vote_expiration_ts = fc::time_point_sec::maximum();

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

  struct shared_delayed_votes_wrapper
  {
    account_details::t_delayed_votes delayed_votes;

    template< typename Allocator >
    shared_delayed_votes_wrapper( allocator< Allocator > a ): delayed_votes( a )
    {
    }

    account_details::t_delayed_votes& get_delayed_votes() { return delayed_votes; }
    const account_details::t_delayed_votes& get_delayed_votes() const { return delayed_votes; }

    bool has_delayed_votes() const { return !delayed_votes.empty(); }

    // start time of oldest delayed vote bucket (the one closest to activation)
    time_point_sec get_oldest_delayed_vote_time() const
    {
      if( has_delayed_votes() )
        return ( delayed_votes.begin() )->time;
      else
        return time_point_sec::maximum();
    }

    private:

      template<typename DEST_COLLECTION_TYPE, typename SRC_COLLECTION_TYPE>
      static void copy( DEST_COLLECTION_TYPE& dst, const SRC_COLLECTION_TYPE& src )
      {
        dst.clear();
        if( src.size() )
        {
          dst.reserve( src.size() );
          for( const auto& item : src )
            dst.push_back( item );
        }
      }

    public:

      template<typename COLLECTION_TYPE>
      void clone( const COLLECTION_TYPE& src )
      {
        copy( delayed_votes, src );
      }

      template<typename COLLECTION_TYPE>
      static std::vector< delayed_votes_data > convert( const COLLECTION_TYPE& src )
      {
        std::vector< delayed_votes_data > _result;

        copy( _result, src );

        return _result;
      }

    size_t get_dynamic_alloc() const
    {
      size_t size = 0;
      size += delayed_votes.capacity() * sizeof( decltype( delayed_votes )::value_type );
      return size;
    }

  };

  struct shared_delayed_votes_wrapper_ex: public shared_delayed_votes_wrapper
  {
    template< typename Allocator >
    shared_delayed_votes_wrapper_ex( allocator< Allocator > a ): shared_delayed_votes_wrapper( a )
    {
    }

    ushare_type sum_delayed_votes = 0; ///< sum of delayed_votes (should be changed to VEST_asset)

    ushare_type get_sum_delayed_votes() const { return sum_delayed_votes; }
    ushare_type& get_sum_delayed_votes() { return sum_delayed_votes; }
    void set_sum_delayed_votes( const ushare_type& value ) { sum_delayed_votes = value; }
  };

  struct delayed_votes_wrapper
  {
    delayed_votes_wrapper(){}

    ushare_type                       sum_delayed_votes;
    std::vector< delayed_votes_data > delayed_votes;

    delayed_votes_wrapper( const ushare_type& _sum_delayed_votes, const account_details::t_delayed_votes& _delayed_votes )
    {
      sum_delayed_votes = _sum_delayed_votes;
      delayed_votes = shared_delayed_votes_wrapper::convert( _delayed_votes );
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

FC_REFLECT( hive::chain::account_details::misc,
          (proxy)
          (name)
          (pending_claimed_accounts)
          (created)(block_created)
          (governance_vote_expiration_ts)
          (withdraw_routes)(pending_escrow_transfers)(open_recurrent_transfers)(witnesses_voted_for)
          (savings_withdraw_requests)(can_vote)(mined)
          (memo_key)
          (proxied_vsf_votes)
        )
FC_REFLECT( hive::chain::account_details::shared_delayed_votes_wrapper,
          (delayed_votes)
        )

FC_REFLECT_DERIVED( hive::chain::account_details::shared_delayed_votes_wrapper_ex,
                    (hive::chain::account_details::shared_delayed_votes_wrapper),
                    (sum_delayed_votes)
                  )

FC_REFLECT( hive::chain::account_details::delayed_votes_wrapper, (sum_delayed_votes)(delayed_votes) )
