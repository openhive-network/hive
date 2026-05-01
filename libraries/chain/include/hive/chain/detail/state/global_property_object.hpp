#pragma once
#include <hive/chain/hive_fwd.hpp>

#include <fc/uint128.hpp>

#include <hive/chain/hive_object_types.hpp>
#include <hive/chain/util/balance.hpp>

#include <hive/protocol/asset.hpp>
#include <hive/protocol/config.hpp>

namespace hive { namespace chain {

using hive::protocol::HIVE_asset;
using hive::protocol::HBD_asset;
using hive::protocol::HBD_price;
using hive::protocol::VEST_asset;
using hive::protocol::VEST_price;

/**
  * @class dynamic_global_property_object
  * @brief Maintains global state information
  * @ingroup object
  * @ingroup implementation
  *
  * This is an implementation detail. The values here are calculated during normal chain operations and reflect the
  * current values of global blockchain properties.
  */
class dynamic_global_property_object : public object< dynamic_global_property_object_type, dynamic_global_property_object >
{
  CHAINBASE_OBJECT( dynamic_global_property_object );

public:
  template< typename Allocator >
  dynamic_global_property_object( allocator< Allocator > a, uint64_t _id,
    const account_name_type& _initial_witness )
    : id( _id ), current_witness( _initial_witness )
  {}

  //main HIVE token counter
  const HIVE_asset& get_current_supply() const { return current_supply; }
  //creates HIVE out of thin air - for inflation
  inline temp_HIVE_balance issue_HIVE( const HIVE_asset& hive_amount );
  //burns all HIVE on given balance into the void - for clearing null
  inline void burn_HIVE( temp_HIVE_balance& hive_balance );
  //all HIVE in existence including those that would be created if all HBD was converted to HIVE
  const HIVE_asset& get_virtual_supply() const { return virtual_supply; }

  HIVE_asset& access_current_supply() { return current_supply; } // TODO: should not be needed once issue/burn/convert are used
  HIVE_asset& access_virtual_supply() { return virtual_supply; } // TODO: should not be needed once issue/burn/convert are used

  //main HBD token counter
  const HBD_asset& get_current_hbd_supply() const { return current_hbd_supply; }
  //initial HBD tokens
  const HBD_asset& get_initial_hbd_supply() const { return init_hbd_supply; }
  //creates HBD out of thin air - for interest
  inline temp_HBD_balance issue_HBD( const HBD_asset& hbd_amount, const HBD_price& exchange_rate, bool full_update_virtual_supply = false );
  //burns all HBD on given balance into the void - for clearing null
  inline void burn_HBD( temp_HBD_balance& hbd_balance, const HBD_price& exchange_rate, bool full_update_virtual_supply = false );
  //converts all HIVE on given balance to HBD
  inline temp_HBD_balance convert_HIVE_to_HBD( temp_HIVE_balance& hive_balance, const HBD_price& exchange_rate, bool full_update_virtual_supply = true );

  HBD_asset& access_current_hbd_supply() { return current_hbd_supply; } //TODO: should not be needed with init/burn/convert
  HBD_asset& access_initial_hbd_supply() { return init_hbd_supply; } //TODO: should not be needed with proper genesis rework

  //rate of interest for holding HBD (in BPS - basis points)
  uint16_t get_hbd_interest_rate() const { return hbd_interest_rate; }
  //percentage of HIVE being converted to HBD during payouts (in BPS - basis points)
  uint16_t get_hbd_print_rate() const { return hbd_print_rate; }

  void set_hbd_interest_rate( uint16_t rate ) { hbd_interest_rate = rate; }
  void set_hbd_print_rate( uint16_t rate ) { hbd_print_rate = rate; }

  //pool of HIVE tokens vested normally
  const HIVE_asset& get_total_vesting_fund_hive() const { return total_vesting_fund_hive; }
  //amount of VESTS produced from HIVE held in normal vested fund
  const VEST_asset& get_total_vesting_shares() const { return total_vesting_shares; }
  //issues a specified amount of VESTS - used internally, also for use in debug plugin and unit tests (used f.e. for initial vest price stabilization)
  inline temp_VEST_balance issue_VESTS( const VEST_asset& vest_amount, bool is_reward = false );
  //burns all VESTS on given balance - used internally, also for clearing null
  inline void burn_VESTS( temp_VEST_balance& vest_balance, bool is_reward = false );
  //creates new VESTS out of all HIVE on given balance, HIVE is moved to total_vesting_fund_hive if not is_reward
  inline temp_VEST_balance convert_HIVE_to_VESTS( temp_HIVE_balance& hive_balance, bool is_reward = false );
  //creates new VESTS out of all HIVE on given balance (with use of passed price), HIVE is moved to total_vesting_fund_hive if not is_reward
  inline temp_VEST_balance convert_HIVE_to_VESTS( temp_HIVE_balance& hive_balance, const VEST_price& vest_price, bool is_reward = false );
  //burns all VESTS on given balance and gives out associated HIVE from total_vesting_fund_hive
  temp_HIVE_balance convert_VESTS_to_HIVE( temp_VEST_balance& vest_balance ) { return convert_VESTS_to_HIVE( vest_balance, get_vesting_share_price() ); }
  //burns all VESTS on given balance (with use of passed price) and gives out associated HIVE from total_vesting_fund_hive
  inline temp_HIVE_balance convert_VESTS_to_HIVE( temp_VEST_balance& vest_balance, const VEST_price& vest_price );

  HIVE_balance& access_total_vesting_fund_hive() { return total_vesting_fund_hive; } //TODO: should not be needed when convert is defined and used
  VEST_asset& access_total_vesting_shares() { return total_vesting_shares; } //TODO: should not be needed when convert is defined and used

  VEST_price get_vesting_share_price() const
  {
    if ( total_vesting_fund_hive.is_empty() || total_vesting_shares.amount == 0 )
      return VEST_price( 1000000, 1000 );
    return VEST_price( total_vesting_shares, total_vesting_fund_hive );
  }

  //pool of HIVE tokens for pending (vested) rewards
  const HIVE_asset& get_pending_rewarded_vesting_hive() const { return pending_rewarded_vesting_hive; }
  //amount of VESTS produced from HIVE held in pending reward vested fund
  const VEST_asset& get_pending_rewarded_vesting_shares() const { return pending_rewarded_vesting_shares; }

  HIVE_asset& access_pending_rewarded_vesting_hive() { return pending_rewarded_vesting_hive; } //TODO: should not be needed when convert is defined and used
  VEST_asset& access_pending_rewarded_vesting_shares() { return pending_rewarded_vesting_shares; } //TODO: should not be needed when convert is defined and used

  VEST_price get_reward_vesting_share_price() const
  {
    return VEST_price( total_vesting_shares + pending_rewarded_vesting_shares,
      total_vesting_fund_hive.as_asset() + pending_rewarded_vesting_hive );
  }

  const HBD_asset& get_dhf_interval_ledger() const { return dhf_interval_ledger; }
  time_point_sec get_next_maintenance_time() const { return next_maintenance_time; }
  time_point_sec get_last_budget_time() const { return last_budget_time; }
  time_point_sec get_next_daily_maintenance_time() const { return next_daily_maintenance_time; }

  void add_to_dhf_interval_ledger( const HBD_asset& value ) { dhf_interval_ledger += value; }
  void clear_dhf_interval_ledger() { dhf_interval_ledger = HBD_asset( 0 ); }
  void set_next_maintenance_time( time_point_sec now, int64_t next_maintenance_delta_seconds )
  {
    last_budget_time = now;
    next_maintenance_time = now + fc::seconds( next_maintenance_delta_seconds );
  }
  void set_next_daily_maintenance_time( time_point_sec now, int64_t next_maintenance_delta_seconds )
  {
    next_daily_maintenance_time = now + fc::seconds( next_maintenance_delta_seconds );
  }

  uint32_t get_head_block_number() const { return head_block_number; }
  const block_id_type& get_head_block_id() const { return head_block_id; }
  time_point_sec get_head_block_time() const { return time; }
  const account_name_type& get_current_witness() const { return current_witness; }
  uint64_t get_current_aslot() const { return current_aslot; }
  fc::uint128_t get_recent_slots_filled() const { return recent_slots_filled; }
  uint8_t get_participation_count() const { return participation_count; }

  void set_new_head_block( uint32_t number, const block_id_type& id, time_point_sec ts )
  {
    head_block_number = number;
    head_block_id = id;
    time = ts;
  }
  void set_current_witness( const account_name_type& witness ) { current_witness = witness; }
  void set_current_aslot( uint64_t slot ) { current_aslot = slot; }
  void set_recent_slots_filled( fc::uint128_t filled_slots_bitmap ) { recent_slots_filled = filled_slots_bitmap; }
  void set_participation_count( uint8_t count ) { participation_count = count; }

  uint64_t get_total_pow() const { return total_pow; }
  uint32_t get_current_pow_witnesses() const { return num_pow_witnesses; }

  void on_pow_witness() { ++total_pow; ++num_pow_witnesses; } // when new pow is handled
  void remove_pow_witness() { --num_pow_witnesses; } // when miner is removed from queue

  uint32_t get_maximum_block_size() const { return maximum_block_size; }
  void set_maximum_block_size( uint32_t size ) { maximum_block_size = size; }

// various settings only changed during hardforks

  uint32_t get_vote_power_reserve_rate() const { return vote_power_reserve_rate; }
  uint32_t get_delegation_return_period() const { return delegation_return_period; }
  uint64_t get_reverse_auction_seconds() const { return reverse_auction_seconds; }
  uint64_t get_early_voting_seconds() const { return early_voting_seconds; }
  uint64_t get_mid_voting_seconds() const { return mid_voting_seconds; }
  int64_t get_available_account_subsidies() const { return available_account_subsidies; }
  uint16_t get_hbd_stop_percent() const { return hbd_stop_percent; }
  uint16_t get_hbd_start_percent() const { return hbd_start_percent; }
  uint16_t get_content_reward_percent() const { return content_reward_percent; }
  uint16_t get_vesting_reward_percent() const { return vesting_reward_percent; }
  uint16_t get_proposal_fund_percent() const { return proposal_fund_percent; }
  uint16_t get_downvote_pool_percent() const { return downvote_pool_percent; }
  int16_t get_current_remove_threshold() const { return current_remove_threshold; }
  uint8_t get_max_consecutive_recurrent_transfer_failures() const { return max_consecutive_recurrent_transfer_failures; }
  uint16_t get_max_recurrent_transfer_end_date() const { return max_recurrent_transfer_end_date; }
  uint8_t get_min_recurrent_transfers_recurrence() const { return min_recurrent_transfers_recurrence; }
  uint16_t get_max_open_recurrent_transfers() const { return max_open_recurrent_transfers; }

  void set_vote_power_reserve_rate( uint32_t value ) { vote_power_reserve_rate = value; }
  void set_delegation_return_period( uint32_t value ) { delegation_return_period = value; }
  void set_reverse_auction_seconds( uint64_t value ) { reverse_auction_seconds = value; }
  void set_early_voting_seconds( uint64_t value ) { early_voting_seconds = value; }
  void set_mid_voting_seconds( uint64_t value ) { mid_voting_seconds = value; }
  void set_available_account_subsidies( int64_t value ) { available_account_subsidies = value; }
  void set_hbd_stop_percent( uint16_t value ) { hbd_stop_percent = value; }
  void set_hbd_start_percent( uint16_t value ) { hbd_start_percent = value; }
  void set_content_reward_percent( uint16_t value ) { content_reward_percent = value; }
  void set_vesting_reward_percent( uint16_t value ) { vesting_reward_percent = value; }
  void set_proposal_fund_percent( uint16_t value ) { proposal_fund_percent = value; }
  void set_downvote_pool_percent( uint16_t value ) { downvote_pool_percent = value; }
  void set_current_remove_threshold( int16_t value ) { current_remove_threshold = value; }
  void set_max_consecutive_recurrent_transfer_failures( uint8_t value ) { max_consecutive_recurrent_transfer_failures = value; }
  void set_max_recurrent_transfer_end_date( uint16_t value ) { max_recurrent_transfer_end_date = value; }
  void set_min_recurrent_transfers_recurrence( uint8_t value ) { min_recurrent_transfers_recurrence = value; }
  void set_max_open_recurrent_transfers( uint16_t value ) { max_open_recurrent_transfers = value; }

  // dgpo is never removed so this routine should never be called
  void check_on_remove() const
  {
    FC_ASSERT( total_vesting_fund_hive.is_empty(), "Removing dgpo with non-empty balance field" );
  }

private:
  uint32_t          head_block_number = 0;
  block_id_type     head_block_id;
  time_point_sec    time = HIVE_GENESIS_TIME;
  account_name_type current_witness; //< TODO: check if worth replacing with account_id_type (ABW: probably not)

  //The total POW accumulated, aka the sum of num_pow_witness at the time new POW is added
  uint64_t total_pow = -1;

  //The current count of how many pending POW witnesses there are, determines the difficulty of doing pow
  uint32_t num_pow_witnesses = 0;

  HIVE_asset   virtual_supply;
  HIVE_asset   current_supply;
  HBD_asset    init_hbd_supply;
  HBD_asset    current_hbd_supply;
  HIVE_balance total_vesting_fund_hive;
  VEST_asset   total_vesting_shares;
  VEST_asset   pending_rewarded_vesting_shares;
  HIVE_asset   pending_rewarded_vesting_hive; // not a balance, just total sum of account_object::reward_vesting_hive

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
    * The current absolute slot number.  Equal to the total
    * number of slots since genesis.  Also equal to the total
    * number of missed slots plus head_block_number.
    */
  uint64_t      current_aslot = 0;

  /**
    * used to compute witness participation.
    */
  fc::uint128_t recent_slots_filled = fc::uint128_max_value();
  uint8_t       participation_count = 128; ///< Divide by 128 to compute participation percentage

  /**
    * The number of votes regenerated per day.  Any user voting slower than this rate will be
    * "wasting" voting power through spillover; any user voting faster than this rate will
    * eventually exhaust their voting mana (since HF28, previously their vote was reduced).
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

  HBD_asset dhf_interval_ledger = HBD_asset( 0 );

  uint16_t downvote_pool_percent = 0;

  // limits number of objects removed in one automatic operation (only applies to situations where many
  // objects can accumulate over time but need to be removed in single operation f.e. proposal votes)
  int16_t current_remove_threshold = HIVE_GLOBAL_REMOVE_THRESHOLD; //negative means no limit

  uint8_t max_consecutive_recurrent_transfer_failures = HIVE_MAX_CONSECUTIVE_RECURRENT_TRANSFER_FAILURES;
  uint16_t max_recurrent_transfer_end_date = HIVE_MAX_RECURRENT_TRANSFER_END_DATE;
  uint8_t min_recurrent_transfers_recurrence = HIVE_MIN_RECURRENT_TRANSFERS_RECURRENCE;
  uint16_t max_open_recurrent_transfers = HIVE_MAX_OPEN_RECURRENT_TRANSFERS;

  CHAINBASE_UNPACK_CONSTRUCTOR( dynamic_global_property_object );
};

inline temp_HIVE_balance dynamic_global_property_object::issue_HIVE( const HIVE_asset& hive_amount )
{
  temp_HIVE_balance hive_balance;
  hive_balance.issue_asset( hive_amount );
  current_supply += hive_amount;
  virtual_supply += hive_amount;
  return hive_balance;
}

inline void dynamic_global_property_object::burn_HIVE( temp_HIVE_balance& hive_balance )
{
  const HIVE_asset& hive_amount = hive_balance;
  current_supply -= hive_amount;
  virtual_supply -= hive_amount;
  hive_balance.burn_asset( hive_amount );
}

inline temp_HBD_balance dynamic_global_property_object::issue_HBD( const HBD_asset& hbd_amount, const HBD_price& exchange_rate, bool full_update_virtual_supply )
{
  temp_HBD_balance hbd_balance;
  hbd_balance.issue_asset( hbd_amount );
  current_hbd_supply += hbd_amount;
  if( full_update_virtual_supply )
    virtual_supply = current_hbd_supply * exchange_rate + current_supply;
  else
    virtual_supply += hbd_amount * exchange_rate;
  return hbd_balance;
}

inline void dynamic_global_property_object::burn_HBD( temp_HBD_balance& hbd_balance, const HBD_price& exchange_rate, bool full_update_virtual_supply )
{
  const HBD_asset& hbd_amount = hbd_balance;
  current_hbd_supply -= hbd_amount;
  if( full_update_virtual_supply )
    virtual_supply = current_hbd_supply * exchange_rate + current_supply;
  else
    virtual_supply -= hbd_amount * exchange_rate;
  hbd_balance.burn_asset( hbd_amount );
}

inline temp_HBD_balance dynamic_global_property_object::convert_HIVE_to_HBD( temp_HIVE_balance& hive_balance, const HBD_price& exchange_rate, bool full_update_virtual_supply )
{
  temp_HBD_balance hbd_balance;
  const HIVE_asset& hive_amount = hive_balance;
  HBD_asset hbd_amount = hive_amount * exchange_rate;
  hbd_balance.issue_asset( hbd_amount );
  current_hbd_supply += hbd_amount;
  current_supply -= hive_amount;
  hive_balance.burn_asset( hive_amount );
  //virtual_supply does not change in principle, but this routine is a replacement for one that did full update which means it repaired prior accumulated truncations
  if( full_update_virtual_supply )
    virtual_supply = current_hbd_supply * exchange_rate + current_supply;
  return hbd_balance;
}

inline temp_VEST_balance dynamic_global_property_object::issue_VESTS( const VEST_asset& vest_amount, bool is_reward )
{
  temp_VEST_balance vest_balance;
  vest_balance.issue_asset( vest_amount );
  if( is_reward )
    pending_rewarded_vesting_shares += vest_amount;
  else
    total_vesting_shares += vest_amount;
  return vest_balance;
}

inline void dynamic_global_property_object::burn_VESTS( temp_VEST_balance& vest_balance, bool is_reward )
{
  const VEST_asset& vest_amount = vest_balance;
  if( is_reward )
    pending_rewarded_vesting_shares -= vest_amount;
  else
    total_vesting_shares -= vest_amount;
  vest_balance.burn_asset( vest_amount );
}

inline temp_VEST_balance dynamic_global_property_object::convert_HIVE_to_VESTS( temp_HIVE_balance& hive_balance, bool is_reward )
{
  VEST_price vest_price = is_reward ? get_reward_vesting_share_price() : get_vesting_share_price();
  return convert_HIVE_to_VESTS( hive_balance, vest_price, is_reward );
}

inline temp_VEST_balance dynamic_global_property_object::convert_HIVE_to_VESTS( temp_HIVE_balance& hive_balance, const VEST_price& vest_price, bool is_reward )
{
  const HIVE_asset& hive_amount = hive_balance;
  VEST_asset vest_amount = hive_amount * vest_price;
  if( is_reward )
    pending_rewarded_vesting_hive += hive_amount;
  else
    total_vesting_fund_hive.transfer_from( hive_balance );
  return issue_VESTS( vest_amount, is_reward );
}

inline temp_HIVE_balance dynamic_global_property_object::convert_VESTS_to_HIVE( temp_VEST_balance& vest_balance, const VEST_price& vest_price )
{
  temp_HIVE_balance hive_balance;
  const VEST_asset& vest_amount = vest_balance;
  HIVE_asset hive_amount = vest_amount * vest_price;
  hive_balance.transfer_from( total_vesting_fund_hive, hive_amount );
  burn_VESTS( vest_balance );
  return hive_balance;
}

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
          (pending_rewarded_vesting_shares)
          (pending_rewarded_vesting_hive)
          (hbd_interest_rate)
          (hbd_print_rate)
          (maximum_block_size)
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
        )
