/*
* Copyright (c) 2016 Steemit, Inc., and contributors.
*/
#pragma once
#include <hive/protocol/blockchain_configuration.hpp>

// WARNING!
// Every symbol defined here needs to be handled appropriately in get_config.cpp
// This is checked by get_config_check.sh called from Dockerfile

#ifdef	IS_TEST_NET

#define HIVE_INIT_PRIVATE_KEY		hive::get_blockchain_configuration().hive_init_private_key.get()
/// Allows to limit number of total produced blocks.
#define TESTNET_BLOCK_LIMIT		hive::get_blockchain_configuration().testnet_block_limit.get()
#define HIVE_ADDRESS_PREFIX		"TST"
#define HIVE_INIT_PUBLIC_KEY_STR		std::string( hive::protocol::public_key_type(hive::get_blockchain_configuration().hive_init_private_key.get().get_public_key()) )
#else
#define HIVE_ADDRESS_PREFIX		"STM"
#define HIVE_INIT_PUBLIC_KEY_STR		"STM8GC13uCZbP44HzMLV6zPZGwVQ8Nt4Kji8PapsPiNq1BK153XTX"
#endif

////////// constexpresions
// theese have to be known during compile time, because they are used as template parameters, or dependencies of them

#define HIVE_100_PERCENT hive::protocol::blockchain_configuration::hive_100_percent
#define HIVE_1_PERCENT hive::protocol::blockchain_configuration::hive_1_percent
#define HIVE_1_BASIS_POINT hive::protocol::blockchain_configuration::hive_1_basis_point
#define HIVE_BLOCK_INTERVAL hive::protocol::blockchain_configuration::hive_block_interval
#define HIVE_BLOCKS_PER_YEAR hive::protocol::blockchain_configuration::hive_blocks_per_year
#define HIVE_BLOCKS_PER_DAY hive::protocol::blockchain_configuration::hive_blocks_per_day
#define HIVE_MAX_WITNESSES hive::protocol::blockchain_configuration::hive_max_witnesses
#define HIVE_MAX_VOTED_WITNESSES_HF0 hive::protocol::blockchain_configuration::hive_max_voted_witnesses_hf0
#define HIVE_MAX_MINER_WITNESSES_HF0 hive::protocol::blockchain_configuration::hive_max_miner_witnesses_hf0
#define HIVE_MAX_RUNNER_WITNESSES_HF0 hive::protocol::blockchain_configuration::hive_max_runner_witnesses_hf0
#define HIVE_MAX_VOTED_WITNESSES_HF17 hive::protocol::blockchain_configuration::hive_max_voted_witnesses_hf17
#define HIVE_MAX_MINER_WITNESSES_HF17 hive::protocol::blockchain_configuration::hive_max_miner_witnesses_hf17
#define HIVE_MAX_RUNNER_WITNESSES_HF17 hive::protocol::blockchain_configuration::hive_max_runner_witnesses_hf17
#define HIVE_MAX_PROXY_RECURSION_DEPTH hive::protocol::blockchain_configuration::hive_max_proxy_recursion_depth
#define HIVE_VOTING_MANA_REGENERATION_SECONDS hive::protocol::blockchain_configuration::hive_voting_mana_regeneration_seconds
#define HIVE_LIQUIDITY_REWARD_PERIOD_SEC hive::protocol::blockchain_configuration::hive_liquidity_reward_period_sec
#define HIVE_APR_PERCENT_MULTIPLY_PER_BLOCK hive::protocol::blockchain_configuration::hive_apr_percent_multiply_per_block
#define HIVE_APR_PERCENT_SHIFT_PER_BLOCK hive::protocol::blockchain_configuration::hive_apr_percent_shift_per_block
#define HIVE_APR_PERCENT_MULTIPLY_PER_ROUND hive::protocol::blockchain_configuration::hive_apr_percent_multiply_per_round
#define HIVE_APR_PERCENT_SHIFT_PER_ROUND hive::protocol::blockchain_configuration::hive_apr_percent_shift_per_round
#define HIVE_APR_PERCENT_MULTIPLY_PER_HOUR hive::protocol::blockchain_configuration::hive_apr_percent_multiply_per_hour
#define HIVE_APR_PERCENT_SHIFT_PER_HOUR hive::protocol::blockchain_configuration::hive_apr_percent_shift_per_hour
#define HIVE_CURATE_APR_PERCENT hive::protocol::blockchain_configuration::hive_curate_apr_percent
#define HIVE_CONTENT_APR_PERCENT hive::protocol::blockchain_configuration::hive_content_apr_percent
#define HIVE_LIQUIDITY_APR_PERCENT hive::protocol::blockchain_configuration::hive_liquidity_apr_percent
#define HIVE_PRODUCER_APR_PERCENT hive::protocol::blockchain_configuration::hive_producer_apr_percent
#define HIVE_POW_APR_PERCENT hive::protocol::blockchain_configuration::hive_pow_apr_percent
#define HIVE_MIN_ACCOUNT_NAME_LENGTH hive::protocol::blockchain_configuration::hive_min_account_name_length
#define HIVE_RD_MIN_DECAY_BITS hive::protocol::blockchain_configuration::hive_rd_min_decay_bits
#define HIVE_IRREVERSIBLE_THRESHOLD hive::protocol::blockchain_configuration::hive_irreversible_threshold
#define HIVE_RD_DECAY_DENOM_SHIFT hive::protocol::blockchain_configuration::hive_rd_decay_denom_shift
#define HIVE_RD_MAX_POOL_BITS hive::protocol::blockchain_configuration::hive_rd_max_pool_bits
#define HIVE_RD_MAX_BUDGET_1 hive::protocol::blockchain_configuration::hive_rd_max_budget_1
#define HIVE_RD_MAX_BUDGET_2 hive::protocol::blockchain_configuration::hive_rd_max_budget_2
#define HIVE_RD_MAX_BUDGET_3 hive::protocol::blockchain_configuration::hive_rd_max_budget_3
#define HIVE_RD_MAX_BUDGET hive::protocol::blockchain_configuration::hive_rd_max_budget

////////// strings

#define HIVE_INIT_MINER_NAME "initminer"
#define HIVE_MINER_ACCOUNT "miners"
#define HIVE_NULL_ACCOUNT "null"
#define HIVE_TEMP_ACCOUNT "temp"
#define HIVE_PROXY_TO_SELF_ACCOUNT ""
#define HIVE_POST_REWARD_FUND_NAME "post"
#define HIVE_COMMENT_REWARD_FUND_NAME "comment"
#define OBSOLETE_TREASURY_ACCOUNT "steem.dao"
#define NEW_HIVE_TREASURY_ACCOUNT "hive.fund"

////////// GLOBAL OBJECT g_blockchain_configuration

#define HIVE_BLOCKCHAIN_VERSION		hive::get_blockchain_configuration().hive_blockchain_version.get()
#define STEEM_CHAIN_ID		hive::get_blockchain_configuration().steem_chain_id.get()
#define HIVE_CHAIN_ID		hive::get_blockchain_configuration().hive_chain_id.get()
#define HIVE_GENESIS_TIME		hive::get_blockchain_configuration().hive_genesis_time.get()
#define HIVE_MINING_TIME		hive::get_blockchain_configuration().hive_mining_time.get()
#define HIVE_CASHOUT_WINDOW_SECONDS_PRE_HF12		hive::get_blockchain_configuration().hive_cashout_window_seconds_pre_hf12.get()
#define HIVE_CASHOUT_WINDOW_SECONDS_PRE_HF17		hive::get_blockchain_configuration().hive_cashout_window_seconds_pre_hf17.get()
#define HIVE_CASHOUT_WINDOW_SECONDS		hive::get_blockchain_configuration().hive_cashout_window_seconds.get()
#define HIVE_SECOND_CASHOUT_WINDOW		hive::get_blockchain_configuration().hive_second_cashout_window.get()
#define HIVE_MAX_CASHOUT_WINDOW_SECONDS		hive::get_blockchain_configuration().hive_max_cashout_window_seconds.get()
#define HIVE_UPVOTE_LOCKOUT_HF7		hive::get_blockchain_configuration().hive_upvote_lockout_hf7.get()
#define HIVE_UPVOTE_LOCKOUT_SECONDS		hive::get_blockchain_configuration().hive_upvote_lockout_seconds.get()
#define HIVE_UPVOTE_LOCKOUT_HF17		hive::get_blockchain_configuration().hive_upvote_lockout_hf17.get()
#define HIVE_MIN_ACCOUNT_CREATION_FEE		hive::get_blockchain_configuration().hive_min_account_creation_fee.get()
#define HIVE_MAX_ACCOUNT_CREATION_FEE		hive::get_blockchain_configuration().hive_max_account_creation_fee.get()
#define HIVE_OWNER_AUTH_RECOVERY_PERIOD		hive::get_blockchain_configuration().hive_owner_auth_recovery_period.get()
#define HIVE_ACCOUNT_RECOVERY_REQUEST_EXPIRATION_PERIOD		hive::get_blockchain_configuration().hive_account_recovery_request_expiration_period.get()
#define HIVE_OWNER_UPDATE_LIMIT		hive::get_blockchain_configuration().hive_owner_update_limit.get()
#define HIVE_OWNER_AUTH_HISTORY_TRACKING_START_BLOCK_NUM		hive::get_blockchain_configuration().hive_owner_auth_history_tracking_start_block_num.get()
#define HIVE_INIT_SUPPLY		hive::get_blockchain_configuration().hive_init_supply.get()
#define HIVE_HBD_INIT_SUPPLY		hive::get_blockchain_configuration().hive_hbd_init_supply.get()
#define HIVE_PROPOSAL_MAINTENANCE_PERIOD		hive::get_blockchain_configuration().hive_proposal_maintenance_period.get()
#define HIVE_PROPOSAL_MAINTENANCE_CLEANUP		hive::get_blockchain_configuration().hive_proposal_maintenance_cleanup.get()
#define HIVE_DAILY_PROPOSAL_MAINTENANCE_PERIOD		hive::get_blockchain_configuration().hive_daily_proposal_maintenance_period.get()
#define HIVE_GOVERNANCE_VOTE_EXPIRATION_PERIOD		hive::get_blockchain_configuration().hive_governance_vote_expiration_period.get()
#define HIVE_GLOBAL_REMOVE_THRESHOLD		hive::get_blockchain_configuration().hive_global_remove_threshold.get()
#define VESTS_SYMBOL		hive::get_blockchain_configuration().vests_symbol.get()
#define HIVE_SYMBOL		hive::get_blockchain_configuration().hive_symbol.get()
#define HBD_SYMBOL		hive::get_blockchain_configuration().hbd_symbol.get()
#define HIVE_BLOCKCHAIN_HARDFORK_VERSION		hive::get_blockchain_configuration().hive_blockchain_hardfork_version.get()
#define HIVE_START_VESTING_BLOCK		hive::get_blockchain_configuration().hive_start_vesting_block.get()
#define HIVE_START_MINER_VOTING_BLOCK		hive::get_blockchain_configuration().hive_start_miner_voting_block.get()
#define HIVE_NUM_INIT_MINERS		hive::get_blockchain_configuration().hive_num_init_miners.get()
#define HIVE_INIT_TIME		hive::get_blockchain_configuration().hive_init_time.get()
#define HIVE_HARDFORK_REQUIRED_WITNESSES		hive::get_blockchain_configuration().hive_hardfork_required_witnesses.get()
#define HIVE_MAX_TIME_UNTIL_EXPIRATION		hive::get_blockchain_configuration().hive_max_time_until_expiration.get()
#define HIVE_MAX_MEMO_SIZE		hive::get_blockchain_configuration().hive_max_memo_size.get()
#define HIVE_VESTING_WITHDRAW_INTERVALS_PRE_HF_16		hive::get_blockchain_configuration().hive_vesting_withdraw_intervals_pre_hf_16.get()
#define HIVE_VESTING_WITHDRAW_INTERVALS		hive::get_blockchain_configuration().hive_vesting_withdraw_intervals.get()
#define HIVE_VESTING_WITHDRAW_INTERVAL_SECONDS		hive::get_blockchain_configuration().hive_vesting_withdraw_interval_seconds.get()
#define HIVE_MAX_WITHDRAW_ROUTES		hive::get_blockchain_configuration().hive_max_withdraw_routes.get()
#define HIVE_MAX_PENDING_TRANSFERS		hive::get_blockchain_configuration().hive_max_pending_transfers.get()
#define HIVE_SAVINGS_WITHDRAW_TIME		hive::get_blockchain_configuration().hive_savings_withdraw_time.get()
#define HIVE_SAVINGS_WITHDRAW_REQUEST_LIMIT		hive::get_blockchain_configuration().hive_savings_withdraw_request_limit.get()
#define HIVE_MAX_VOTE_CHANGES		hive::get_blockchain_configuration().hive_max_vote_changes.get()
#define HIVE_REVERSE_AUCTION_WINDOW_SECONDS_HF6		hive::get_blockchain_configuration().hive_reverse_auction_window_seconds_hf6.get()
#define HIVE_REVERSE_AUCTION_WINDOW_SECONDS_HF20		hive::get_blockchain_configuration().hive_reverse_auction_window_seconds_hf20.get()
#define HIVE_REVERSE_AUCTION_WINDOW_SECONDS_HF21		hive::get_blockchain_configuration().hive_reverse_auction_window_seconds_hf21.get()
#define HIVE_EARLY_VOTING_SECONDS_HF25		hive::get_blockchain_configuration().hive_early_voting_seconds_hf25.get()
#define HIVE_MID_VOTING_SECONDS_HF25		hive::get_blockchain_configuration().hive_mid_voting_seconds_hf25.get()
#define HIVE_MIN_VOTE_INTERVAL_SEC		hive::get_blockchain_configuration().hive_min_vote_interval_sec.get()
#define HIVE_VOTE_DUST_THRESHOLD		hive::get_blockchain_configuration().hive_vote_dust_threshold.get()
#define HIVE_DOWNVOTE_POOL_PERCENT_HF21		hive::get_blockchain_configuration().hive_downvote_pool_percent_hf21.get()
#define HIVE_DELAYED_VOTING_TOTAL_INTERVAL_SECONDS		hive::get_blockchain_configuration().hive_delayed_voting_total_interval_seconds.get()
#define HIVE_DELAYED_VOTING_INTERVAL_SECONDS		hive::get_blockchain_configuration().hive_delayed_voting_interval_seconds.get()
#define HIVE_MIN_ROOT_COMMENT_INTERVAL		hive::get_blockchain_configuration().hive_min_root_comment_interval.get()
#define HIVE_MIN_REPLY_INTERVAL		hive::get_blockchain_configuration().hive_min_reply_interval.get()
#define HIVE_MIN_REPLY_INTERVAL_HF20		hive::get_blockchain_configuration().hive_min_reply_interval_hf20.get()
#define HIVE_MIN_COMMENT_EDIT_INTERVAL		hive::get_blockchain_configuration().hive_min_comment_edit_interval.get()
#define HIVE_POST_AVERAGE_WINDOW		hive::get_blockchain_configuration().hive_post_average_window.get()
#define HIVE_POST_WEIGHT_CONSTANT		hive::get_blockchain_configuration().hive_post_weight_constant.get()
#define HIVE_MAX_ACCOUNT_WITNESS_VOTES		hive::get_blockchain_configuration().hive_max_account_witness_votes.get()
#define HIVE_DEFAULT_HBD_INTEREST_RATE		hive::get_blockchain_configuration().hive_default_hbd_interest_rate.get()
#define HIVE_INFLATION_RATE_START_PERCENT		hive::get_blockchain_configuration().hive_inflation_rate_start_percent.get()
#define HIVE_INFLATION_RATE_STOP_PERCENT		hive::get_blockchain_configuration().hive_inflation_rate_stop_percent.get()
#define HIVE_INFLATION_NARROWING_PERIOD		hive::get_blockchain_configuration().hive_inflation_narrowing_period.get()
#define HIVE_CONTENT_REWARD_PERCENT_HF16		hive::get_blockchain_configuration().hive_content_reward_percent_hf16.get()
#define HIVE_VESTING_FUND_PERCENT_HF16		hive::get_blockchain_configuration().hive_vesting_fund_percent_hf16.get()
#define HIVE_PROPOSAL_FUND_PERCENT_HF0		hive::get_blockchain_configuration().hive_proposal_fund_percent_hf0.get()
#define HIVE_CONTENT_REWARD_PERCENT_HF21		hive::get_blockchain_configuration().hive_content_reward_percent_hf21.get()
#define HIVE_PROPOSAL_FUND_PERCENT_HF21		hive::get_blockchain_configuration().hive_proposal_fund_percent_hf21.get()
#define HIVE_HF21_CONVERGENT_LINEAR_RECENT_CLAIMS		hive::get_blockchain_configuration().hive_hf21_convergent_linear_recent_claims.get()
#define HIVE_CONTENT_CONSTANT_HF21		hive::get_blockchain_configuration().hive_content_constant_hf21.get()
#define HIVE_MINER_PAY_PERCENT		hive::get_blockchain_configuration().hive_miner_pay_percent.get()
#define HIVE_MAX_RATION_DECAY_RATE		hive::get_blockchain_configuration().hive_max_ration_decay_rate.get()
#define HIVE_BANDWIDTH_AVERAGE_WINDOW_SECONDS		hive::get_blockchain_configuration().hive_bandwidth_average_window_seconds.get()
#define HIVE_BANDWIDTH_PRECISION		hive::get_blockchain_configuration().hive_bandwidth_precision.get()
#define HIVE_MAX_COMMENT_DEPTH_PRE_HF17		hive::get_blockchain_configuration().hive_max_comment_depth_pre_hf17.get()
#define HIVE_MAX_COMMENT_DEPTH		hive::get_blockchain_configuration().hive_max_comment_depth.get()
#define HIVE_SOFT_MAX_COMMENT_DEPTH		hive::get_blockchain_configuration().hive_soft_max_comment_depth.get()
#define HIVE_MAX_RESERVE_RATIO		hive::get_blockchain_configuration().hive_max_reserve_ratio.get()
#define HIVE_CREATE_ACCOUNT_WITH_HIVE_MODIFIER		hive::get_blockchain_configuration().hive_create_account_with_hive_modifier.get()
#define HIVE_CREATE_ACCOUNT_DELEGATION_RATIO		hive::get_blockchain_configuration().hive_create_account_delegation_ratio.get()
#define HIVE_CREATE_ACCOUNT_DELEGATION_TIME		hive::get_blockchain_configuration().hive_create_account_delegation_time.get()
#define HIVE_MINING_REWARD		hive::get_blockchain_configuration().hive_mining_reward.get()
#define HIVE_EQUIHASH_N		hive::get_blockchain_configuration().hive_equihash_n.get()
#define HIVE_EQUIHASH_K		hive::get_blockchain_configuration().hive_equihash_k.get()
#define HIVE_LIQUIDITY_TIMEOUT_SEC		hive::get_blockchain_configuration().hive_liquidity_timeout_sec.get()
#define HIVE_MIN_LIQUIDITY_REWARD_PERIOD_SEC		hive::get_blockchain_configuration().hive_min_liquidity_reward_period_sec.get()
#define HIVE_LIQUIDITY_REWARD_BLOCKS		hive::get_blockchain_configuration().hive_liquidity_reward_blocks.get()
#define HIVE_MIN_LIQUIDITY_REWARD		hive::get_blockchain_configuration().hive_min_liquidity_reward.get()
#define HIVE_MIN_CONTENT_REWARD		hive::get_blockchain_configuration().hive_min_content_reward.get()
#define HIVE_MIN_CURATE_REWARD		hive::get_blockchain_configuration().hive_min_curate_reward.get()
#define HIVE_MIN_PRODUCER_REWARD		hive::get_blockchain_configuration().hive_min_producer_reward.get()
#define HIVE_MIN_POW_REWARD		hive::get_blockchain_configuration().hive_min_pow_reward.get()
#define HIVE_ACTIVE_CHALLENGE_FEE		hive::get_blockchain_configuration().hive_active_challenge_fee.get()
#define HIVE_OWNER_CHALLENGE_FEE		hive::get_blockchain_configuration().hive_owner_challenge_fee.get()
#define HIVE_ACTIVE_CHALLENGE_COOLDOWN		hive::get_blockchain_configuration().hive_active_challenge_cooldown.get()
#define HIVE_OWNER_CHALLENGE_COOLDOWN		hive::get_blockchain_configuration().hive_owner_challenge_cooldown.get()
#define HIVE_RECENT_RSHARES_DECAY_TIME_HF17		hive::get_blockchain_configuration().hive_recent_rshares_decay_time_hf17.get()
#define HIVE_RECENT_RSHARES_DECAY_TIME_HF19		hive::get_blockchain_configuration().hive_recent_rshares_decay_time_hf19.get()
#define HIVE_CONTENT_CONSTANT_HF0		hive::get_blockchain_configuration().hive_content_constant_hf0.get()
#define HIVE_MIN_PAYOUT_HBD		hive::get_blockchain_configuration().hive_min_payout_hbd.get()
#define HIVE_HBD_STOP_PERCENT_HF14		hive::get_blockchain_configuration().hive_hbd_stop_percent_hf14.get()
#define HIVE_HBD_STOP_PERCENT_HF20		hive::get_blockchain_configuration().hive_hbd_stop_percent_hf20.get()
#define HIVE_HBD_START_PERCENT_HF14		hive::get_blockchain_configuration().hive_hbd_start_percent_hf14.get()
#define HIVE_HBD_START_PERCENT_HF20		hive::get_blockchain_configuration().hive_hbd_start_percent_hf20.get()
#define HIVE_MAX_ACCOUNT_NAME_LENGTH		hive::get_blockchain_configuration().hive_max_account_name_length.get()
#define HIVE_MIN_PERMLINK_LENGTH		hive::get_blockchain_configuration().hive_min_permlink_length.get()
#define HIVE_MAX_PERMLINK_LENGTH		hive::get_blockchain_configuration().hive_max_permlink_length.get()
#define HIVE_MAX_WITNESS_URL_LENGTH		hive::get_blockchain_configuration().hive_max_witness_url_length.get()
#define HIVE_MAX_SHARE_SUPPLY		hive::get_blockchain_configuration().hive_max_share_supply.get()
#define HIVE_MAX_SATOSHIS		hive::get_blockchain_configuration().hive_max_satoshis.get()
#define HIVE_MAX_SIG_CHECK_DEPTH		hive::get_blockchain_configuration().hive_max_sig_check_depth.get()
#define HIVE_MAX_SIG_CHECK_ACCOUNTS		hive::get_blockchain_configuration().hive_max_sig_check_accounts.get()
#define HIVE_MIN_TRANSACTION_SIZE_LIMIT		hive::get_blockchain_configuration().hive_min_transaction_size_limit.get()
#define HIVE_SECONDS_PER_YEAR		hive::get_blockchain_configuration().hive_seconds_per_year.get()
#define HIVE_HBD_INTEREST_COMPOUND_INTERVAL_SEC		hive::get_blockchain_configuration().hive_hbd_interest_compound_interval_sec.get()
#define HIVE_MAX_TRANSACTION_SIZE		hive::get_blockchain_configuration().hive_max_transaction_size.get()
#define HIVE_MIN_BLOCK_SIZE_LIMIT		hive::get_blockchain_configuration().hive_min_block_size_limit.get()
#define HIVE_MAX_BLOCK_SIZE		hive::get_blockchain_configuration().hive_max_block_size.get()
#define HIVE_SOFT_MAX_BLOCK_SIZE		hive::get_blockchain_configuration().hive_soft_max_block_size.get()
#define HIVE_MIN_BLOCK_SIZE		hive::get_blockchain_configuration().hive_min_block_size.get()
#define HIVE_BLOCKS_PER_HOUR		hive::get_blockchain_configuration().hive_blocks_per_hour.get()
#define HIVE_FEED_INTERVAL_BLOCKS		hive::get_blockchain_configuration().hive_feed_interval_blocks.get()
#define HIVE_FEED_HISTORY_WINDOW_PRE_HF_16		hive::get_blockchain_configuration().hive_feed_history_window_pre_hf_16.get()
#define HIVE_FEED_HISTORY_WINDOW		hive::get_blockchain_configuration().hive_feed_history_window.get()
#define HIVE_MAX_FEED_AGE_SECONDS		hive::get_blockchain_configuration().hive_max_feed_age_seconds.get()
#define HIVE_MIN_FEEDS		hive::get_blockchain_configuration().hive_min_feeds.get()
#define HIVE_CONVERSION_DELAY_PRE_HF_16		hive::get_blockchain_configuration().hive_conversion_delay_pre_hf_16.get()
#define HIVE_CONVERSION_DELAY		hive::get_blockchain_configuration().hive_conversion_delay.get()
#define HIVE_MIN_UNDO_HISTORY		hive::get_blockchain_configuration().hive_min_undo_history.get()
#define HIVE_MAX_UNDO_HISTORY		hive::get_blockchain_configuration().hive_max_undo_history.get()
#define HIVE_MIN_TRANSACTION_EXPIRATION_LIMIT		hive::get_blockchain_configuration().hive_min_transaction_expiration_limit.get()
#define HIVE_BLOCKCHAIN_PRECISION		hive::get_blockchain_configuration().hive_blockchain_precision.get()
#define HIVE_BLOCKCHAIN_PRECISION_DIGITS		hive::get_blockchain_configuration().hive_blockchain_precision_digits.get()
#define HIVE_MAX_INSTANCE_ID		hive::get_blockchain_configuration().hive_max_instance_id.get()
#define HIVE_MAX_AUTHORITY_MEMBERSHIP		hive::get_blockchain_configuration().hive_max_authority_membership.get()
#define HIVE_MAX_ASSET_WHITELIST_AUTHORITIES		hive::get_blockchain_configuration().hive_max_asset_whitelist_authorities.get()
#define HIVE_MAX_URL_LENGTH		hive::get_blockchain_configuration().hive_max_url_length.get()
#define HIVE_VIRTUAL_SCHEDULE_LAP_LENGTH		hive::get_blockchain_configuration().hive_virtual_schedule_lap_length.get()
#define HIVE_VIRTUAL_SCHEDULE_LAP_LENGTH2		hive::get_blockchain_configuration().hive_virtual_schedule_lap_length2.get()
#define HIVE_INITIAL_VOTE_POWER_RATE		hive::get_blockchain_configuration().hive_initial_vote_power_rate.get()
#define HIVE_REDUCED_VOTE_POWER_RATE		hive::get_blockchain_configuration().hive_reduced_vote_power_rate.get()
#define HIVE_MAX_LIMIT_ORDER_EXPIRATION		hive::get_blockchain_configuration().hive_max_limit_order_expiration.get()
#define HIVE_DELEGATION_RETURN_PERIOD_HF0		hive::get_blockchain_configuration().hive_delegation_return_period_hf0.get()
#define HIVE_DELEGATION_RETURN_PERIOD_HF20		hive::get_blockchain_configuration().hive_delegation_return_period_hf20.get()
#define HIVE_RD_MAX_DECAY_BITS		hive::get_blockchain_configuration().hive_rd_max_decay_bits.get()
#define HIVE_RD_MIN_DECAY		hive::get_blockchain_configuration().hive_rd_min_decay.get()
#define HIVE_RD_MIN_BUDGET		hive::get_blockchain_configuration().hive_rd_min_budget.get()
#define HIVE_RD_MAX_DECAY		hive::get_blockchain_configuration().hive_rd_max_decay.get()
#define HIVE_ACCOUNT_SUBSIDY_PRECISION		hive::get_blockchain_configuration().hive_account_subsidy_precision.get()
#define HIVE_WITNESS_SUBSIDY_BUDGET_PERCENT		hive::get_blockchain_configuration().hive_witness_subsidy_budget_percent.get()
#define HIVE_WITNESS_SUBSIDY_DECAY_PERCENT		hive::get_blockchain_configuration().hive_witness_subsidy_decay_percent.get()
#define HIVE_DEFAULT_ACCOUNT_SUBSIDY_DECAY		hive::get_blockchain_configuration().hive_default_account_subsidy_decay.get()
#define HIVE_DEFAULT_ACCOUNT_SUBSIDY_BUDGET		hive::get_blockchain_configuration().hive_default_account_subsidy_budget.get()
#define HIVE_DECAY_BACKSTOP_PERCENT		hive::get_blockchain_configuration().hive_decay_backstop_percent.get()
#define HIVE_BLOCK_GENERATION_POSTPONED_TX_LIMIT		hive::get_blockchain_configuration().hive_block_generation_postponed_tx_limit.get()
#define HIVE_PENDING_TRANSACTION_EXECUTION_LIMIT		hive::get_blockchain_configuration().hive_pending_transaction_execution_limit.get()
#define HIVE_CUSTOM_OP_ID_MAX_LENGTH		hive::get_blockchain_configuration().hive_custom_op_id_max_length.get()
#define HIVE_CUSTOM_OP_DATA_MAX_LENGTH		hive::get_blockchain_configuration().hive_custom_op_data_max_length.get()
#define HIVE_BENEFICIARY_LIMIT		hive::get_blockchain_configuration().hive_beneficiary_limit.get()
#define HIVE_COMMENT_TITLE_LIMIT		hive::get_blockchain_configuration().hive_comment_title_limit.get()
#define HIVE_ONE_DAY_SECONDS		hive::get_blockchain_configuration().hive_one_day_seconds.get()
#define HIVE_ROOT_POST_PARENT		hive::get_blockchain_configuration().hive_root_post_parent.get()
#define HIVE_TREASURY_FEE		hive::get_blockchain_configuration().hive_treasury_fee.get()
#define HIVE_PROPOSAL_SUBJECT_MAX_LENGTH		hive::get_blockchain_configuration().hive_proposal_subject_max_length.get()
#define HIVE_PROPOSAL_MAX_IDS_NUMBER		hive::get_blockchain_configuration().hive_proposal_max_ids_number.get()
#define HIVE_PROPOSAL_FEE_INCREASE_DAYS		hive::get_blockchain_configuration().hive_proposal_fee_increase_days.get()
#define HIVE_PROPOSAL_FEE_INCREASE_DAYS_SEC		hive::get_blockchain_configuration().hive_proposal_fee_increase_days_sec.get()
#define HIVE_PROPOSAL_FEE_INCREASE_AMOUNT		hive::get_blockchain_configuration().hive_proposal_fee_increase_amount.get()
#define HIVE_PROPOSAL_CONVERSION_RATE		hive::get_blockchain_configuration().hive_proposal_conversion_rate.get()

// #define HIVE_INIT_MINER_NAME hive::get_blockchain_configuration().hive_init_miner_name.get()
// #define HIVE_MINER_ACCOUNT hive::get_blockchain_configuration().hive_miner_account.get()
// #define HIVE_NULL_ACCOUNT hive::get_blockchain_configuration().hive_null_account.get()
// #define HIVE_TEMP_ACCOUNT hive::get_blockchain_configuration().hive_temp_account.get()
// #define HIVE_PROXY_TO_SELF_ACCOUNT hive::get_blockchain_configuration().hive_proxy_to_self_account.get()
// #define HIVE_POST_REWARD_FUND_NAME hive::get_blockchain_configuration().hive_post_reward_fund_name.get()
// #define HIVE_COMMENT_REWARD_FUND_NAME hive::get_blockchain_configuration().hive_comment_reward_fund_name.get()
// #define OBSOLETE_TREASURY_ACCOUNT hive::get_blockchain_configuration().obsolete_treasury_account.get()
// #define NEW_HIVE_TREASURY_ACCOUNT hive::get_blockchain_configuration().new_hive_treasury_account.get()

#ifdef	HIVE_ENABLE_SMT

#define SMT_MAX_VOTABLE_ASSETS		hive::get_blockchain_configuration().smt_max_votable_assets.get()
#define SMT_VESTING_WITHDRAW_INTERVAL_SECONDS		hive::get_blockchain_configuration().smt_vesting_withdraw_interval_seconds.get()
#define SMT_UPVOTE_LOCKOUT		hive::get_blockchain_configuration().smt_upvote_lockout.get()
#define SMT_EMISSION_MIN_INTERVAL_SECONDS		hive::get_blockchain_configuration().smt_emission_min_interval_seconds.get()
#define SMT_EMIT_INDEFINITELY		hive::get_blockchain_configuration().smt_emit_indefinitely.get()
#define SMT_MAX_NOMINAL_VOTES_PER_DAY		hive::get_blockchain_configuration().smt_max_nominal_votes_per_day.get()
#define SMT_MAX_VOTES_PER_REGENERATION		hive::get_blockchain_configuration().smt_max_votes_per_regeneration.get()
#define SMT_DEFAULT_VOTES_PER_REGEN_PERIOD		hive::get_blockchain_configuration().smt_default_votes_per_regen_period.get()
#define SMT_DEFAULT_PERCENT_CURATION_REWARDS		hive::get_blockchain_configuration().smt_default_percent_curation_rewards.get()

#endif
