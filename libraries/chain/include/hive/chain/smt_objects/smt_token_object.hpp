#pragma once

#include <hive/chain/hive_fwd.hpp>
#include <hive/chain/hive_object_types.hpp>
#include <hive/protocol/smt_operations.hpp>

#ifdef HIVE_ENABLE_SMT

namespace hive { namespace chain {

using protocol::curve_id;

enum class smt_phase : uint8_t
{
  account_elevated,
  setup_completed,
  contribution_begin_time_completed,
  contribution_end_time_completed,
  launch_failed,                      /// launch window closed with either not enough contributions or some cap not revealed
  launch_success                      /// enough contributions were declared and caps revealed before launch windows closed
};

/**Note that the object represents both liquid and vesting variant of SMT.
  * The same object is returned by indices when searched by liquid/vesting symbol/nai.
  */
class smt_token_object : public object< smt_token_object_type, smt_token_object >
{
  CHAINBASE_OBJECT( smt_token_object );

public:
  struct smt_market_maker_state
  {
    uint32_t   reserve_ratio = 0;
    HIVE_asset hive_balance;
    asset      token_balance;
  };

public:
  template< typename Allocator >
  smt_token_object( allocator< Allocator > a, uint64_t _id,
    asset_symbol_type _liquid_symbol, const account_name_type& _control_account )
    : id( _id ), liquid_symbol( _liquid_symbol ), control_account( _control_account )
  {
    market_maker.token_balance = asset( 0, liquid_symbol );
  }

  price one_vesting_to_one_liquid() const
  {
    int64_t one_smt = std::pow(10, liquid_symbol.decimals());
    return price ( asset( one_smt, liquid_symbol.get_paired_symbol() ), asset( one_smt, liquid_symbol ) );
    // ^ On the assumption that liquid and vesting SMT have the same precision. See issue 2212
  }

  price get_vesting_share_price() const
  {
    if ( total_vesting_fund_smt == 0 || total_vesting_shares == 0 )
      return one_vesting_to_one_liquid();
      // ^ In original method of globa_property_object it was one liquid to one vesting which seems to be a bug.

    return price( asset( total_vesting_shares, liquid_symbol.get_paired_symbol() ), asset( total_vesting_fund_smt, liquid_symbol ) );
  }

  price get_reward_vesting_share_price() const
  {
    share_type reward_vesting_shares = total_vesting_shares + pending_rewarded_vesting_shares;
    share_type reward_vesting_smt = total_vesting_fund_smt + pending_rewarded_vesting_smt;

    if( reward_vesting_shares == 0 || reward_vesting_smt == 0 )
        return one_vesting_to_one_liquid();
    // ^ Additional check not found in original get_reward_vesting_share_price. See issue 2212

    return price( asset( reward_vesting_shares, liquid_symbol.get_paired_symbol() ), asset( reward_vesting_smt, liquid_symbol ) );
  }

  /**The object represents both liquid and vesting variant of SMT
    * To get vesting symbol, call liquid_symbol.get_paired_symbol()
    */
  asset_symbol_type    liquid_symbol;
  account_name_type    control_account;
  smt_phase            phase = smt_phase::account_elevated;

  share_type           current_supply = 0;
  share_type           total_vesting_fund_smt = 0;
  share_type           total_vesting_shares = 0;
  share_type           pending_rewarded_vesting_shares = 0;
  share_type           pending_rewarded_vesting_smt = 0;

  smt_market_maker_state  market_maker;

  /// set_setup_parameters
  bool                 allow_voting = true;

  /// set_runtime_parameters
  uint32_t             cashout_window_seconds = HIVE_CASHOUT_WINDOW_SECONDS;
  uint32_t             reverse_auction_window_seconds = HIVE_REVERSE_AUCTION_WINDOW_SECONDS_HF20;

  uint32_t             vote_regeneration_period_seconds = HIVE_VOTING_MANA_REGENERATION_SECONDS;
  uint32_t             votes_per_regeneration_period = SMT_DEFAULT_VOTES_PER_REGEN_PERIOD;

  uint128_t            content_constant = HIVE_CONTENT_CONSTANT_HF0;
  uint16_t             percent_curation_rewards = SMT_DEFAULT_PERCENT_CURATION_REWARDS;
  protocol::curve_id   author_reward_curve = curve_id::linear;
  protocol::curve_id   curation_reward_curve = curve_id::square_root;

  bool                 allow_downvotes = true;

  ///parameters for 'smt_setup_operation'
  int64_t              max_supply = 0;

  CHAINBASE_UNPACK_CONSTRUCTOR(smt_token_object);
};

class smt_ico_object : public object< smt_ico_object_type, smt_ico_object >
{
  CHAINBASE_OBJECT( smt_ico_object );

public:
  CHAINBASE_DEFAULT_CONSTRUCTOR( smt_ico_object )

  asset_symbol_type             symbol;
  hive::protocol::
  smt_capped_generation_policy  capped_generation_policy;
  time_point_sec                contribution_begin_time;
  time_point_sec                contribution_end_time;
  time_point_sec                launch_time;
  share_type                    hive_units_soft_cap = -1;
  share_type                    hive_units_hard_cap = -1;
  asset                         contributed = asset( 0, HIVE_SYMBOL );

  CHAINBASE_UNPACK_CONSTRUCTOR(smt_ico_object);
};

class smt_token_emissions_object : public object< smt_token_emissions_object_type, smt_token_emissions_object >
{
  CHAINBASE_OBJECT( smt_token_emissions_object );

public:
  CHAINBASE_DEFAULT_CONSTRUCTOR( smt_token_emissions_object )

  asset_symbol_type                     symbol;
  time_point_sec                        schedule_time = HIVE_GENESIS_TIME;
  hive::protocol::smt_emissions_unit    emissions_unit;
  uint32_t                              interval_seconds = 0;
  uint32_t                              interval_count = 0;
  time_point_sec                        lep_time = HIVE_GENESIS_TIME;
  time_point_sec                        rep_time = HIVE_GENESIS_TIME;
  asset                                 lep_abs_amount = asset();
  asset                                 rep_abs_amount = asset();
  uint32_t                              lep_rel_amount_numerator = 0;
  uint32_t                              rep_rel_amount_numerator = 0;
  uint8_t                               rel_amount_denom_bits = 0;

  CHAINBASE_UNPACK_CONSTRUCTOR(smt_token_emissions_object);
};

class smt_contribution_object : public object< smt_contribution_object_type, smt_contribution_object >
{
  CHAINBASE_OBJECT( smt_contribution_object );

public:
  template< typename Allocator >
  smt_contribution_object( allocator< Allocator > a, uint64_t _id,
    const account_name_type& _contributor, const asset& _contribution, const asset_symbol_type& _smt_symbol, uint32_t _contribution_id )
    : id( _id ), symbol( _smt_symbol ), contributor( _contributor ), contribution_id( _contribution_id ), contribution( _contribution )
  {}

  asset_symbol_type symbol;
  account_name_type contributor;
  uint32_t          contribution_id;
  HIVE_asset        contribution;

  CHAINBASE_UNPACK_CONSTRUCTOR(smt_contribution_object);
};

struct by_symbol_contributor;
struct by_contributor;
struct by_symbol_id;

typedef multi_index_container <
  smt_contribution_object,
  indexed_by <
    ordered_unique< tag< by_id >,
      const_mem_fun< smt_contribution_object, smt_contribution_object::id_type, &smt_contribution_object::get_id > >,
    ordered_unique< tag< by_symbol_contributor >,
      composite_key< smt_contribution_object,
        member< smt_contribution_object, asset_symbol_type, &smt_contribution_object::symbol >,
        member< smt_contribution_object, account_name_type, &smt_contribution_object::contributor >,
        member< smt_contribution_object, uint32_t, &smt_contribution_object::contribution_id >
      >
    >,
    ordered_unique< tag< by_symbol_id >,
      composite_key< smt_contribution_object,
        member< smt_contribution_object, asset_symbol_type, &smt_contribution_object::symbol >,
        const_mem_fun< smt_contribution_object, smt_contribution_object::id_type, &smt_contribution_object::get_id >
      >
    >
// #ifndef IS_LOW_MEM // indexing by contributor might cause optimization problems in the future
    ,
    ordered_unique< tag< by_contributor >,
      composite_key< smt_contribution_object,
        member< smt_contribution_object, account_name_type, &smt_contribution_object::contributor >,
        member< smt_contribution_object, asset_symbol_type, &smt_contribution_object::symbol >,
        member< smt_contribution_object, uint32_t, &smt_contribution_object::contribution_id >
      >
    >
// #endif
  >,
  allocator< smt_contribution_object >
> smt_contribution_index;

struct by_symbol;
struct by_control_account;

typedef multi_index_container <
  smt_token_object,
  indexed_by <
    ordered_unique< tag< by_id >,
      const_mem_fun< smt_token_object, smt_token_object::id_type, &smt_token_object::get_id > >,
    ordered_unique< tag< by_symbol >,
      member< smt_token_object, asset_symbol_type, &smt_token_object::liquid_symbol > >,
    ordered_unique< tag< by_control_account >,
      composite_key< smt_token_object,
        member< smt_token_object, account_name_type, &smt_token_object::control_account >,
        member< smt_token_object, asset_symbol_type, &smt_token_object::liquid_symbol >
      >
    >
  >,
  allocator< smt_token_object >
> smt_token_index;

typedef multi_index_container <
  smt_ico_object,
  indexed_by <
    ordered_unique< tag< by_id >,
      const_mem_fun< smt_ico_object, smt_ico_object::id_type, &smt_ico_object::get_id > >,
    ordered_unique< tag< by_symbol >,
      member< smt_ico_object, asset_symbol_type, &smt_ico_object::symbol > >
  >,
  allocator< smt_ico_object >
> smt_ico_index;

struct by_symbol_time;

typedef multi_index_container <
  smt_token_emissions_object,
  indexed_by <
    ordered_unique< tag< by_id >,
      const_mem_fun< smt_token_emissions_object, smt_token_emissions_object::id_type, &smt_token_emissions_object::get_id > >,
    ordered_unique< tag< by_symbol_time >,
      composite_key< smt_token_emissions_object,
        member< smt_token_emissions_object, asset_symbol_type, &smt_token_emissions_object::symbol >,
        member< smt_token_emissions_object, time_point_sec, &smt_token_emissions_object::schedule_time >
      >
    >
  >,
  allocator< smt_token_emissions_object >
> smt_token_emissions_index;

} } // namespace hive::chain

FC_REFLECT_ENUM( hive::chain::smt_phase,
            (account_elevated)
            (setup_completed)
            (contribution_begin_time_completed)
            (contribution_end_time_completed)
            (launch_failed)
            (launch_success)
)

FC_REFLECT( hive::chain::smt_token_object::smt_market_maker_state,
  (reserve_ratio)
  (hive_balance)
  (token_balance)
)

FC_REFLECT( hive::chain::smt_token_object,
  (id)
  (liquid_symbol)
  (control_account)
  (phase)
  (current_supply)
  (total_vesting_fund_smt)
  (total_vesting_shares)
  (pending_rewarded_vesting_shares)
  (pending_rewarded_vesting_smt)
  (allow_downvotes)
  (market_maker)
  (allow_voting)
  (cashout_window_seconds)
  (reverse_auction_window_seconds)
  (vote_regeneration_period_seconds)
  (votes_per_regeneration_period)
  (content_constant)
  (percent_curation_rewards)
  (author_reward_curve)
  (curation_reward_curve)
  (max_supply)
)

FC_REFLECT( hive::chain::smt_ico_object,
  (id)
  (symbol)
  (capped_generation_policy)
  (contribution_begin_time)
  (contribution_end_time)
  (launch_time)
  (hive_units_soft_cap)
  (hive_units_hard_cap)
  (contributed)
)

FC_REFLECT( hive::chain::smt_token_emissions_object,
  (id)
  (symbol)
  (schedule_time)
  (emissions_unit)
  (interval_seconds)
  (interval_count)
  (lep_time)
  (rep_time)
  (lep_abs_amount)
  (rep_abs_amount)
  (lep_rel_amount_numerator)
  (rep_rel_amount_numerator)
  (rel_amount_denom_bits)
)

FC_REFLECT( hive::chain::smt_contribution_object,
  (id)
  (symbol)
  (contributor)
  (contribution_id)
  (contribution)
)

CHAINBASE_SET_INDEX_TYPE( hive::chain::smt_token_object, hive::chain::smt_token_index )
CHAINBASE_SET_INDEX_TYPE( hive::chain::smt_ico_object, hive::chain::smt_ico_index )
CHAINBASE_SET_INDEX_TYPE( hive::chain::smt_token_emissions_object, hive::chain::smt_token_emissions_index )
CHAINBASE_SET_INDEX_TYPE( hive::chain::smt_contribution_object, hive::chain::smt_contribution_index )

#endif
