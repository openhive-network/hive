#pragma once

#include <hive/protocol/base.hpp>
#include <hive/protocol/asset.hpp>
#include <hive/protocol/config.hpp>
#include <hive/protocol/misc_utilities.hpp>

#ifdef HIVE_ENABLE_SMT

#define SMT_MAX_UNIT_ROUTES            10
#define SMT_MAX_UNIT_COUNT             20
#define SMT_MAX_DECIMAL_PLACES         8
#define SMT_MIN_HARD_CAP_HIVE_UNITS   10000
#define SMT_MIN_HARD_CAP_HIVE_UNITS    10000
#define SMT_MIN_SATURATION_HIVE_UNITS  1000
#define SMT_MIN_SOFT_CAP_HIVE_UNITS   1000
#define SMT_MIN_SOFT_CAP_HIVE_UNITS    1000

namespace hive { namespace protocol {


/**
  * This operation introduces new SMT into blockchain as identified by
  * Numerical Asset Identifier (NAI). Also the SMT precision (decimal points)
  * is explicitly provided.
  */
struct smt_create_operation : public base_operation
{
  account_name_type control_account;
  asset_symbol_type symbol;

  /// The amount to be transfered from @account to null account as elevation fee.
  asset             smt_creation_fee;
  /// Separately provided precision for clarity and redundancy.
  uint8_t           precision;

  extensions_type   extensions;

  void validate()const;

  void get_required_active_authorities( flat_set<account_name_type>& a )const
  { a.insert( control_account ); }
};

struct smt_generation_unit
{
  flat_map< account_name_type, uint16_t >        hive_unit;
  flat_map< account_name_type, uint16_t >        token_unit;

  uint32_t hive_unit_sum()const;
  uint32_t token_unit_sum()const;

  void validate()const;
};

struct smt_capped_generation_policy
{
  smt_generation_unit pre_soft_cap_unit;
  smt_generation_unit post_soft_cap_unit;

  uint16_t            soft_cap_percent = 0;

  uint32_t            min_unit_ratio = 0;
  uint32_t            max_unit_ratio = 0;

  extensions_type     extensions;

  void validate()const;
};

typedef static_variant<
  smt_capped_generation_policy
  > smt_generation_policy;

struct smt_setup_operation : public base_operation
{
  account_name_type control_account;
  asset_symbol_type symbol;

  int64_t                 max_supply = HIVE_MAX_SHARE_SUPPLY;

  smt_generation_policy   initial_generation_policy;

  time_point_sec          contribution_begin_time;
  time_point_sec          contribution_end_time;
  time_point_sec          launch_time;

  share_type              hive_units_soft_cap;
  share_type              hive_units_hard_cap;

  extensions_type         extensions;

  void validate()const;

  void get_required_active_authorities( flat_set<account_name_type>& a )const
  { a.insert( control_account ); }
};

struct smt_emissions_unit
{
  flat_map< account_name_type, uint16_t >        token_unit;
};

struct smt_setup_emissions_operation : public base_operation
{
  account_name_type   control_account;
  asset_symbol_type   symbol;

  time_point_sec      schedule_time;
  smt_emissions_unit  emissions_unit;

  uint32_t            interval_seconds = 0;
  uint32_t            interval_count = 0;

  time_point_sec      lep_time;
  time_point_sec      rep_time;

  asset               lep_abs_amount;
  asset               rep_abs_amount;
  uint32_t            lep_rel_amount_numerator = 0;
  uint32_t            rep_rel_amount_numerator = 0;

  uint8_t             rel_amount_denom_bits = 0;
  bool                remove = false;

  extensions_type     extensions;

  void validate()const;

  void get_required_active_authorities( flat_set<account_name_type>& a )const
  { a.insert( control_account ); }
};

struct smt_param_allow_voting
{
  bool value = true;
};

typedef static_variant<
  smt_param_allow_voting
  > smt_setup_parameter;

struct smt_param_windows_v1
{
  uint32_t cashout_window_seconds = 0;                // HIVE_CASHOUT_WINDOW_SECONDS
  uint32_t reverse_auction_window_seconds = 0;        // HIVE_REVERSE_AUCTION_WINDOW_SECONDS
};

struct smt_param_vote_regeneration_period_seconds_v1
{
  uint32_t vote_regeneration_period_seconds = 0;      // HIVE_VOTING_MANA_REGENERATION_SECONDS
  uint32_t votes_per_regeneration_period = 0;
};

struct smt_param_rewards_v1
{
  uint128_t               content_constant = 0;
  uint16_t                percent_curation_rewards = 0;
  protocol::curve_id      author_reward_curve;
  protocol::curve_id      curation_reward_curve;
};

struct smt_param_allow_downvotes
{
  bool value = true;
};

typedef static_variant<
  smt_param_windows_v1,
  smt_param_vote_regeneration_period_seconds_v1,
  smt_param_rewards_v1,
  smt_param_allow_downvotes
  > smt_runtime_parameter;

struct smt_set_setup_parameters_operation : public base_operation
{
  account_name_type                control_account;
  asset_symbol_type                symbol;
  flat_set< smt_setup_parameter >  setup_parameters;
  extensions_type                  extensions;

  void validate()const;

  void get_required_active_authorities( flat_set<account_name_type>& a )const
  { a.insert( control_account ); }
};

struct smt_set_runtime_parameters_operation : public base_operation
{
  account_name_type                   control_account;
  asset_symbol_type                   symbol;
  flat_set< smt_runtime_parameter >   runtime_parameters;
  extensions_type                     extensions;

  void validate()const;

  void get_required_active_authorities( flat_set<account_name_type>& a )const
  { a.insert( control_account ); }
};

struct smt_contribute_operation : public base_operation
{
  account_name_type  contributor;
  asset_symbol_type  symbol;
  uint32_t           contribution_id;
  asset              contribution;
  extensions_type    extensions;

  void validate() const;
  void get_required_active_authorities( flat_set<account_name_type>& a )const
  { a.insert( contributor ); }
};

} }

FC_REFLECT(
  hive::protocol::smt_create_operation,
  (control_account)
  (symbol)
  (smt_creation_fee)
  (extensions)
)

FC_REFLECT(
  hive::protocol::smt_setup_operation,
  (control_account)
  (symbol)
  (max_supply)
  (initial_generation_policy)
  (contribution_begin_time)
  (contribution_end_time)
  (launch_time)
  (hive_units_soft_cap)
  (hive_units_hard_cap)
  (extensions)
  )

FC_REFLECT(
  hive::protocol::smt_generation_unit,
  (hive_unit)
  (token_unit)
  )


FC_REFLECT(
  hive::protocol::smt_capped_generation_policy,
  (pre_soft_cap_unit)
  (post_soft_cap_unit)
  (soft_cap_percent)
  (min_unit_ratio)
  (max_unit_ratio)
  (extensions)
  )

FC_REFLECT(
  hive::protocol::smt_emissions_unit,
  (token_unit)
  )

FC_REFLECT(
  hive::protocol::smt_setup_emissions_operation,
  (control_account)
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
  (remove)
  (extensions)
  )

FC_REFLECT(
  hive::protocol::smt_param_allow_voting,
  (value)
  )

FC_REFLECT_TYPENAME( hive::protocol::smt_setup_parameter )

FC_REFLECT(
  hive::protocol::smt_param_windows_v1,
  (cashout_window_seconds)
  (reverse_auction_window_seconds)
  )

FC_REFLECT(
  hive::protocol::smt_param_vote_regeneration_period_seconds_v1,
  (vote_regeneration_period_seconds)
  (votes_per_regeneration_period)
  )

FC_REFLECT(
  hive::protocol::smt_param_rewards_v1,
  (content_constant)
  (percent_curation_rewards)
  (author_reward_curve)
  (curation_reward_curve)
  )

FC_REFLECT(
  hive::protocol::smt_param_allow_downvotes,
  (value)
)

FC_REFLECT_TYPENAME(
  hive::protocol::smt_runtime_parameter
  )

FC_REFLECT(
  hive::protocol::smt_set_setup_parameters_operation,
  (control_account)
  (symbol)
  (setup_parameters)
  (extensions)
  )

FC_REFLECT(
  hive::protocol::smt_set_runtime_parameters_operation,
  (control_account)
  (symbol)
  (runtime_parameters)
  (extensions)
  )

FC_REFLECT(
  hive::protocol::smt_contribute_operation,
  (contributor)
  (symbol)
  (contribution_id)
  (contribution)
  (extensions)
  )

#endif
