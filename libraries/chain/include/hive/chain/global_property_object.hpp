#pragma once
#include <hive/chain/hive_fwd.hpp>

#include <fc/uint128.hpp>

#include <hive/chain/hive_object_types.hpp>

#include <hive/protocol/asset.hpp>

namespace hive { namespace chain {

  using hive::protocol::asset;
  using hive::protocol::price;

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
    CHAINBASE_OBJECT( dynamic_global_property_object );
    public:
      template< typename Allocator >
      dynamic_global_property_object( allocator< Allocator > a, uint64_t _id,
        const account_name_type& _initial_witness, const asset& _initial_hive_supply, const asset& _initial_hbd_supply )
        : id( _id ), current_witness( _initial_witness ), virtual_supply( _initial_hive_supply, HIVE_SYMBOL ),
        current_supply( _initial_hive_supply, HIVE_SYMBOL ), init_hbd_supply( _initial_hbd_supply, HBD_SYMBOL )
      {}

      //main HIVE token counter (see also get_full_hive_supply)
      const asset& get_current_supply() const { return current_supply; }

      //initial amount of HBD issued (see also get_full_hbd_supply)
      const asset& get_init_hbd_supply() const { return init_hbd_supply; }
      //main HBD token counter (see also get_full_hbd_supply)
      const asset& get_current_hbd_supply() const { return current_hbd_supply; }
      //rate of interest for holding HBD (in BPS - basis points)
      uint16_t get_hbd_interest_rate() const { return hbd_interest_rate; }
      //percentage of HIVE being converted to HBD during payouts (in BPS - basis points)
      uint16_t get_hbd_print_rate() const { return hbd_print_rate; }

      //pool of HIVE tokens vested normally
      const asset& get_total_vesting_fund_hive() const { return total_vesting_fund_hive; }
      //amount of VESTS produced from HIVE held in normal vested fund
      const asset& get_total_vesting_shares() const { return total_vesting_shares; }

      //pool of HIVE tokens for pending (liquid) rewards
      const asset& get_total_reward_fund_hive() const { return total_reward_fund_hive; }

      //pool of HIVE tokens for pending (vested) rewards
      const asset& get_pending_rewarded_vesting_hive() const { return pending_rewarded_vesting_hive; }
      //amount of VESTS produced from HIVE held in pending reward vested fund
      const asset& get_pending_rewarded_vesting_shares() const { return pending_rewarded_vesting_shares; }

      uint32_t          head_block_number = 0;
      block_id_type     head_block_id;
      time_point_sec    time = HIVE_GENESIS_TIME;
      account_name_type current_witness; //< TODO: replace with account_id_type

      /**
        *  The total POW accumulated, aka the sum of num_pow_witness at the time new POW is added
        */
      uint64_t total_pow = -1;

      /**
        * The current count of how many pending POW witnesses there are, determines the difficulty
        * of doing pow
        */
      uint32_t num_pow_witnesses = 0;

      asset       virtual_supply             = asset( 0, HIVE_SYMBOL ); //< TODO: replace with HIVE_asset
      asset       current_supply             = asset( 0, HIVE_SYMBOL ); //< TODO: replace with HIVE_asset
      asset       init_hbd_supply            = asset( 0, HBD_SYMBOL ); //< TODO: replace with HBD_asset
      asset       current_hbd_supply         = asset( 0, HBD_SYMBOL ); //< TODO: replace with HBD_asset
      asset       total_vesting_fund_hive    = asset( 0, HIVE_SYMBOL ); //< TODO: replace with HIVE_asset
      asset       total_vesting_shares       = asset( 0, VESTS_SYMBOL ); //< TODO: replace with VEST_asset
      asset       total_reward_fund_hive     = asset( 0, HIVE_SYMBOL ); //< TODO: replace with HIVE_asset
      fc::uint128 total_reward_shares2; ///< the running total of REWARD^2
      asset       pending_rewarded_vesting_shares = asset( 0, VESTS_SYMBOL ); //< TODO: replace with VEST_asset
      asset       pending_rewarded_vesting_hive   = asset( 0, HIVE_SYMBOL ); //< TODO: replace with HIVE_asset

      price       get_vesting_share_price() const
      {
        if ( total_vesting_fund_hive.amount == 0 || total_vesting_shares.amount == 0 )
          return price ( asset( 1000, HIVE_SYMBOL ), asset( 1000000, VESTS_SYMBOL ) );

        return price( total_vesting_shares, total_vesting_fund_hive );
      }

      price get_reward_vesting_share_price() const
      {
        return price( total_vesting_shares + pending_rewarded_vesting_shares,
          total_vesting_fund_hive + pending_rewarded_vesting_hive );
      }

      /**
        *  This property defines the interest rate that HBD deposits receive.
        */
      uint16_t hbd_interest_rate = 0;

      uint16_t hbd_print_rate = HIVE_100_PERCENT;

      /**
        *  Maximum block size is decided by the set of active witnesses which change every round.
        *  Each witness posts what they think the maximum size should be as part of their witness
        *  properties, the median size is chosen to be the maximum block size for the round.
        *
        *  @note the minimum value for maximum_block_size is defined by the protocol to prevent the
        *  network from getting stuck by witnesses attempting to set this too low.
        */
      uint32_t     maximum_block_size = HIVE_MAX_BLOCK_SIZE;

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
      fc::uint128_t recent_slots_filled = fc::uint128::max_value();
      uint8_t       participation_count = 128; ///< Divide by 128 to compute participation percentage

      /**
        * The number of votes regenerated per day.  Any user voting slower than this rate will be
        * "wasting" voting power through spillover; any user voting faster than this rate will have
        * their votes reduced.
        */
      uint32_t vote_power_reserve_rate = HIVE_INITIAL_VOTE_POWER_RATE;

      uint32_t delegation_return_period = HIVE_DELEGATION_RETURN_PERIOD_HF0;

      uint64_t reverse_auction_seconds = HIVE_REVERSE_AUCTION_WINDOW_SECONDS_HF6;
      uint64_t early_voting_seconds = 0;
      uint64_t mid_voting_seconds = 0;

      int64_t available_account_subsidies = 0;

      uint16_t hbd_stop_percent = HIVE_HBD_STOP_PERCENT_HF14;
      uint16_t hbd_start_percent = HIVE_HBD_START_PERCENT_HF14;

      // Settings used to compute payments for every proposal
      time_point_sec next_maintenance_time = HIVE_GENESIS_TIME;
      time_point_sec last_budget_time = HIVE_GENESIS_TIME;
      // Setting to convert hive to HBD in the treasury account
      time_point_sec next_daily_maintenance_time = HIVE_GENESIS_TIME;

      uint16_t content_reward_percent = HIVE_CONTENT_REWARD_PERCENT_HF16;
      uint16_t vesting_reward_percent = HIVE_VESTING_FUND_PERCENT_HF16;
      uint16_t proposal_fund_percent = HIVE_PROPOSAL_FUND_PERCENT_HF0;

      asset dhf_interval_ledger = asset( 0, HBD_SYMBOL ); //< TODO: replace with HBD_asset

      uint16_t downvote_pool_percent = 0;

      // limits number of objects removed in one automatic operation (only applies to situations where many
      // objects can accumulate over time but need to be removed in single operation f.e. proposal votes)
      int16_t current_remove_threshold = HIVE_GLOBAL_REMOVE_THRESHOLD; //negative means no limit

      uint8_t max_consecutive_recurrent_transfer_failures = HIVE_MAX_CONSECUTIVE_RECURRENT_TRANSFER_FAILURES;
      uint16_t max_recurrent_transfer_end_date = HIVE_MAX_RECURRENT_TRANSFER_END_DATE;
      uint8_t min_recurrent_transfers_recurrence = HIVE_MIN_RECURRENT_TRANSFERS_RECURRENCE;
      uint16_t max_open_recurrent_transfers = HIVE_MAX_OPEN_RECURRENT_TRANSFERS;

#ifdef HIVE_ENABLE_SMT
      asset smt_creation_fee = asset( 1000, HBD_SYMBOL ); //< TODO: replace with HBD_asset
#endif
    CHAINBASE_UNPACK_CONSTRUCTOR(dynamic_global_property_object);
  };

  typedef multi_index_container<
    dynamic_global_property_object,
    indexed_by<
      ordered_unique< tag< by_id >,
        const_mem_fun< dynamic_global_property_object, dynamic_global_property_object::id_type, &dynamic_global_property_object::get_id > >
    >,
    allocator< dynamic_global_property_object >
  > dynamic_global_property_index;

} } // hive::chain

FC_REFLECT( hive::chain::dynamic_global_property_object,
          (id)
          (head_block_number)
          (head_block_id)
          (time)
          (current_witness)
          (total_pow)
          (num_pow_witnesses)
          (virtual_supply)
          (current_supply)
          (init_hbd_supply)
          (current_hbd_supply)
          (total_vesting_fund_hive)
          (total_vesting_shares)
          (total_reward_fund_hive)
          (total_reward_shares2)
          (pending_rewarded_vesting_shares)
          (pending_rewarded_vesting_hive)
          (hbd_interest_rate)
          (hbd_print_rate)
          (maximum_block_size)
          (required_actions_partition_percent)
          (current_aslot)
          (recent_slots_filled)
          (participation_count)
          (vote_power_reserve_rate)
          (delegation_return_period)
          (reverse_auction_seconds)
          (early_voting_seconds)
          (mid_voting_seconds)
          (available_account_subsidies)
          (hbd_stop_percent)
          (hbd_start_percent)
          (next_maintenance_time)
          (last_budget_time)
          (next_daily_maintenance_time)
          (content_reward_percent)
          (vesting_reward_percent)
          (proposal_fund_percent)
          (dhf_interval_ledger)
          (downvote_pool_percent)
          (current_remove_threshold)
          (max_consecutive_recurrent_transfer_failures)
          (max_recurrent_transfer_end_date)
          (max_open_recurrent_transfers)
          (min_recurrent_transfers_recurrence)
#ifdef HIVE_ENABLE_SMT
          (smt_creation_fee)
#endif
        )
CHAINBASE_SET_INDEX_TYPE( hive::chain::dynamic_global_property_object, hive::chain::dynamic_global_property_index )
