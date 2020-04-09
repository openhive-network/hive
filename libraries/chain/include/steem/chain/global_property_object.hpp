#pragma once
#include <steem/chain/steem_fwd.hpp>

#include <fc/uint128.hpp>

#include <steem/chain/steem_object_types.hpp>

#include <steem/protocol/asset.hpp>

namespace steem { namespace chain {

   using steem::protocol::asset;
   using steem::protocol::price;

   /**
    * @class dynamic_global_property_object
    * @brief Maintains global state information
    * @ingroup object
    * @ingroup implementation
    *
    * This is an implementation detail. The values here are calculated during normal chain operations and reflect the
    * current values of global blockchain properties.
    */
   class dynamic_global_property_object : public object< dynamic_global_property_object_type, dynamic_global_property_object>
   {
      CHAINBASE_OBJECT( dynamic_global_property_object, true );
      public:
         template< typename Constructor, typename Allocator >
         dynamic_global_property_object( allocator< Allocator > a, int64_t _id,
            TTempBalance* created_initial_supply, uint64_t initial_supply,
            TTempBalance* created_initial_sbd_supply, uint64_t initial_sbd_supply, Constructor&& c )
            : id( _id ), virtual_supply( initial_supply, STEEM_SYMBOL ), current_supply( initial_supply, STEEM_SYMBOL ),
            init_sbd_supply( initial_sbd_supply, SBD_SYMBOL )
         {
            c( *this );
            created_initial_supply->issue_asset( get_full_steem_supply() );
            created_initial_sbd_supply->issue_asset( get_full_sbd_supply() );
         }

         void on_remove() const
         {
            FC_ASSERT( ( total_vesting_fund_steem.amount == 0 ) && ( total_reward_fund_steem.amount == 0 ) &&
               ( pending_rewarded_vesting_steem.amount == 0 ), "HIVE tokens not transfered out before dynamic_global_property_object removal." );
         }

         // all HIVE in circulation
         asset get_full_steem_supply() const { return current_supply; }
         // creates new HIVE tokens
         void issue_steem( TTempBalance* balance, const asset& amount )
         {
            current_supply += amount;
            balance->issue_asset( amount );
         }
         // removes existing HIVE tokens
         void burn_steem( TTempBalance* balance, const asset& amount )
         {
            current_supply -= amount;
            balance->burn_asset( amount );
         }

         /**
          * all HBD in circulation
          * Note: initial supply does not take part in interest calculation, that's why it is separate from current_sbd_supply
          */
         asset get_full_sbd_supply() const
         {
            return current_sbd_supply + init_sbd_supply;
         }
         // creates new HBD tokens
         void issue_sbd( TTempBalance* balance, const asset& amount )
         {
            current_sbd_supply += amount;
            balance->issue_asset( amount );
         }
         // removes existing HIVE tokens
         void burn_sbd( TTempBalance* balance, const asset& amount )
         {
            current_sbd_supply -= amount;
            balance->burn_asset( amount );
         }

         /**
          * all VESTS in circulation
          * Note: the counters used here act as a counterweight for total_vesting_fund_steem and pending_rewarded_vesting_steem
          * pool balances respectively, that's why they are not a single counter
          */
         asset get_full_vest_supply() const
         {
            return total_vesting_shares + pending_rewarded_vesting_shares;
         }
         // creates new VESTS taking liquid HIVE to the pool
         void issue_vests( TTempBalance* balance, const asset& amount, TTempBalance* liquid, const asset& liquid_amount, bool to_reward = false )
         {
            if( to_reward )
            {
               pending_rewarded_vesting_shares += amount;
               get_pending_rewarded_vesting_steem().transfer_from( liquid, liquid_amount.amount.value );
            }
            else
            {
               total_vesting_shares += amount;
               get_total_vesting_fund_steem().transfer_from( liquid, liquid_amount.amount.value );
            }
            balance->issue_asset( amount );
         }
         // removes existing VESTS and gives out liquid HIVE from the pool
         void burn_vests( TTempBalance* balance, const asset& amount, TTempBalance* liquid, const asset& liquid_amount, bool from_reward = false )
         {
            if( from_reward )
            {
               pending_rewarded_vesting_shares -= amount;
               get_pending_rewarded_vesting_steem().transfer_to( liquid, liquid_amount.amount.value );
            }
            else
            {
               total_vesting_shares -= amount;
               get_total_vesting_fund_steem().transfer_to( liquid, liquid_amount.amount.value );
            }
            balance->burn_asset( amount );
         }
         // moves reward balance to regular balance
         void move_vests_from_reward( const asset& amount, const asset& liquid_amount )
         {
            TTempBalance steem_balance( STEEM_SYMBOL );
            pending_rewarded_vesting_shares -= amount;
            get_pending_rewarded_vesting_steem().transfer_to( &steem_balance, liquid_amount.amount.value );
            total_vesting_shares += amount;
            get_total_vesting_fund_steem().transfer_from( &steem_balance );
         }

         price get_vesting_share_price() const
         {
            if( total_vesting_fund_steem.amount == 0 || total_vesting_shares.amount == 0 )
               return price( asset( 1000, STEEM_SYMBOL ), asset( 1000000, VESTS_SYMBOL ) );

            return price( total_vesting_shares, total_vesting_fund_steem );
         }

         price get_reward_vesting_share_price() const
         {
            return price( total_vesting_shares + pending_rewarded_vesting_shares,
               total_vesting_fund_steem + pending_rewarded_vesting_steem );
         }

         id_type           id;

         uint32_t          head_block_number = 0;
         block_id_type     head_block_id;
         time_point_sec    time;
         account_name_type current_witness;

         /**
          *  The total POW accumulated, aka the sum of num_pow_witness at the time new POW is added
          */
         uint64_t total_pow = -1;

         /**
          * The current count of how many pending POW witnesses there are, determines the difficulty
          * of doing pow
          */
         uint32_t num_pow_witnesses = 0;

         asset       virtual_supply             = asset( 0, STEEM_SYMBOL );
         HIVE_BALANCE_COUNTER( current_supply, get_current_supply );
         HBD_BALANCE_COUNTER( init_sbd_supply, get_init_sbd_supply );
         HBD_BALANCE_COUNTER( current_sbd_supply, get_current_sbd_supply );
         HIVE_BALANCE( total_vesting_fund_steem, get_total_vesting_fund_steem );
         VEST_BALANCE_COUNTER( total_vesting_shares, get_total_vesting_shares ); //counterweight for total_vesting_fund_steem
         HIVE_BALANCE( total_reward_fund_steem, get_total_reward_fund_steem );
      public:
         fc::uint128 total_reward_shares2; ///< the running total of REWARD^2
         VEST_BALANCE_COUNTER( pending_rewarded_vesting_shares, get_pending_rewarded_vesting_shares ); //counterweight for pending_rewarded_vesting_steem balance
         HIVE_BALANCE( pending_rewarded_vesting_steem, get_pending_rewarded_vesting_steem );
      public:

         /**
          *  This property defines the interest rate that HBD deposits receive.
          */
         uint16_t sbd_interest_rate = 0;

         uint16_t sbd_print_rate = STEEM_100_PERCENT;

         /**
          *  Maximum block size is decided by the set of active witnesses which change every round.
          *  Each witness posts what they think the maximum size should be as part of their witness
          *  properties, the median size is chosen to be the maximum block size for the round.
          *
          *  @note the minimum value for maximum_block_size is defined by the protocol to prevent the
          *  network from getting stuck by witnesses attempting to set this too low.
          */
         uint32_t     maximum_block_size = 0;

         /**
          * The size of the block that is partitioned for actions.
          * Required actions can only be delayed if they take up more than this amount. More can be
          * included, but are not required. Block generation should only include transactions up
          * to maximum_block_size - required_actions_parition_size to ensure required actions are
          * not delayed when they should not be.
          */
         uint16_t     required_actions_partition_percent = 0;

         /**
          * The current absolute slot number.  Equal to the total
          * number of slots since genesis.  Also equal to the total
          * number of missed slots plus head_block_number.
          */
         uint64_t      current_aslot = 0;

         /**
          * used to compute witness participation.
          */
         fc::uint128_t recent_slots_filled;
         uint8_t       participation_count = 0; ///< Divide by 128 to compute participation percentage

         uint32_t last_irreversible_block_num = 0;

         /**
          * The number of votes regenerated per day.  Any user voting slower than this rate will be
          * "wasting" voting power through spillover; any user voting faster than this rate will have
          * their votes reduced.
          */
         uint32_t vote_power_reserve_rate = STEEM_INITIAL_VOTE_POWER_RATE;

         uint32_t delegation_return_period = STEEM_DELEGATION_RETURN_PERIOD_HF0;

         uint64_t reverse_auction_seconds = 0;

         int64_t available_account_subsidies = 0;

         uint16_t sbd_stop_percent = 0;
         uint16_t sbd_start_percent = 0;

         //settings used to compute payments for every proposal
         time_point_sec next_maintenance_time;
         time_point_sec last_budget_time;

         uint16_t content_reward_percent = STEEM_CONTENT_REWARD_PERCENT_HF16;
         uint16_t vesting_reward_percent = STEEM_VESTING_FUND_PERCENT_HF16;
         uint16_t sps_fund_percent = STEEM_PROPOSAL_FUND_PERCENT_HF0;

         asset sps_interval_ledger = asset( 0, SBD_SYMBOL );

         uint16_t downvote_pool_percent = 0;

#ifdef STEEM_ENABLE_SMT
         asset smt_creation_fee = asset( 1000, SBD_SYMBOL );
#endif
   };

   typedef multi_index_container<
      dynamic_global_property_object,
      indexed_by<
         ordered_unique< tag< by_id >,
            member< dynamic_global_property_object, dynamic_global_property_object::id_type, &dynamic_global_property_object::id > >
      >,
      allocator< dynamic_global_property_object >
   > dynamic_global_property_index;

} } // steem::chain

#ifdef ENABLE_MIRA
namespace mira {

template<> struct is_static_length< steem::chain::dynamic_global_property_object > : public boost::true_type {};

} // mira
#endif

FC_REFLECT( steem::chain::dynamic_global_property_object,
             (id)
             (head_block_number)
             (head_block_id)
             (time)
             (current_witness)
             (total_pow)
             (num_pow_witnesses)
             (virtual_supply)
             (current_supply)
             (init_sbd_supply)
             (current_sbd_supply)
             (total_vesting_fund_steem)
             (total_vesting_shares)
             (total_reward_fund_steem)
             (total_reward_shares2)
             (pending_rewarded_vesting_shares)
             (pending_rewarded_vesting_steem)
             (sbd_interest_rate)
             (sbd_print_rate)
             (maximum_block_size)
             (required_actions_partition_percent)
             (current_aslot)
             (recent_slots_filled)
             (participation_count)
             (last_irreversible_block_num)
             (vote_power_reserve_rate)
             (delegation_return_period)
             (reverse_auction_seconds)
             (available_account_subsidies)
             (sbd_stop_percent)
             (sbd_start_percent)
             (next_maintenance_time)
             (last_budget_time)
             (content_reward_percent)
             (vesting_reward_percent)
             (sps_fund_percent)
             (sps_interval_ledger)
             (downvote_pool_percent)
#ifdef STEEM_ENABLE_SMT
             (smt_creation_fee)
#endif
          )
CHAINBASE_SET_INDEX_TYPE( steem::chain::dynamic_global_property_object, steem::chain::dynamic_global_property_index )
