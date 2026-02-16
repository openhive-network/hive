#pragma once

#include <hive/protocol/types.hpp>
#include <hive/protocol/authority.hpp>
#include <hive/protocol/asset.hpp>
#include <hive/protocol/block.hpp>
#include <hive/protocol/misc_utilities.hpp>
#include <hive/protocol/comment_types.hpp>

#include <hive/chain/hive_object_types.hpp>
#include <hive/chain/util/manabar.hpp>
#include <hive/chain/util/asset.hpp>
#include <hive/chain/util/delayed_voting.hpp>
#include <hive/chain/util/rd_dynamics.hpp>
#include <hive/chain/witness_objects.hpp>
#include <hive/plugins/metadata_api/metadata_api_plugin.hpp>

#include <hive/plugins/metadata/metadata_plugin.hpp>

#ifdef HIVE_ENABLE_SMT
#include <hive/chain/smt_objects/smt_token_object.hpp>
#endif

#include <fc/optional.hpp>
#include <fc/container/flat.hpp>

namespace hive { namespace chain {
  // Forward declarations
  class database;
  class reward_fund_object;
  class witness_vote_object;
  class escrow_object;
  class withdraw_vesting_route_object;
  class vesting_delegation_object;
  class vesting_delegation_expiration_object;
  class convert_request_object;
  class collateralized_convert_request_object;
  class decline_voting_rights_request_object;
  class limit_order_object;
  class dynamic_global_property_object;
  class change_recovery_account_request_object;
  class comment_object;
  class comment_cashout_object;
  class comment_vote_object;
  class account_object;
  class account_authority_object;
  class account_metadata_object;
  class owner_authority_history_object;
  class account_recovery_request_object;
  class savings_withdraw_object;
  class feed_history_object;
  class witness_object;
  class witness_schedule_object;
  class hardfork_property_object;
  class smt_token_object;
  class smt_ico_object;
  class smt_token_emissions_object;
  class smt_contribution_object;
  class proposal_object;
  class proposal_vote_object;
  class recurrent_transfer_object;
  class full_block_type;
  class full_transaction_type;

  // Type alias to avoid including hive_objects.hpp
  using reward_fund_name_type = protocol::fixed_string<16>;
} }

#include <hive/plugins/metadata_api/metadata_api.hpp>
#include <hive/plugins/metadata/metadata_objects.hpp>

namespace hive { namespace plugins { namespace database_api {

using namespace hive::chain;

struct api_reward_fund_object
{
  api_reward_fund_object() = default;
  api_reward_fund_object( const reward_fund_object& o, const database& db );

  reward_fund_id_type     id;
  reward_fund_name_type   name;
  asset                   reward_balance;
  uint128_t               recent_claims = 0;
  time_point_sec          last_update;
  uint128_t               content_constant = 0;
  uint16_t                percent_curation_rewards = 0;
  uint16_t                percent_content_rewards = 0;
  protocol::curve_id      author_reward_curve = protocol::quadratic;
  protocol::curve_id      curation_reward_curve = protocol::quadratic;
};

struct api_witness_vote_object
{
  api_witness_vote_object( const witness_vote_object& o, const database& db );

  witness_vote_id_type id;
  account_name_type    witness;
  account_name_type    account;
};

struct api_escrow_object
{
  api_escrow_object( const escrow_object& o, const database& db );

  escrow_id_type    id;
  uint32_t          escrow_id;
  account_name_type from;
  account_name_type to;
  account_name_type agent;
  time_point_sec    ratification_deadline;
  time_point_sec    escrow_expiration;
  asset             hbd_balance;
  asset             hive_balance;
  asset             pending_fee;
  bool              to_approved;
  bool              agent_approved;
  bool              disputed;
};

struct api_withdraw_vesting_route_object
{
  api_withdraw_vesting_route_object( const withdraw_vesting_route_object& o, const database& db );
  api_withdraw_vesting_route_object() = default;

  withdraw_vesting_route_id_type id;
  account_name_type              from_account;
  account_name_type              to_account;
  uint16_t                       percent;
  bool                           auto_vest;
};

struct api_vesting_delegation_object
{
  api_vesting_delegation_object( const vesting_delegation_object& o, const database& db );

  vesting_delegation_id_type id;
  account_name_type          delegator;
  account_name_type          delegatee;
  asset                      vesting_shares;
  time_point_sec             min_delegation_time;
};

struct api_vesting_delegation_expiration_object
{
  api_vesting_delegation_expiration_object( const vesting_delegation_expiration_object& o, const database& db );

  vesting_delegation_expiration_id_type id;
  account_name_type                     delegator;
  asset                                 vesting_shares;
  time_point_sec                        expiration;
};

struct api_convert_request_object
{
  api_convert_request_object() = default;
  api_convert_request_object( const convert_request_object& o, const database& db );

  convert_request_id_type id;
  account_name_type       owner;
  uint32_t                requestid;
  asset                   amount;
  time_point_sec          conversion_date;
};

struct api_collateralized_convert_request_object
{
  api_collateralized_convert_request_object() = default;
  api_collateralized_convert_request_object( const collateralized_convert_request_object& o, const database& db );

  collateralized_convert_request_id_type id;
  account_name_type                      owner;
  uint32_t                               requestid;
  asset                                  collateral_amount;
  asset                                  converted_amount;
  time_point_sec                         conversion_date;
};

struct api_decline_voting_rights_request_object
{
  api_decline_voting_rights_request_object( const decline_voting_rights_request_object& o, const database& db );

  decline_voting_rights_request_id_type id;
  account_name_type                     account;
  time_point_sec                        effective_date;
};

struct api_limit_order_object
{
  api_limit_order_object( const limit_order_object& o, const database& db );
  api_limit_order_object() = default;

  limit_order_id_type id;
  time_point_sec      created;
  time_point_sec      expiration;
  account_name_type   seller;
  uint32_t            orderid;
  share_type          for_sale;
  price               sell_price;
};

struct api_dynamic_global_property_object
{
  api_dynamic_global_property_object( const dynamic_global_property_object& o, const database& db );
  api_dynamic_global_property_object() = default;

  dynamic_global_property_id_type id;
  uint32_t                        head_block_number                   = 0;
  block_id_type                   head_block_id;
  time_point_sec                  time;
  account_name_type               current_witness;
  uint64_t                        total_pow  = 0;
  uint32_t                        num_pow_witnesses                   = 0;
  asset                           virtual_supply;
  asset                           current_supply;
  asset                           init_hbd_supply;
  asset                           current_hbd_supply;
  asset                           total_vesting_fund_hive;
  asset                           total_vesting_shares;
  asset                           total_reward_fund_hive;
  fc::uint128                     total_reward_shares2 = 0;
  asset                           pending_rewarded_vesting_shares;
  asset                           pending_rewarded_vesting_hive;
  uint16_t                        hbd_interest_rate                   = 0;
  uint16_t                        hbd_print_rate                      = 0;
  uint32_t                        maximum_block_size                  = 0;
  uint64_t                        current_aslot                       = 0;
  fc::uint128_t                   recent_slots_filled                 = 0;
  uint8_t                         participation_count                 = 0;
  uint32_t                        last_irreversible_block_num         = 0;
  uint32_t                        vote_power_reserve_rate             = 0;
  uint32_t                        delegation_return_period            = 0;
  uint64_t                        reverse_auction_seconds             = 0;
  int64_t                         available_account_subsidies         = 0;
  uint16_t                        hbd_stop_percent                    = 0;
  uint16_t                        hbd_start_percent                   = 0;
  time_point_sec                  next_maintenance_time;
  time_point_sec                  last_budget_time;
  time_point_sec                  next_daily_maintenance_time;
  uint16_t                        content_reward_percent              = 0;
  uint16_t                        vesting_reward_percent              = 0;
  uint16_t                        proposal_fund_percent               = 0;
  asset                           dhf_interval_ledger;
  uint16_t                        downvote_pool_percent               = 0;
  int16_t                         current_remove_threshold            = 0;
  uint64_t                        early_voting_seconds                = 0;
  uint64_t                        mid_voting_seconds                  = 0;
  uint8_t                        max_consecutive_recurrent_transfer_failures = HIVE_MAX_CONSECUTIVE_RECURRENT_TRANSFER_FAILURES;
  uint16_t                        max_recurrent_transfer_end_date = HIVE_MAX_RECURRENT_TRANSFER_END_DATE;
  uint8_t                        min_recurrent_transfers_recurrence = HIVE_MIN_RECURRENT_TRANSFERS_RECURRENCE;
  uint16_t                        max_open_recurrent_transfers = HIVE_MAX_OPEN_RECURRENT_TRANSFERS;
#ifdef HIVE_ENABLE_SMT
  asset                           smt_creation_fee;
#endif
};

struct api_change_recovery_account_request_object
{
  api_change_recovery_account_request_object( const change_recovery_account_request_object& o, const database& db );

  change_recovery_account_request_id_type id;
  account_name_type                       account_to_recover;
  account_name_type                       recovery_account;
  time_point_sec                          effective_on;
};

struct api_commment_cashout_info
{
  api_commment_cashout_info(const comment_cashout_object& o, const database& db);

  uint64_t       total_vote_weight = 0;
  asset          total_payout_value;
  asset          curator_payout_value;
  asset          max_accepted_payout;

  share_type     author_rewards = 0;
  share_type     children_abs_rshares = 0;
  share_type     net_rshares = 0;
  share_type     abs_rshares = 0;
  share_type     vote_rshares = 0;

  int32_t        net_votes = 0;

  time_point_sec last_payout;
  time_point_sec cashout_time;
  time_point_sec max_cashout_time;

  uint16_t       percent_hbd = 0;
  uint16_t       reward_weight = 0;
  bool           allow_replies = false;
  bool           allow_votes = false;
  bool           allow_curation_rewards = false;
  bool           was_voted_on = false;
};

struct api_account_object
{
  api_account_object( const account_object& a, const database& db, const metadata::metadata_plugin* metadata_plugin, bool delayed_votes_active );
  api_account_object() = default;

  account_id_type   id;

  account_name_type name;
  authority         owner;
  authority         active;
  authority         posting;
  public_key_type   memo_key;
  string            json_metadata;
  string            posting_json_metadata;
  account_name_type proxy;

  time_point_sec    previous_owner_update;
  time_point_sec    last_owner_update;
  time_point_sec    last_account_update;

  time_point_sec    created;
  bool              mined = false;
  account_name_type recovery_account;
  account_name_type reset_account;
  time_point_sec    last_account_recovery;
  uint32_t          comment_count = 0; // always zero
  uint32_t          lifetime_vote_count = 0; // always zero
  uint32_t          post_count = 0;

  bool              can_vote = false;
  util::manabar     voting_manabar;
  util::manabar     downvote_manabar;

  asset             balance;
  asset             savings_balance;

  asset             hbd_balance;
  uint128_t         hbd_seconds = 0;
  time_point_sec    hbd_seconds_last_update;
  time_point_sec    hbd_last_interest_payment;

  asset             savings_hbd_balance;
  uint128_t         savings_hbd_seconds = 0;
  time_point_sec    savings_hbd_seconds_last_update;
  time_point_sec    savings_hbd_last_interest_payment;

  uint8_t           savings_withdraw_requests = 0;

  asset             reward_hbd_balance;
  asset             reward_hive_balance;
  asset             reward_vesting_balance;
  asset             reward_vesting_hive;

  share_type        curation_rewards;
  share_type        posting_rewards;

  asset             vesting_shares;
  asset             delegated_vesting_shares;
  asset             received_vesting_shares;
  asset             vesting_withdraw_rate;
  
  asset             post_voting_power;

  time_point_sec    next_vesting_withdrawal;
  share_type        withdrawn;
  share_type        to_withdraw;
  uint16_t          withdraw_routes = 0;
  uint16_t          pending_transfers = 0;

  vector< share_type > proxied_vsf_votes;

  uint16_t          witnesses_voted_for = 0;

  time_point_sec    last_post;
  time_point_sec    last_root_post;
  time_point_sec    last_post_edit;
  time_point_sec    last_vote_time;
  uint32_t          post_bandwidth = 0;

  share_type        pending_claimed_accounts = 0;
  uint16_t          open_recurrent_transfers = 0;

  bool              is_smt = false;

  fc::optional< vector< delayed_votes_data > >  delayed_votes;
  time_point_sec governance_vote_expiration_ts;
};

struct api_owner_authority_history_object
{
  api_owner_authority_history_object( const owner_authority_history_object& o, const database& db );
  api_owner_authority_history_object() = default;

  owner_authority_history_id_type  id;

  account_name_type                account;
  authority                        previous_owner_authority;
  time_point_sec                   last_valid_time;
};

struct api_account_recovery_request_object
{
  api_account_recovery_request_object( const account_recovery_request_object& o, const database& db );
  api_account_recovery_request_object() = default;

  account_recovery_request_id_type id;
  account_name_type                account_to_recover;
  authority                        new_owner_authority;
  time_point_sec                   expires;
};

struct api_account_history_object
{

};

struct api_savings_withdraw_object
{
  api_savings_withdraw_object( const savings_withdraw_object& o, const database& db );
  api_savings_withdraw_object() = default;

  savings_withdraw_id_type   id;
  account_name_type          from;
  account_name_type          to;
  string                     memo;
  uint32_t                   request_id = 0;
  asset                      amount;
  time_point_sec             complete;
};

struct api_feed_history_object
{
  api_feed_history_object( const feed_history_object& f );
  api_feed_history_object() = default;

  feed_history_id_type id;
  price                current_median_history;
  price                market_median_history;
  price                current_min_history;
  price                current_max_history;
  deque< price >       price_history;
};

struct api_witness_object
{
  api_witness_object( const witness_object& w, const database& db );
  api_witness_object() = default;

  witness_id_type   id;
  account_name_type owner;
  time_point_sec    created;
  string            url;
  uint32_t          total_missed = 0;
  uint64_t          last_aslot = 0;
  uint64_t          last_confirmed_block_num = 0;
  uint64_t          pow_worker = 0;
  public_key_type   signing_key;
  chain_properties  props;
  price             hbd_exchange_rate;
  time_point_sec    last_hbd_exchange_update;
  share_type        votes;
  fc::uint128       virtual_last_update = 0;
  fc::uint128       virtual_position = 0;
  fc::uint128       virtual_scheduled_time = 0;
  digest_type       last_work;
  version           running_version;
  hardfork_version  hardfork_version_vote;
  time_point_sec    hardfork_time_vote;
  int64_t           available_witness_account_subsidies = 0;
};

// only filled parts that are different between active and future wso
struct future_chain_properties
{
  future_chain_properties() = default;

  bool fill( const chain_properties& active, const chain_properties& future );

  fc::optional<asset>    account_creation_fee;
  fc::optional<uint32_t> maximum_block_size;
  fc::optional<uint16_t> hbd_interest_rate;
  fc::optional<int32_t>  account_subsidy_budget;
  fc::optional<uint32_t> account_subsidy_decay;
};

// only filled parts that are different between active and future wso
struct future_witness_schedule
{
  future_witness_schedule() = default;

  bool fill( const witness_schedule_object& active, const witness_schedule_object& future );

  fc::optional<uint8_t>                 num_scheduled_witnesses;
  fc::optional<uint8_t>                 elected_weight;
  fc::optional<uint8_t>                 timeshare_weight;
  fc::optional<uint8_t>                 miner_weight;
  fc::optional<uint8_t>                 witness_pay_normalization_factor;
  fc::optional<future_chain_properties> median_props;
  fc::optional<version>                 majority_version;

  fc::optional<uint8_t>                 max_voted_witnesses;
  fc::optional<uint8_t>                 max_miner_witnesses;
  fc::optional<uint8_t>                 max_runner_witnesses;
  fc::optional<uint8_t>                 hardfork_required_witnesses;

  fc::optional<rd_dynamics_params>      account_subsidy_rd;
  fc::optional<rd_dynamics_params>      account_subsidy_witness_rd;
  fc::optional<int64_t>                 min_witness_account_subsidy_decay;
};

struct api_witness_schedule_object
{
  api_witness_schedule_object() = default;
  api_witness_schedule_object( const witness_schedule_object& wso,
    const witness_schedule_object& future_wso, bool include_future, const database& db );

  witness_schedule_id_type              id; //always from active wso

  fc::uint128                           current_virtual_time = 0; //always from future wso
  uint32_t                              next_shuffle_block_num; //always from future wso
  vector<string>                        current_shuffled_witnesses; //from active wso
  fc::optional< vector<string> >        future_shuffled_witnesses; //from future wso (only filled on request)
  uint8_t                               num_scheduled_witnesses;
  uint8_t                               elected_weight;
  uint8_t                               timeshare_weight;
  uint8_t                               miner_weight;
  uint32_t                              witness_pay_normalization_factor;
  chain_properties                      median_props;
  version                               majority_version;

  uint8_t                               max_voted_witnesses;
  uint8_t                               max_miner_witnesses;
  uint8_t                               max_runner_witnesses;
  uint8_t                               hardfork_required_witnesses;

  rd_dynamics_params                    account_subsidy_rd;
  rd_dynamics_params                    account_subsidy_witness_rd;
  int64_t                               min_witness_account_subsidy_decay = 0;

  fc::optional<future_witness_schedule> future_changes; //only filled on request when there are changes
};

struct api_signed_block_object : public signed_block
{
  api_signed_block_object(const std::shared_ptr<full_block_type>& full_block);
  api_signed_block_object() = default;

  block_id_type                 block_id;
  public_key_type               signing_key;
  vector< transaction_id_type > transaction_ids;
};

struct api_hardfork_property_object
{
  api_hardfork_property_object( const hardfork_property_object& h );
  api_hardfork_property_object() = default;

  hardfork_property_id_type     id;
  vector< fc::time_point_sec >  processed_hardforks;
  uint32_t                      last_hardfork;
  protocol::hardfork_version    current_hardfork_version;
  protocol::hardfork_version    next_hardfork;
  fc::time_point_sec            next_hardfork_time;
};

#ifdef HIVE_ENABLE_SMT

struct api_smt_token_object
{
  api_smt_token_object( const smt_token_object& token, const database& db );

  smt_token_object                token;
  fc::optional< smt_ico_object >  ico;
};

struct api_smt_token_emissions_object
{
  api_smt_token_emissions_object( const smt_token_emissions_object& o, const database& db );

  smt_token_emissions_id_type           id;
  asset_symbol_type                     symbol;
  time_point_sec                        schedule_time;
  hive::protocol::smt_emissions_unit    emissions_unit;
  uint32_t                              interval_seconds;
  uint32_t                              interval_count;
  time_point_sec                        lep_time;
  time_point_sec                        rep_time;
  asset                                 lep_abs_amount;
  asset                                 rep_abs_amount;
  uint32_t                              lep_rel_amount_numerator;
  uint32_t                              rep_rel_amount_numerator;
  uint8_t                               rel_amount_denom_bits;
};

struct api_smt_contribution_object
{
  api_smt_contribution_object( const smt_contribution_object& o, const database& db );

  smt_contribution_id_type id;
  asset_symbol_type        symbol;
  account_name_type        contributor;
  uint32_t                 contribution_id;
  asset                    contribution;
};

#endif

enum proposal_status
{
  all,
  inactive,
  active,
  expired,
  votable
};

proposal_status get_proposal_status( const proposal_object& po, const time_point_sec current_time );

typedef uint64_t api_id_type;

struct api_proposal_object
{
  api_proposal_object() = default;
  api_proposal_object(const proposal_object& po, const time_point_sec& current_time);

  api_id_type       id;

  uint32_t          proposal_id;
  account_name_type creator;
  account_name_type receiver;
  time_point_sec    start_date;
  time_point_sec    end_date;
  asset             daily_pay;
  string            subject;
  string            permlink;
  uint64_t          total_votes = 0;
  proposal_status   status = proposal_status::all;
};

struct api_proposal_vote_object
{
  api_proposal_vote_object() = default;
  api_proposal_vote_object( const proposal_vote_object& pvo, const database& db );

  proposal_vote_id_type   id;
  account_name_type       voter;
  api_proposal_object     proposal;
};


struct api_recurrent_transfer_object
{
  api_recurrent_transfer_object() = default;
  api_recurrent_transfer_object( const recurrent_transfer_object& o, const account_name_type from_name, const account_name_type to_name );

  recurrent_transfer_id_type id;
  time_point_sec    trigger_date;
  account_name_type from;
  account_name_type to;
  asset             amount;
  string            memo;
  uint16_t          recurrence = 0;
  uint8_t           consecutive_failures = 0;
  uint16_t          remaining_executions = 0;
  uint8_t           pair_id = 0;
};


struct order
{
  price                order_price;
  double               real_price; // dollars per HIVE
  share_type           hive;
  share_type           hbd;
  fc::time_point_sec   created;
};

struct order_book
{
  vector< order >      asks;
  vector< order >      bids;
};

} } } // hive::plugins::database_api

FC_REFLECT(hive::plugins::database_api::api_commment_cashout_info,
  (total_vote_weight)
  (total_payout_value)
  (curator_payout_value)
  (max_accepted_payout)

  (author_rewards)
  (children_abs_rshares)
  (net_rshares)
  (abs_rshares)
  (vote_rshares)

  (net_votes)

  (last_payout)
  (cashout_time)
  (max_cashout_time)

  (percent_hbd)
  (reward_weight)
  (allow_replies)
  (allow_votes)
  (allow_curation_rewards)
  (was_voted_on)
)

FC_REFLECT( hive::plugins::database_api::api_reward_fund_object,
          (id)(name)(reward_balance)(recent_claims)(last_update)(content_constant)
          (percent_curation_rewards)(percent_content_rewards)(author_reward_curve)
          (curation_reward_curve)
        )

FC_REFLECT( hive::plugins::database_api::api_witness_vote_object,
          (id)(witness)(account)
        )

FC_REFLECT( hive::plugins::database_api::api_escrow_object,
          (id)(escrow_id)(from)(to)(agent)(ratification_deadline)(escrow_expiration)
          (hbd_balance)(hive_balance)(pending_fee)(to_approved)(agent_approved)(disputed)
        )

FC_REFLECT( hive::plugins::database_api::api_withdraw_vesting_route_object,
          (id)(from_account)(to_account)(percent)(auto_vest)
        )

FC_REFLECT( hive::plugins::database_api::api_vesting_delegation_object,
          (id)(delegator)(delegatee)(vesting_shares)(min_delegation_time)
        )

FC_REFLECT( hive::plugins::database_api::api_vesting_delegation_expiration_object,
          (id)(delegator)(vesting_shares)(expiration)
        )

FC_REFLECT( hive::plugins::database_api::api_convert_request_object,
          (id)(owner)(requestid)(amount)(conversion_date)
        )

FC_REFLECT( hive::plugins::database_api::api_collateralized_convert_request_object,
          (id)(owner)(requestid)(collateral_amount)(converted_amount)(conversion_date)
        )

FC_REFLECT( hive::plugins::database_api::api_decline_voting_rights_request_object,
          (id)(account)(effective_date)
        )

FC_REFLECT( hive::plugins::database_api::api_limit_order_object,
          (id)(created)(expiration)(seller)(orderid)(for_sale)(sell_price)
        )

FC_REFLECT( hive::plugins::database_api::api_dynamic_global_property_object,
          (id)(head_block_number)(head_block_id)(time)(current_witness)(total_pow)
          (num_pow_witnesses)(virtual_supply)(current_supply)(init_hbd_supply)(current_hbd_supply)
          (total_vesting_fund_hive)(total_vesting_shares)(total_reward_fund_hive)
          (total_reward_shares2)(pending_rewarded_vesting_shares)(pending_rewarded_vesting_hive)
          (hbd_interest_rate)(hbd_print_rate)(maximum_block_size)
          (current_aslot)(recent_slots_filled)(participation_count)(last_irreversible_block_num)
          (vote_power_reserve_rate)(delegation_return_period)(reverse_auction_seconds)
          (available_account_subsidies)(hbd_stop_percent)(hbd_start_percent)(next_maintenance_time)
          (last_budget_time)(next_daily_maintenance_time)(content_reward_percent)(vesting_reward_percent)(proposal_fund_percent)
          (dhf_interval_ledger)(downvote_pool_percent)(current_remove_threshold)(early_voting_seconds)(mid_voting_seconds)
          (max_consecutive_recurrent_transfer_failures)(max_recurrent_transfer_end_date)(min_recurrent_transfers_recurrence)
          (max_open_recurrent_transfers)
#ifdef HIVE_ENABLE_SMT
          (smt_creation_fee)
#endif
        )

FC_REFLECT( hive::plugins::database_api::api_change_recovery_account_request_object,
          (id)(account_to_recover)(recovery_account)(effective_on)
        )

FC_REFLECT( hive::plugins::database_api::api_account_object,
          (id)(name)(owner)(active)(posting)(memo_key)(json_metadata)(posting_json_metadata)(proxy)(previous_owner_update)(last_owner_update)(last_account_update)
          (created)(mined)
          (recovery_account)(last_account_recovery)(reset_account)
          (comment_count)(lifetime_vote_count)(post_count)(can_vote)(voting_manabar)(downvote_manabar)
          (balance)
          (savings_balance)
          (hbd_balance)(hbd_seconds)(hbd_seconds_last_update)(hbd_last_interest_payment)
          (savings_hbd_balance)(savings_hbd_seconds)(savings_hbd_seconds_last_update)(savings_hbd_last_interest_payment)(savings_withdraw_requests)
          (reward_hbd_balance)(reward_hive_balance)(reward_vesting_balance)(reward_vesting_hive)
          (vesting_shares)(delegated_vesting_shares)(received_vesting_shares)(vesting_withdraw_rate)(post_voting_power)(next_vesting_withdrawal)(withdrawn)(to_withdraw)(withdraw_routes)
          (pending_transfers)(curation_rewards)
          (posting_rewards)
          (proxied_vsf_votes)(witnesses_voted_for)
          (last_post)(last_root_post)(last_post_edit)(last_vote_time)
          (post_bandwidth)(pending_claimed_accounts)(open_recurrent_transfers)
          (is_smt)
          (delayed_votes)
          (governance_vote_expiration_ts)
        )

FC_REFLECT( hive::plugins::database_api::api_owner_authority_history_object,
          (id)
          (account)
          (previous_owner_authority)
          (last_valid_time)
        )

FC_REFLECT( hive::plugins::database_api::api_account_recovery_request_object,
          (id)
          (account_to_recover)
          (new_owner_authority)
          (expires)
        )

FC_REFLECT( hive::plugins::database_api::api_savings_withdraw_object,
          (id)
          (from)
          (to)
          (memo)
          (request_id)
          (amount)
          (complete)
        )

FC_REFLECT( hive::plugins::database_api::api_feed_history_object,
          (id)
          (current_median_history)
          (market_median_history)
          (current_min_history)
          (current_max_history)
          (price_history)
        )

FC_REFLECT( hive::plugins::database_api::api_witness_object,
          (id)
          (owner)
          (created)
          (url)(votes)(virtual_last_update)(virtual_position)(virtual_scheduled_time)(total_missed)
          (last_aslot)(last_confirmed_block_num)(pow_worker)(signing_key)
          (props)
          (hbd_exchange_rate)(last_hbd_exchange_update)
          (last_work)
          (running_version)
          (hardfork_version_vote)(hardfork_time_vote)
          (available_witness_account_subsidies)
        )

FC_REFLECT( hive::plugins::database_api::future_chain_properties,
          (account_creation_fee)
          (maximum_block_size)
          (hbd_interest_rate)
          (account_subsidy_budget)
          (account_subsidy_decay)
        )

FC_REFLECT( hive::plugins::database_api::future_witness_schedule,
          (num_scheduled_witnesses)
          (elected_weight)
          (timeshare_weight)
          (miner_weight)
          (witness_pay_normalization_factor)
          (median_props)
          (majority_version)
          (max_voted_witnesses)
          (max_miner_witnesses)
          (max_runner_witnesses)
          (hardfork_required_witnesses)
          (account_subsidy_rd)
          (account_subsidy_witness_rd)
          (min_witness_account_subsidy_decay)
        )

FC_REFLECT( hive::plugins::database_api::api_witness_schedule_object,
          (id)
          (current_virtual_time)
          (next_shuffle_block_num)
          (current_shuffled_witnesses)
          (future_shuffled_witnesses)
          (num_scheduled_witnesses)
          (elected_weight)
          (timeshare_weight)
          (miner_weight)
          (witness_pay_normalization_factor)
          (median_props)
          (majority_version)
          (max_voted_witnesses)
          (max_miner_witnesses)
          (max_runner_witnesses)
          (hardfork_required_witnesses)
          (account_subsidy_rd)
          (account_subsidy_witness_rd)
          (min_witness_account_subsidy_decay)
          (future_changes)
        )

FC_REFLECT_DERIVED( hive::plugins::database_api::api_signed_block_object, (hive::protocol::signed_block),
              (block_id)
              (signing_key)
              (transaction_ids)
            )

FC_REFLECT( hive::plugins::database_api::api_hardfork_property_object,
        (id)
        (processed_hardforks)
        (last_hardfork)
        (current_hardfork_version)
        (next_hardfork)
        (next_hardfork_time)
        )

#ifdef HIVE_ENABLE_SMT

FC_REFLECT( hive::plugins::database_api::api_smt_token_object,
  (token)
  (ico)
)

FC_REFLECT( hive::plugins::database_api::api_smt_token_emissions_object,
          (id)(symbol)(schedule_time)(emissions_unit)(interval_seconds)(interval_count)
          (lep_time)(rep_time)(lep_abs_amount)(rep_abs_amount)(lep_rel_amount_numerator)
          (rep_rel_amount_numerator)(rel_amount_denom_bits)
        )

FC_REFLECT( hive::plugins::database_api::api_smt_contribution_object,
          (id)(symbol)(contributor)(contribution_id)(contribution)
        )

#endif

FC_REFLECT_ENUM( hive::plugins::database_api::proposal_status,
            (all)
            (inactive)
            (active)
            (expired)
            (votable)
          )

FC_REFLECT( hive::plugins::database_api::api_proposal_object,
        (id)
        (proposal_id)
        (creator)
        (receiver)
        (start_date)
        (end_date)
        (daily_pay)
        (subject)
        (permlink)
        (total_votes)
        (status)
        )

FC_REFLECT( hive::plugins::database_api::api_proposal_vote_object,
        (id)
        (voter)
        (proposal)
        )

FC_REFLECT( hive::plugins::database_api::order, (order_price)(real_price)(hive)(hbd)(created) );

FC_REFLECT( hive::plugins::database_api::order_book, (asks)(bids) );
FC_REFLECT(hive::plugins::database_api::api_recurrent_transfer_object, (id)(trigger_date)(from)(to)(amount)(memo)(recurrence)(consecutive_failures)(remaining_executions)(pair_id))
