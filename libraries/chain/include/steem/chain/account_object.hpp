#pragma once
#include <steem/chain/steem_fwd.hpp>

#include <fc/fixed_string.hpp>

#include <steem/protocol/authority.hpp>
#include <steem/protocol/steem_operations.hpp>

#include <steem/chain/balance.hpp>
#include <steem/chain/steem_object_types.hpp>
#include <steem/chain/witness_objects.hpp>
#include <steem/chain/shared_authority.hpp>
#include <steem/chain/util/manabar.hpp>

#include <numeric>

namespace steem { namespace chain {

   using steem::protocol::authority;
   
   class account_object : public object< account_object_type, account_object >
   {
      public:
         template< typename Allocator >
         account_object( allocator< Allocator > a, int64_t _id,
            const account_name_type& _name, const public_key_type& _key, const time_point_sec& _creation_time, bool _mined,
            const account_name_type& _recovery_account, bool _fill_mana, const asset& incoming_delegation )
            : id( _id ), name( _name ), memo_key( _key ), created( _creation_time ), mined( _mined ),
            recovery_account( _recovery_account )
         {
            received_vesting_shares += incoming_delegation;
            voting_manabar.last_update_time = _creation_time.sec_since_epoch();
            downvote_manabar.last_update_time = _creation_time.sec_since_epoch();
            if( _fill_mana )
               voting_manabar.current_mana = STEEM_100_PERCENT;
         }

         //used for creation of accounts at genesis and in tests
         template< typename Constructor, typename Allocator >
         account_object( allocator< Allocator > a, int64_t _id, Constructor&& c )
            : id( _id )
         {
            c( *this );
         }

         void on_remove() const
         {
            FC_ASSERT( ( balance.amount == 0 ) && ( savings_balance.amount == 0 ) && ( reward_steem_balance.amount == 0 ),
               "HIVE tokens not transfered out before account_object removal." );
            FC_ASSERT( ( sbd_balance.amount == 0 ) && ( savings_sbd_balance.amount == 0 ) && ( reward_sbd_balance.amount == 0 ),
               "HBD tokens not transfered out before account_object removal." );
            FC_ASSERT( ( vesting_shares.amount == 0 ) && ( reward_vesting_balance.amount == 0 ),
               "VESTS not transfered out before account_object removal." );
         }

         /**
          * how much VESTS that account has that can still be delegated.
          * Note: that is total amount of VESTS minus those that are already delegated (note that received vests cannot be delegated further)
          * further reduced by those that are in the process of withdrawal.
          */
         asset get_vesting_shares_available_for_delegation() const
         {
            return vesting_shares - delegated_vesting_shares - asset( to_withdraw - withdrawn, VESTS_SYMBOL );
         }
         /**
          * how much VESTS that account has that can be withdrawn.
          * Note: that is total amount of VESTS minus those that are currently delegated (have to be undelegated first to withdraw);
          * those that are in the process of withdrawal can be withdrawn as new withdrawal schedule overrides previous one.
          */
         asset get_vesting_shares_available_for_withdraw() const
         {
            return vesting_shares - delegated_vesting_shares;
         }

         id_type           id;

         account_name_type name;
         public_key_type   memo_key;
         account_name_type proxy;

         time_point_sec    last_account_update;

         time_point_sec    created;
         bool              mined = true;
         account_name_type recovery_account;
         account_name_type reset_account = STEEM_NULL_ACCOUNT;
         time_point_sec    last_account_recovery;
         uint32_t          comment_count = 0;
         uint32_t          lifetime_vote_count = 0;
         uint32_t          post_count = 0;

         bool              can_vote = true;
         util::manabar     voting_manabar;
         util::manabar     downvote_manabar;

         HIVE_BALANCE( balance, get_balance ); ///< total liquid shares held by this account
         HIVE_BALANCE( savings_balance, get_savings );  ///< total liquid shares held by this account

         /**
          *  HBD Deposits pay interest based upon the interest rate set by witnesses. The purpose of these
          *  fields is to track the total (time * sbd_balance) that it is held. Then at the appointed time
          *  interest can be paid using the following equation:
          *
          *  interest = interest_rate * sbd_seconds / seconds_per_year
          *
          *  Every time the sbd_balance is updated the sbd_seconds is also updated. If at least
          *  STEEM_MIN_COMPOUNDING_INTERVAL_SECONDS has past since sbd_last_interest_payment then
          *  interest is added to sbd_balance.
          *
          *  @defgroup sbd_data sbd Balance Data
          */
         ///@{
         HBD_BALANCE( sbd_balance, get_sbd_balance ); /// total sbd balance
      public:
         uint128_t         sbd_seconds; ///< total sbd * how long it has been held
         time_point_sec    sbd_seconds_last_update; ///< the last time the sbd_seconds was updated
         time_point_sec    sbd_last_interest_payment; ///< used to pay interest at most once per month


         HBD_BALANCE( savings_sbd_balance, get_sbd_savings ); /// total sbd balance
      public:
         uint128_t         savings_sbd_seconds; ///< total sbd * how long it has been held
         time_point_sec    savings_sbd_seconds_last_update; ///< the last time the sbd_seconds was updated
         time_point_sec    savings_sbd_last_interest_payment; ///< used to pay interest at most once per month

         uint8_t           savings_withdraw_requests = 0;
         ///@}

         HBD_BALANCE( reward_sbd_balance, get_sbd_rewards );
         HIVE_BALANCE( reward_steem_balance, get_rewards );
         VEST_BALANCE( reward_vesting_balance, get_vest_rewards );
      public:
         asset             reward_vesting_steem = asset( 0, STEEM_SYMBOL ); //ABW: could be just number since it represents counterweight for reward_vesting_balance

         share_type        curation_rewards = 0;
         share_type        posting_rewards = 0;

         VEST_BALANCE( vesting_shares, get_vesting_shares ); ///< total vesting shares held by this account, controls its voting power
      public:
         asset             delegated_vesting_shares = asset( 0, VESTS_SYMBOL ); //ABW: could be just value
         asset             received_vesting_shares = asset( 0, VESTS_SYMBOL ); //ABW: could be just value

         asset             vesting_withdraw_rate = asset( 0, VESTS_SYMBOL ); ///< at the time this is updated it can be at most vesting_shares/104 //ABW: could be just value
         time_point_sec    next_vesting_withdrawal = fc::time_point_sec::maximum(); ///< after every withdrawal this is incremented by 1 week
         share_type        withdrawn = 0; /// Track how many shares have been withdrawn
         share_type        to_withdraw = 0; /// Might be able to look this up with operation history.
         uint16_t          withdraw_routes = 0;

         fc::array<share_type, STEEM_MAX_PROXY_RECURSION_DEPTH> proxied_vsf_votes;// = std::vector<share_type>( STEEM_MAX_PROXY_RECURSION_DEPTH, 0 ); ///< the total VFS votes proxied to this account

         uint16_t          witnesses_voted_for = 0;

         time_point_sec    last_post;
         time_point_sec    last_root_post = fc::time_point_sec::min();
         time_point_sec    last_post_edit;
         time_point_sec    last_vote_time;
         uint32_t          post_bandwidth = 0;

         share_type        pending_claimed_accounts = 0;

         /// This function should be used only when the account votes for a witness directly
         share_type        witness_vote_weight()const {
            return std::accumulate( proxied_vsf_votes.begin(),
                                    proxied_vsf_votes.end(),
                                    vesting_shares.amount );
         }
         share_type        proxied_vsf_votes_total()const {
            return std::accumulate( proxied_vsf_votes.begin(),
                                    proxied_vsf_votes.end(),
                                    share_type() );
         }

         friend class fc::reflector<account_object>;
   };

   class account_metadata_object : public object< account_metadata_object_type, account_metadata_object >
   {
      public:
         template< typename Allocator >
         account_metadata_object( allocator< Allocator > a, int64_t _id,
            const account_object& _account, const string& _metadata )
            : id( _id ), json_metadata( a ), posting_json_metadata( a )
         {
            account = _account.id;
            from_string( json_metadata, _metadata );
         }

         id_type           id;
         account_id_type   account;
         shared_string     json_metadata;
         shared_string     posting_json_metadata;
   };

   class account_authority_object : public object< account_authority_object_type, account_authority_object >
   {
      public:
         template< typename Constructor, typename Allocator >
         account_authority_object( allocator< Allocator > a, int64_t _id, Constructor&& c )
            : id( _id ), owner( a ), active( a ), posting( a )
         {
            c( *this );
         }

         id_type           id;

         account_name_type account;

         shared_authority  owner;   ///< used for backup control, can set owner or active
         shared_authority  active;  ///< used for all monetary operations, can set active or posting
         shared_authority  posting; ///< used for voting and posting

         time_point_sec    last_owner_update;
   };

   class vesting_delegation_object : public object< vesting_delegation_object_type, vesting_delegation_object >
   {
      public:
         template< typename Allocator >
         vesting_delegation_object( allocator< Allocator > a, int64_t _id,
            const account_name_type& _delegator, const account_name_type& _delegatee, const asset& _vests,
            const time_point_sec& _delegation_time )
            : id( _id ), delegator( _delegator ), delegatee( _delegatee ), min_delegation_time( _delegation_time )
         {
            vesting_shares += _vests;
         }

         id_type           id;
         account_name_type delegator;
         account_name_type delegatee;
         asset             vesting_shares = asset( 0, VESTS_SYMBOL );
         time_point_sec    min_delegation_time;
   };

   class vesting_delegation_expiration_object : public object< vesting_delegation_expiration_object_type, vesting_delegation_expiration_object >
   {
      public:
         template< typename Allocator >
         vesting_delegation_expiration_object( allocator< Allocator > a, int64_t _id,
            const account_name_type& _delegator, const asset& _delta, const time_point_sec& _expiration )
            : id( _id ), delegator( _delegator ), expiration( _expiration )
         {
            vesting_shares += _delta;
         }

         id_type           id;
         account_name_type delegator;
         asset             vesting_shares = asset( 0, VESTS_SYMBOL );
         time_point_sec    expiration;
   };

   class owner_authority_history_object : public object< owner_authority_history_object_type, owner_authority_history_object >
   {
      public:
         template< typename Allocator >
         owner_authority_history_object( allocator< Allocator > a, int64_t _id,
            const account_name_type& _account, const account_authority_object& _previous_authority, const time_point_sec& _now )
            : id( _id ), account( _account ), previous_owner_authority( allocator< shared_authority >( a ) ), last_valid_time( _now )
         {
            previous_owner_authority = _previous_authority.owner;
         }

         id_type           id;

         account_name_type account;
         shared_authority  previous_owner_authority;
         time_point_sec    last_valid_time;
   };

   class account_recovery_request_object : public object< account_recovery_request_object_type, account_recovery_request_object >
   {
      public:
         template< typename Allocator >
         account_recovery_request_object( allocator< Allocator > a, int64_t _id,
            const account_name_type& _account, const authority& _new_authority, const time_point_sec& _expires )
            : id( _id ), account_to_recover( _account ), new_owner_authority( allocator< shared_authority >( a ) ), expires( _expires )
         {
            new_owner_authority = _new_authority;
         }

         id_type           id;

         account_name_type account_to_recover;
         shared_authority  new_owner_authority;
         time_point_sec    expires;
   };

   class change_recovery_account_request_object : public object< change_recovery_account_request_object_type, change_recovery_account_request_object >
   {
      public:
         template< typename Allocator >
         change_recovery_account_request_object( allocator< Allocator > a, int64_t _id,
            const account_name_type& _account, const account_name_type& _new_recovery_account, const time_point_sec& _effective_time )
            : id( _id ), account_to_recover( _account ), recovery_account( _new_recovery_account ), effective_on( _effective_time )
         {}

         id_type           id;

         account_name_type account_to_recover;
         account_name_type recovery_account;
         time_point_sec    effective_on;
   };

   struct by_proxy;
   struct by_next_vesting_withdrawal;

   /**
    * @ingroup object_index
    */
   typedef multi_index_container<
      account_object,
      indexed_by<
         ordered_unique< tag< by_id >,
            member< account_object, account_id_type, &account_object::id > >,
         ordered_unique< tag< by_name >,
            member< account_object, account_name_type, &account_object::name > >,
         ordered_unique< tag< by_proxy >,
            composite_key< account_object,
               member< account_object, account_name_type, &account_object::proxy >,
               member< account_object, account_name_type, &account_object::name >
            > /// composite key by proxy
         >,
         ordered_unique< tag< by_next_vesting_withdrawal >,
            composite_key< account_object,
               member< account_object, time_point_sec, &account_object::next_vesting_withdrawal >,
               member< account_object, account_name_type, &account_object::name >
            > /// composite key by_next_vesting_withdrawal
         >
      >,
      allocator< account_object >
   > account_index;

   struct by_account;

   typedef multi_index_container <
      account_metadata_object,
      indexed_by<
         ordered_unique< tag< by_id >,
            member< account_metadata_object, account_metadata_id_type, &account_metadata_object::id > >,
         ordered_unique< tag< by_account >,
            member< account_metadata_object, account_id_type, &account_metadata_object::account > >
      >,
      allocator< account_metadata_object >
   > account_metadata_index;

   typedef multi_index_container <
      owner_authority_history_object,
      indexed_by <
         ordered_unique< tag< by_id >,
            member< owner_authority_history_object, owner_authority_history_id_type, &owner_authority_history_object::id > >,
         ordered_unique< tag< by_account >,
            composite_key< owner_authority_history_object,
               member< owner_authority_history_object, account_name_type, &owner_authority_history_object::account >,
               member< owner_authority_history_object, time_point_sec, &owner_authority_history_object::last_valid_time >,
               member< owner_authority_history_object, owner_authority_history_id_type, &owner_authority_history_object::id >
            >,
            composite_key_compare< std::less< account_name_type >, std::less< time_point_sec >, std::less< owner_authority_history_id_type > >
         >
      >,
      allocator< owner_authority_history_object >
   > owner_authority_history_index;

   struct by_last_owner_update;

   typedef multi_index_container <
      account_authority_object,
      indexed_by <
         ordered_unique< tag< by_id >,
            member< account_authority_object, account_authority_id_type, &account_authority_object::id > >,
         ordered_unique< tag< by_account >,
            composite_key< account_authority_object,
               member< account_authority_object, account_name_type, &account_authority_object::account >,
               member< account_authority_object, account_authority_id_type, &account_authority_object::id >
            >,
            composite_key_compare< std::less< account_name_type >, std::less< account_authority_id_type > >
         >,
         ordered_unique< tag< by_last_owner_update >,
            composite_key< account_authority_object,
               member< account_authority_object, time_point_sec, &account_authority_object::last_owner_update >,
               member< account_authority_object, account_authority_id_type, &account_authority_object::id >
            >,
            composite_key_compare< std::greater< time_point_sec >, std::less< account_authority_id_type > >
         >
      >,
      allocator< account_authority_object >
   > account_authority_index;

   struct by_delegation;

   typedef multi_index_container <
      vesting_delegation_object,
      indexed_by <
         ordered_unique< tag< by_id >,
            member< vesting_delegation_object, vesting_delegation_id_type, &vesting_delegation_object::id > >,
         ordered_unique< tag< by_delegation >,
            composite_key< vesting_delegation_object,
               member< vesting_delegation_object, account_name_type, &vesting_delegation_object::delegator >,
               member< vesting_delegation_object, account_name_type, &vesting_delegation_object::delegatee >
            >,
            composite_key_compare< std::less< account_name_type >, std::less< account_name_type > >
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
            member< vesting_delegation_expiration_object, vesting_delegation_expiration_id_type, &vesting_delegation_expiration_object::id > >,
         ordered_unique< tag< by_expiration >,
            composite_key< vesting_delegation_expiration_object,
               member< vesting_delegation_expiration_object, time_point_sec, &vesting_delegation_expiration_object::expiration >,
               member< vesting_delegation_expiration_object, vesting_delegation_expiration_id_type, &vesting_delegation_expiration_object::id >
            >,
            composite_key_compare< std::less< time_point_sec >, std::less< vesting_delegation_expiration_id_type > >
         >,
         ordered_unique< tag< by_account_expiration >,
            composite_key< vesting_delegation_expiration_object,
               member< vesting_delegation_expiration_object, account_name_type, &vesting_delegation_expiration_object::delegator >,
               member< vesting_delegation_expiration_object, time_point_sec, &vesting_delegation_expiration_object::expiration >,
               member< vesting_delegation_expiration_object, vesting_delegation_expiration_id_type, &vesting_delegation_expiration_object::id >
            >,
            composite_key_compare< std::less< account_name_type >, std::less< time_point_sec >, std::less< vesting_delegation_expiration_id_type > >
         >
      >,
      allocator< vesting_delegation_expiration_object >
   > vesting_delegation_expiration_index;

   struct by_expiration;

   typedef multi_index_container <
      account_recovery_request_object,
      indexed_by <
         ordered_unique< tag< by_id >,
            member< account_recovery_request_object, account_recovery_request_id_type, &account_recovery_request_object::id > >,
         ordered_unique< tag< by_account >,
            member< account_recovery_request_object, account_name_type, &account_recovery_request_object::account_to_recover >
         >,
         ordered_unique< tag< by_expiration >,
            composite_key< account_recovery_request_object,
               member< account_recovery_request_object, time_point_sec, &account_recovery_request_object::expires >,
               member< account_recovery_request_object, account_name_type, &account_recovery_request_object::account_to_recover >
            >,
            composite_key_compare< std::less< time_point_sec >, std::less< account_name_type > >
         >
      >,
      allocator< account_recovery_request_object >
   > account_recovery_request_index;

   struct by_effective_date;

   typedef multi_index_container <
      change_recovery_account_request_object,
      indexed_by <
         ordered_unique< tag< by_id >,
            member< change_recovery_account_request_object, change_recovery_account_request_id_type, &change_recovery_account_request_object::id > >,
         ordered_unique< tag< by_account >,
            member< change_recovery_account_request_object, account_name_type, &change_recovery_account_request_object::account_to_recover >
         >,
         ordered_unique< tag< by_effective_date >,
            composite_key< change_recovery_account_request_object,
               member< change_recovery_account_request_object, time_point_sec, &change_recovery_account_request_object::effective_on >,
               member< change_recovery_account_request_object, account_name_type, &change_recovery_account_request_object::account_to_recover >
            >,
            composite_key_compare< std::less< time_point_sec >, std::less< account_name_type > >
         >
      >,
      allocator< change_recovery_account_request_object >
   > change_recovery_account_request_index;
} }

#ifdef ENABLE_MIRA
namespace mira {

template<> struct is_static_length< steem::chain::account_object > : public boost::true_type {};
template<> struct is_static_length< steem::chain::vesting_delegation_object > : public boost::true_type {};
template<> struct is_static_length< steem::chain::vesting_delegation_expiration_object > : public boost::true_type {};
template<> struct is_static_length< steem::chain::change_recovery_account_request_object > : public boost::true_type {};

} // mira
#endif

FC_REFLECT( steem::chain::account_object,
             (id)(name)(memo_key)(proxy)(last_account_update)
             (created)(mined)
             (recovery_account)(last_account_recovery)(reset_account)
             (comment_count)(lifetime_vote_count)(post_count)(can_vote)(voting_manabar)(downvote_manabar)
             (balance)
             (savings_balance)
             (sbd_balance)(sbd_seconds)(sbd_seconds_last_update)(sbd_last_interest_payment)
             (savings_sbd_balance)(savings_sbd_seconds)(savings_sbd_seconds_last_update)(savings_sbd_last_interest_payment)(savings_withdraw_requests)
             (reward_steem_balance)(reward_sbd_balance)(reward_vesting_balance)(reward_vesting_steem)
             (vesting_shares)(delegated_vesting_shares)(received_vesting_shares)
             (vesting_withdraw_rate)(next_vesting_withdrawal)(withdrawn)(to_withdraw)(withdraw_routes)
             (curation_rewards)
             (posting_rewards)
             (proxied_vsf_votes)(witnesses_voted_for)
             (last_post)(last_root_post)(last_post_edit)(last_vote_time)(post_bandwidth)
             (pending_claimed_accounts)
          )

CHAINBASE_SET_INDEX_TYPE( steem::chain::account_object, steem::chain::account_index )

FC_REFLECT( steem::chain::account_metadata_object,
             (id)(account)(json_metadata)(posting_json_metadata) )
CHAINBASE_SET_INDEX_TYPE( steem::chain::account_metadata_object, steem::chain::account_metadata_index )

FC_REFLECT( steem::chain::account_authority_object,
             (id)(account)(owner)(active)(posting)(last_owner_update)
)
CHAINBASE_SET_INDEX_TYPE( steem::chain::account_authority_object, steem::chain::account_authority_index )

FC_REFLECT( steem::chain::vesting_delegation_object,
            (id)(delegator)(delegatee)(vesting_shares)(min_delegation_time) )
CHAINBASE_SET_INDEX_TYPE( steem::chain::vesting_delegation_object, steem::chain::vesting_delegation_index )

FC_REFLECT( steem::chain::vesting_delegation_expiration_object,
            (id)(delegator)(vesting_shares)(expiration) )
CHAINBASE_SET_INDEX_TYPE( steem::chain::vesting_delegation_expiration_object, steem::chain::vesting_delegation_expiration_index )

FC_REFLECT( steem::chain::owner_authority_history_object,
             (id)(account)(previous_owner_authority)(last_valid_time)
          )
CHAINBASE_SET_INDEX_TYPE( steem::chain::owner_authority_history_object, steem::chain::owner_authority_history_index )

FC_REFLECT( steem::chain::account_recovery_request_object,
             (id)(account_to_recover)(new_owner_authority)(expires)
          )
CHAINBASE_SET_INDEX_TYPE( steem::chain::account_recovery_request_object, steem::chain::account_recovery_request_index )

FC_REFLECT( steem::chain::change_recovery_account_request_object,
             (id)(account_to_recover)(recovery_account)(effective_on)
          )
CHAINBASE_SET_INDEX_TYPE( steem::chain::change_recovery_account_request_object, steem::chain::change_recovery_account_request_index )
