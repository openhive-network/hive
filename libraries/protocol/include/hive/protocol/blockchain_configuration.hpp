#pragma once

// std
#include <utility>
#include <cstdint>

// fc
#include <fc/crypto/elliptic.hpp>

// project includes
#include <hive/protocol/asset_symbol.hpp>
#include <hive/protocol/hardfork.hpp>
#include <hive/protocol/fixed_string.hpp>

namespace hive
{
  namespace protocol
  {
    struct asset;
    using fc::uint128_t;

    namespace blockchain_configuration
    {

      // has to be const, because they are used as template parameters, or dependencies of them

      constexpr uint32_t hive_100_percent{10'000u};
      constexpr uint32_t hive_1_percent{hive_100_percent / 100};
      constexpr uint32_t hive_1_basis_point{hive_100_percent / 10'000};
      constexpr int hive_block_interval{3};
      constexpr int hive_blocks_per_year{365 * 24 * 60 * 60 / hive_block_interval};
      constexpr uint32_t hive_blocks_per_day{24u * 60 * 60 / hive_block_interval};
      constexpr int hive_max_witnesses{21};
      constexpr int hive_max_voted_witnesses_hf0{19};
      constexpr int hive_max_miner_witnesses_hf0{1};
      constexpr int hive_max_runner_witnesses_hf0{1};
      constexpr int hive_max_voted_witnesses_hf17{20};
      constexpr int hive_max_miner_witnesses_hf17{0};
      constexpr int hive_max_runner_witnesses_hf17{1};
      constexpr int hive_max_proxy_recursion_depth{4};
      constexpr int hive_voting_mana_regeneration_seconds{5 * 60 * 60 * 24};
      constexpr int hive_liquidity_reward_period_sec{60 * 60};
      constexpr uint64_t hive_apr_percent_multiply_per_block{(uint64_t(0x5ccc) << 0x20) | (uint64_t(0xe802) << 0x10) | (uint64_t(0xde5f))};
      constexpr int hive_apr_percent_shift_per_block{87};
      constexpr uint64_t hive_apr_percent_multiply_per_round{(uint64_t(0x79cc) << 0x20) | (uint64_t(0xf5c7) << 0x10) | (uint64_t(0x3480))};
      constexpr int hive_apr_percent_shift_per_round{83};
      constexpr uint64_t hive_apr_percent_multiply_per_hour{(uint64_t(0x6cc1) << 0x20) | (uint64_t(0x39a1) << 0x10) | (uint64_t(0x5cbd))};
      constexpr int hive_apr_percent_shift_per_hour{77};
      constexpr int hive_curate_apr_percent{3875};
      constexpr int hive_content_apr_percent{3875};
      constexpr int hive_liquidity_apr_percent{750};
      constexpr int hive_producer_apr_percent{750};
      constexpr int hive_pow_apr_percent{750};
      constexpr uint32_t hive_min_account_name_length{3u};
      constexpr int hive_rd_min_decay_bits{6};
      constexpr uint32_t hive_irreversible_threshold{75u * hive_1_percent};
      constexpr uint32_t hive_rd_decay_denom_shift{36u};
      constexpr int hive_rd_max_pool_bits{64};
      constexpr uint64_t hive_rd_max_budget_1{(uint64_t(1) << (hive_rd_max_pool_bits + hive_rd_min_decay_bits - hive_rd_decay_denom_shift)) - 1};
      constexpr uint64_t hive_rd_max_budget_2{(uint64_t(1) << (64 - hive_rd_decay_denom_shift)) - 1};
      constexpr uint64_t hive_rd_max_budget_3{uint64_t(std::numeric_limits<int32_t>::max())};
      constexpr int hive_rd_max_budget{int32_t(std::min({hive_rd_max_budget_1, hive_rd_max_budget_2, hive_rd_max_budget_3}))};

      /**
       * @brief This class contains blockchain configration with default confiduration 
      */
      struct blockchain_configuration_t
      {
        template <typename DataType>
        class blockchain_configuration_member_t
        {
          std::shared_ptr<DataType> m_data;

          bool valid() const { return m_data.get(); }

        public:
          using value_type = DataType;

          blockchain_configuration_member_t() {}

          template <typename... U>
          explicit blockchain_configuration_member_t(U &&...u) : m_data{ std::make_shared<DataType>( std::forward<U>(u)...) } {}

          const value_type &get() const { FC_ASSERT( valid() ); return *m_data; }
          void set(const value_type &val) { FC_ASSERT(valid()); *this->m_data = val; }

          blockchain_configuration_member_t &operator=(const value_type &v)
          {
            set(v);
            return *this;
          }
        };

        template <typename Any>
        using member_wrapper_t = blockchain_configuration_member_t<Any>;

        using member_str_t = member_wrapper_t<fc::string>;
        using member_hash_t = member_wrapper_t<fc::sha256>;
        using member_pv_key_t = member_wrapper_t<fc::ecc::private_key>;
        using member_version_t = member_wrapper_t<version>;
        using member_hardfork_version_t = member_wrapper_t<hardfork_version>;
        using member_int_t = member_wrapper_t<int32_t>;
        using member_uint_t = member_wrapper_t<uint32_t>;
        using member_bigint_t = member_wrapper_t<int64_t>;
        using member_ubigint_t = member_wrapper_t<uint64_t>;
        using member_hugeint_t = member_wrapper_t<fc::uint128_t>;
        using member_asset_t = member_wrapper_t<asset>;
        using member_asset_symbol_t = member_wrapper_t<asset_symbol_type>;
        using member_time_t = member_wrapper_t<fc::microseconds>;
        using member_timepoint_t = member_wrapper_t<fc::time_point_sec>;
        using member_accountname_t = member_wrapper_t<fixed_string<16>>;

        /**
         * @brief this allows to chain configuration properties
         * @example usage of blockchain_configuration_t::operator() 
         * ```
         * using bconf = blockchain_configuration_t;
         * const auto new_conf = get_current_blockchain_config()
         *  (&bconf::hive_upvote_lockout_seconds, 12u)
         *  (&bconf::hive_max_vote_changes, 200)
         *  (&bconf::hive_chain_id, fc::sha256::hash("my super hash"))
         *  (&bconf::hive_upvote_lockout_hf17, fc::minutes(66));
         * ```
        */
        template <typename memeber_t, typename Any = typename memeber_t::value_type>
        blockchain_configuration_t operator()(memeber_t blockchain_configuration_t::*ptr_to_member, const Any &value) const
        {
          blockchain_configuration_t result(*this);
          result.*ptr_to_member = value;
          return result;
        }

        member_int_t hive_one_day_seconds{60 * 60 * 24};
#ifdef IS_TEST_NET
        member_version_t hive_blockchain_version{version(
#ifdef HIVE_ENABLE_SMT
            1, 26, 0
#else
            1, 25, 0
#endif
        )};
        member_pv_key_t hive_init_private_key{fc::ecc::private_key::regenerate(fc::sha256::hash(std::string("init_key")))};
        member_hash_t steem_chain_id{fc::sha256::hash("testnet")};
        member_hash_t hive_chain_id{fc::sha256::hash("testnet")};
        member_timepoint_t hive_genesis_time{fc::time_point_sec(1451606400)};
        member_timepoint_t hive_mining_time{fc::time_point_sec(1451606400)};
        member_int_t hive_cashout_window_seconds{60 * 60};
        member_int_t hive_cashout_window_seconds_pre_hf12;
        member_int_t hive_cashout_window_seconds_pre_hf17;
        member_int_t hive_second_cashout_window{60 * 60};
        member_int_t hive_max_cashout_window_seconds{60 * 60 * 24};
        member_time_t hive_upvote_lockout_hf7{fc::minutes(1)};
        member_uint_t hive_upvote_lockout_seconds{60u * 5};
        member_time_t hive_upvote_lockout_hf17{fc::minutes(5)};
        member_int_t hive_min_account_creation_fee{0};
        member_bigint_t hive_max_account_creation_fee{int64_t(1000000000)};
        member_time_t hive_owner_auth_recovery_period{fc::seconds(60)};
        member_time_t hive_account_recovery_request_expiration_period{fc::seconds(12)};
        member_time_t hive_owner_update_limit{fc::seconds(0)};
        member_uint_t hive_owner_auth_history_tracking_start_block_num{1u};
        member_bigint_t hive_init_supply{int64_t(250) * int64_t(1000000) * int64_t(1000)};
        member_bigint_t hive_hbd_init_supply{int64_t(7) * int64_t(1000000) * int64_t(1000)};
        member_uint_t testnet_block_limit{3000000u};
        member_int_t hive_proposal_maintenance_period{3600};
        member_int_t hive_proposal_maintenance_cleanup{60 * 60 * 24 * 1};
        member_int_t hive_daily_proposal_maintenance_period{60 * 60};
        member_time_t hive_governance_vote_expiration_period{fc::days(20)};
        member_int_t hive_global_remove_threshold{20};
#else
        member_version_t hive_blockchain_version{version(1, 25, 8)};
        member_hash_t steem_chain_id{fc::sha256()};
        member_hash_t hive_chain_id{fc::sha256::hash("beeab0de00000000000000000000000000000000000000000000000000000000")};
        member_timepoint_t hive_genesis_time{fc::time_point_sec(1458835200)};
        member_timepoint_t hive_mining_time{fc::time_point_sec(1458838800)};
        member_int_t hive_cashout_window_seconds_pre_hf12{60 * 60 * 24};
        member_int_t hive_cashout_window_seconds_pre_hf17{60 * 60 * 12};
        member_int_t hive_cashout_window_seconds{60 * 60 * 24 * 7};
        member_int_t hive_second_cashout_window{60 * 60 * 24 * 30};
        member_int_t hive_max_cashout_window_seconds{60 * 60 * 24 * 14};
        member_time_t hive_upvote_lockout_hf7{fc::minutes(1)};
        member_uint_t hive_upvote_lockout_seconds{60u * 60 * 12};
        member_time_t hive_upvote_lockout_hf17{fc::hours(12)};
        member_int_t hive_min_account_creation_fee{1};
        member_bigint_t hive_max_account_creation_fee{int64_t(1000000000)};
        member_time_t hive_owner_auth_recovery_period{fc::days(30)};
        member_time_t hive_account_recovery_request_expiration_period{fc::days(1)};
        member_time_t hive_owner_update_limit{fc::minutes(60)};
        member_uint_t hive_owner_auth_history_tracking_start_block_num{3186477u};
        member_bigint_t hive_init_supply{int64_t(0)};
        member_bigint_t hive_hbd_init_supply{int64_t(0)};
        member_int_t hive_proposal_maintenance_period{3600};
        member_int_t hive_proposal_maintenance_cleanup{60 * 60 * 24 * 1};
        member_int_t hive_daily_proposal_maintenance_period;
        member_time_t hive_governance_vote_expiration_period{fc::days(365)};
        member_int_t hive_global_remove_threshold{200};
#endif
        member_asset_symbol_t vests_symbol{hive::protocol::asset_symbol_type::from_asset_num(HIVE_ASSET_NUM_VESTS)};
        member_asset_symbol_t hive_symbol{hive::protocol::asset_symbol_type::from_asset_num(HIVE_ASSET_NUM_HIVE)};
        member_asset_symbol_t hbd_symbol{hive::protocol::asset_symbol_type::from_asset_num(HIVE_ASSET_NUM_HBD)};
        member_hardfork_version_t hive_blockchain_hardfork_version;
        member_uint_t hive_start_vesting_block{hive_blocks_per_day * 7u};
        member_uint_t hive_start_miner_voting_block{hive_blocks_per_day * 30u};
        member_int_t hive_num_init_miners{1};
        member_timepoint_t hive_init_time{fc::time_point_sec()};
        member_int_t hive_hardfork_required_witnesses{17};
        member_uint_t hive_max_time_until_expiration{60u * 60};
        member_uint_t hive_max_memo_size{2048u};
        member_int_t hive_vesting_withdraw_intervals_pre_hf_16{104};
        member_int_t hive_vesting_withdraw_intervals{13};
        member_int_t hive_vesting_withdraw_interval_seconds{60 * 60 * 24 * 7};
        member_int_t hive_max_withdraw_routes{10};
        member_int_t hive_max_pending_transfers{255};
        member_time_t hive_savings_withdraw_time{fc::days(3)};
        member_int_t hive_savings_withdraw_request_limit{100};
        member_int_t hive_max_vote_changes{5};
        member_int_t hive_reverse_auction_window_seconds_hf6{60 * 30};
        member_int_t hive_reverse_auction_window_seconds_hf20{60 * 15};
        member_int_t hive_reverse_auction_window_seconds_hf21{60 * 5};
        member_int_t hive_early_voting_seconds_hf25{24 * 60 * 60};
        member_int_t hive_mid_voting_seconds_hf25{48 * 60 * 60};
        member_int_t hive_min_vote_interval_sec{3};
        member_int_t hive_vote_dust_threshold{50000000};
        member_uint_t hive_downvote_pool_percent_hf21{25 * hive_1_percent};
        member_int_t hive_delayed_voting_total_interval_seconds{60 * 60 * 24 * 30};
        member_int_t hive_delayed_voting_interval_seconds{60 * 60 * 24 * 1};
        member_time_t hive_min_root_comment_interval{fc::seconds(60 * 5)};
        member_time_t hive_min_reply_interval{fc::seconds(20)};
        member_time_t hive_min_reply_interval_hf20{fc::seconds(3)};
        member_time_t hive_min_comment_edit_interval{fc::seconds(3)};
        member_uint_t hive_post_average_window{60 * 60 * 24u};
        member_ubigint_t hive_post_weight_constant{uint64_t(4 * hive_100_percent) * (4 * hive_100_percent)};
        member_int_t hive_max_account_witness_votes{30};
        member_uint_t hive_default_hbd_interest_rate{10u * hive_1_percent};
        member_int_t hive_inflation_rate_start_percent{978};
        member_int_t hive_inflation_rate_stop_percent{95};
        member_int_t hive_inflation_narrowing_period{250000};
        member_uint_t hive_content_reward_percent_hf16{75u * hive_1_percent};
        member_uint_t hive_vesting_fund_percent_hf16{15u * hive_1_percent};
        member_int_t hive_proposal_fund_percent_hf0{0};
        member_uint_t hive_content_reward_percent_hf21{65u * hive_1_percent};
        member_uint_t hive_proposal_fund_percent_hf21{10 * hive_1_percent};
        member_hugeint_t hive_hf21_convergent_linear_recent_claims{fc::uint128_t(0, 503600561838938636ull)};
        member_hugeint_t hive_content_constant_hf21{fc::uint128_t(0, 2000000000000ull)};
        member_uint_t hive_miner_pay_percent{hive_1_percent};
        member_int_t hive_max_ration_decay_rate{1000000};
        member_int_t hive_bandwidth_average_window_seconds{60 * 60 * 24 * 7};
        member_ubigint_t hive_bandwidth_precision{uint64_t(1000000)};
        member_int_t hive_max_comment_depth_pre_hf17{6};
        member_int_t hive_max_comment_depth{0xffff};
        member_int_t hive_soft_max_comment_depth{0xff};
        member_int_t hive_max_reserve_ratio{20000};
        member_int_t hive_create_account_with_hive_modifier{30};
        member_int_t hive_create_account_delegation_ratio{5};
        member_time_t hive_create_account_delegation_time{fc::days(30)};
        member_asset_t hive_mining_reward;
        member_uint_t hive_equihash_n{140u};
        member_uint_t hive_equihash_k{6u};
        member_time_t hive_liquidity_timeout_sec{fc::seconds(60 * 60 * 24 * 7)};
        member_time_t hive_min_liquidity_reward_period_sec{fc::seconds(60)};
        member_int_t hive_liquidity_reward_blocks{hive_liquidity_reward_period_sec / hive_block_interval};
        member_asset_t hive_min_liquidity_reward;
        member_asset_t hive_min_content_reward;
        member_asset_t hive_min_curate_reward;
        member_asset_t hive_min_producer_reward;
        member_asset_t hive_min_pow_reward;
        member_asset_t hive_active_challenge_fee;
        member_asset_t hive_owner_challenge_fee;
        member_time_t hive_active_challenge_cooldown{fc::days(1)};
        member_time_t hive_owner_challenge_cooldown{fc::days(1)};
        member_time_t hive_recent_rshares_decay_time_hf17{fc::days(30)};
        member_time_t hive_recent_rshares_decay_time_hf19{fc::days(15)};
        member_hugeint_t hive_content_constant_hf0{uint128_t(uint64_t(2000000000000ll))};
        member_asset_t hive_min_payout_hbd;
        member_uint_t hive_hbd_stop_percent_hf14{5u * hive_1_percent};
        member_uint_t hive_hbd_stop_percent_hf20{10u * hive_1_percent};
        member_uint_t hive_hbd_start_percent_hf14{2u * hive_1_percent};
        member_uint_t hive_hbd_start_percent_hf20{9u * hive_1_percent};
        member_uint_t hive_max_account_name_length{16u};
        member_uint_t hive_min_permlink_length{0u};
        member_uint_t hive_max_permlink_length{256u};
        member_uint_t hive_max_witness_url_length{2048u};
        member_bigint_t hive_max_share_supply{int64_t(1000000000000000ll)};
        member_bigint_t hive_max_satoshis{int64_t(4611686018427387903ll)};
        member_int_t hive_max_sig_check_depth{2};
        member_int_t hive_max_sig_check_accounts{125};
        member_int_t hive_min_transaction_size_limit{1024};
        member_ubigint_t hive_seconds_per_year{uint64_t(60 * 60 * 24 * 365ll)};
        member_int_t hive_hbd_interest_compound_interval_sec{60 * 60 * 24 * 30};
        member_int_t hive_max_transaction_size{1024 * 64};
        member_uint_t hive_min_block_size_limit;
        member_uint_t hive_max_block_size;
        member_uint_t hive_soft_max_block_size{2u * 1024 * 1024};
        member_uint_t hive_min_block_size{115u};
        member_int_t hive_blocks_per_hour{60 * 60 / hive_block_interval};
        member_int_t hive_feed_interval_blocks;
        member_int_t hive_feed_history_window_pre_hf_16{24 * 7};
        member_uint_t hive_feed_history_window{12u * 7};
        member_int_t hive_max_feed_age_seconds{60 * 60 * 24 * 7};
        member_uint_t hive_min_feeds{hive_max_witnesses / 3u};
        member_time_t hive_conversion_delay_pre_hf_16{fc::days(7)};
        member_time_t hive_conversion_delay;
        member_int_t hive_min_undo_history{10};
        member_uint_t hive_max_undo_history{10000u};
        member_int_t hive_min_transaction_expiration_limit{hive_block_interval * 5};
        member_ubigint_t hive_blockchain_precision{uint64_t(1000)};
        member_int_t hive_blockchain_precision_digits{3};
        member_ubigint_t hive_max_instance_id{uint64_t(-1) >> 16};
        member_uint_t hive_max_authority_membership{40u};
        member_int_t hive_max_asset_whitelist_authorities{10};
        member_int_t hive_max_url_length{127};
        member_hugeint_t hive_virtual_schedule_lap_length{fc::uint128(uint64_t(-1))};
        member_hugeint_t hive_virtual_schedule_lap_length2{fc::uint128::max_value()};
        member_int_t hive_initial_vote_power_rate{40};
        member_int_t hive_reduced_vote_power_rate{10};
        member_int_t hive_max_limit_order_expiration{60 * 60 * 24 * 28};
        member_int_t hive_delegation_return_period_hf0;
        member_int_t hive_delegation_return_period_hf20{hive_voting_mana_regeneration_seconds};
        member_int_t hive_rd_max_decay_bits{32};
        member_uint_t hive_rd_min_decay{uint32_t(1) << hive_rd_min_decay_bits};
        member_int_t hive_rd_min_budget{1};
        member_uint_t hive_rd_max_decay{uint32_t(0xFFFFFFFF)};
        member_uint_t hive_account_subsidy_precision{hive_100_percent};
        member_uint_t hive_witness_subsidy_budget_percent{125u * hive_1_percent};
        member_uint_t hive_witness_subsidy_decay_percent{hive_max_witnesses * hive_100_percent};
        member_int_t hive_default_account_subsidy_decay{347321};
        member_int_t hive_default_account_subsidy_budget{797};
        member_uint_t hive_decay_backstop_percent{90u * hive_1_percent};
        member_uint_t hive_block_generation_postponed_tx_limit{5u};
        member_time_t hive_pending_transaction_execution_limit{fc::milliseconds(200)};
        member_uint_t hive_custom_op_id_max_length{32u};
        member_uint_t hive_custom_op_data_max_length{8192u};
        member_uint_t hive_beneficiary_limit{128u};
        member_uint_t hive_comment_title_limit{256u};
        member_accountname_t hive_root_post_parent{ member_accountname_t::value_type{} };
        member_ubigint_t hive_treasury_fee;
        member_uint_t hive_proposal_subject_max_length{80u};
        member_uint_t hive_proposal_max_ids_number{5u};
        member_int_t hive_proposal_fee_increase_days{60};
        member_uint_t hive_proposal_fee_increase_days_sec;
        member_ubigint_t hive_proposal_fee_increase_amount;
        member_uint_t hive_proposal_conversion_rate{5 * hive_1_basis_point};
        member_int_t days_for_delayed_voting;

        // member_str_t hive_init_miner_name{"initminer"};
        // member_str_t hive_miner_account{"miners"};
        // member_str_t hive_null_account{"null"};
        // member_str_t hive_temp_account{"temp"};
        // member_str_t hive_proxy_to_self_account{""};
        // member_str_t hive_post_reward_fund_name{"post"};
        // member_str_t hive_comment_reward_fund_name{"comment"};
        // member_str_t obsolete_treasury_account{"steem.dao"};
        // member_str_t new_hive_treasury_account{"hive.fund"};

#ifdef HIVE_ENABLE_SMT
        member_int_t smt_max_votable_assets{2};
        member_int_t smt_vesting_withdraw_interval_seconds{60 * 60 * 24 * 7};
        member_int_t smt_upvote_lockout{60 * 60 * 12};
        member_int_t smt_emission_min_interval_seconds{60 * 60 * 6};
        member_uint_t smt_emit_indefinitely{std::numeric_limits<uint32_t>::max()};
        member_int_t smt_max_nominal_votes_per_day{1000};
        member_uint_t smt_max_votes_per_regeneration;
        member_int_t smt_default_votes_per_regen_period{50};
        member_uint_t smt_default_percent_curation_rewards{25u * hive_1_percent};
#endif

        /** @brief default constructor, some of members depends on others, and theese, cannot be initialized in standard way */
        blockchain_configuration_t();
      };

      extern
#ifndef IS_TEST_NET
      const
#endif
      blockchain_configuration_t g_blockchain_configuration;
    }
  }

  const protocol::blockchain_configuration::blockchain_configuration_t &get_blockchain_configuration();

#ifdef IS_TEST_NET
  void set_blockchain_configuration(const protocol::blockchain_configuration::blockchain_configuration_t &);
  void restore_blockchain_configuration();
#endif
} // hive::protocol::blockchain_configuration
