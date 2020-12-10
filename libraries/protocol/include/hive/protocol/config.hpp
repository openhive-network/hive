/*
 * Copyright (c) 2016 Steemit, Inc., and contributors.
 */
#pragma once
#include <hive/protocol/hardfork.hpp>

// WARNING!
// Every symbol defined here needs to be handled appropriately in get_config.cpp
// This is checked by get_config_check.sh called from Dockerfile

#ifdef IS_TEST_NET
#define HIVE_BLOCKCHAIN_VERSION               ( version(1, 25, 0) )

#define HIVE_INIT_PRIVATE_KEY                 (fc::ecc::private_key::regenerate(fc::sha256::hash(std::string("init_key"))))
#define HIVE_INIT_PUBLIC_KEY_STR              (std::string( hive::protocol::public_key_type(HIVE_INIT_PRIVATE_KEY.get_public_key()) ))
#define STEEM_CHAIN_ID                        (fc::sha256::hash("testnet"))
#define HIVE_CHAIN_ID                         (fc::sha256::hash("testnet"))
#define HIVE_ADDRESS_PREFIX                   "TST"

#define HIVE_GENESIS_TIME                     (fc::time_point_sec(1451606400))
#define HIVE_MINING_TIME                      (fc::time_point_sec(1451606400))
#define HIVE_CASHOUT_WINDOW_SECONDS           (60*60) /// 1 hr
#define HIVE_CASHOUT_WINDOW_SECONDS_PRE_HF12  (HIVE_CASHOUT_WINDOW_SECONDS)
#define HIVE_CASHOUT_WINDOW_SECONDS_PRE_HF17  (HIVE_CASHOUT_WINDOW_SECONDS)
#define HIVE_SECOND_CASHOUT_WINDOW            (60*60*24*3) /// 3 days
#define HIVE_MAX_CASHOUT_WINDOW_SECONDS       (60*60*24) /// 1 day
#define HIVE_UPVOTE_LOCKOUT_HF7               (fc::minutes(1))
#define HIVE_UPVOTE_LOCKOUT_SECONDS           (60*5)    /// 5 minutes
#define HIVE_UPVOTE_LOCKOUT_HF17              (fc::minutes(5))


#define HIVE_MIN_ACCOUNT_CREATION_FEE         0
#define HIVE_MAX_ACCOUNT_CREATION_FEE         int64_t(1000000000)

#define HIVE_OWNER_AUTH_RECOVERY_PERIOD                   fc::seconds(60)
#define HIVE_ACCOUNT_RECOVERY_REQUEST_EXPIRATION_PERIOD   fc::seconds(12)
#define HIVE_OWNER_UPDATE_LIMIT                           fc::seconds(0)
#define HIVE_OWNER_AUTH_HISTORY_TRACKING_START_BLOCK_NUM  1

#define HIVE_INIT_SUPPLY                      (int64_t( 250 ) * int64_t( 1000000 ) * int64_t( 1000 ))
#define HIVE_HBD_INIT_SUPPLY                  (int64_t( 7 ) * int64_t( 1000000 ) * int64_t( 1000 ))

/// Allows to limit number of total produced blocks.
#define TESTNET_BLOCK_LIMIT                   (3000000)

#define HIVE_PROPOSAL_MAINTENANCE_PERIOD          3600
#define HIVE_PROPOSAL_MAINTENANCE_CLEANUP         (60*60*24*1) // 1 day
#define HIVE_DAILY_PROPOSAL_MAINTENANCE_PERIOD           (60*60) /// 1 hour

#else // IS LIVE HIVE NETWORK

#define HIVE_BLOCKCHAIN_VERSION               ( version(1, 24, 7) )

#define HIVE_INIT_PUBLIC_KEY_STR              "STM8GC13uCZbP44HzMLV6zPZGwVQ8Nt4Kji8PapsPiNq1BK153XTX"
#define STEEM_CHAIN_ID                        fc::sha256()
#define HIVE_CHAIN_ID                         fc::sha256("beeab0de00000000000000000000000000000000000000000000000000000000")
#define HIVE_ADDRESS_PREFIX                   "STM"

#define HIVE_GENESIS_TIME                     (fc::time_point_sec(1458835200))
#define HIVE_MINING_TIME                      (fc::time_point_sec(1458838800))
#define HIVE_CASHOUT_WINDOW_SECONDS_PRE_HF12  (60*60*24)    /// 1 day
#define HIVE_CASHOUT_WINDOW_SECONDS_PRE_HF17  (60*60*12)    /// 12 hours
#define HIVE_CASHOUT_WINDOW_SECONDS           (60*60*24*7)  /// 7 days
#define HIVE_SECOND_CASHOUT_WINDOW            (60*60*24*30) /// 30 days
#define HIVE_MAX_CASHOUT_WINDOW_SECONDS       (60*60*24*14) /// 2 weeks
#define HIVE_UPVOTE_LOCKOUT_HF7               (fc::minutes(1))
#define HIVE_UPVOTE_LOCKOUT_SECONDS           (60*60*12)    /// 12 hours
#define HIVE_UPVOTE_LOCKOUT_HF17              (fc::hours(12))

#define HIVE_MIN_ACCOUNT_CREATION_FEE         1
#define HIVE_MAX_ACCOUNT_CREATION_FEE         int64_t(1000000000)

#define HIVE_OWNER_AUTH_RECOVERY_PERIOD                   fc::days(30)
#define HIVE_ACCOUNT_RECOVERY_REQUEST_EXPIRATION_PERIOD   fc::days(1)
#define HIVE_OWNER_UPDATE_LIMIT                           fc::minutes(60)
#define HIVE_OWNER_AUTH_HISTORY_TRACKING_START_BLOCK_NUM  3186477

#define HIVE_INIT_SUPPLY                      int64_t(0)
#define HIVE_HBD_INIT_SUPPLY                  int64_t(0)

#define HIVE_PROPOSAL_MAINTENANCE_PERIOD           3600
#define HIVE_PROPOSAL_MAINTENANCE_CLEANUP          (60*60*24*1) /// 1 day
#define HIVE_DAILY_PROPOSAL_MAINTENANCE_PERIOD           HIVE_ONE_DAY_SECONDS

#endif

#define VESTS_SYMBOL  (hive::protocol::asset_symbol_type::from_asset_num( HIVE_ASSET_NUM_VESTS ) )
#define HIVE_SYMBOL   (hive::protocol::asset_symbol_type::from_asset_num( HIVE_ASSET_NUM_HIVE ) )
#define HBD_SYMBOL    (hive::protocol::asset_symbol_type::from_asset_num( HIVE_ASSET_NUM_HBD ) )

#define HIVE_BLOCKCHAIN_HARDFORK_VERSION      ( hardfork_version( HIVE_BLOCKCHAIN_VERSION ) )

#define HIVE_100_PERCENT                      10000
#define HIVE_1_PERCENT                        (HIVE_100_PERCENT/100)
#define HIVE_1_BASIS_POINT                    (HIVE_100_PERCENT/10000) // 0.01%

#define HIVE_BLOCK_INTERVAL                   3
#define HIVE_BLOCKS_PER_YEAR                  (365*24*60*60/HIVE_BLOCK_INTERVAL)
#define HIVE_BLOCKS_PER_DAY                   (24*60*60/HIVE_BLOCK_INTERVAL)
#define HIVE_START_VESTING_BLOCK              (HIVE_BLOCKS_PER_DAY * 7)
#define HIVE_START_MINER_VOTING_BLOCK         (HIVE_BLOCKS_PER_DAY * 30)

#define HIVE_INIT_MINER_NAME                  "initminer"
#define HIVE_NUM_INIT_MINERS                  1
#define HIVE_INIT_TIME                        (fc::time_point_sec())

#define HIVE_MAX_WITNESSES                    21

#define HIVE_MAX_VOTED_WITNESSES_HF0          19
#define HIVE_MAX_MINER_WITNESSES_HF0          1
#define HIVE_MAX_RUNNER_WITNESSES_HF0         1

#define HIVE_MAX_VOTED_WITNESSES_HF17         20
#define HIVE_MAX_MINER_WITNESSES_HF17         0
#define HIVE_MAX_RUNNER_WITNESSES_HF17        1

#define HIVE_HARDFORK_REQUIRED_WITNESSES      17 // 17 of the 21 dpos witnesses (20 elected and 1 virtual time) required for hardfork. This guarantees 75% participation on all subsequent rounds.
#define HIVE_MAX_TIME_UNTIL_EXPIRATION        (60*60) // seconds,  aka: 1 hour
#define HIVE_MAX_MEMO_SIZE                    2048
#define HIVE_MAX_PROXY_RECURSION_DEPTH        4
#define HIVE_VESTING_WITHDRAW_INTERVALS_PRE_HF_16 104
#define HIVE_VESTING_WITHDRAW_INTERVALS       13
#define HIVE_VESTING_WITHDRAW_INTERVAL_SECONDS (60*60*24*7) /// 1 week per interval
#define HIVE_MAX_WITHDRAW_ROUTES              10
#define HIVE_MAX_PENDING_TRANSFERS            255
#define HIVE_SAVINGS_WITHDRAW_TIME            (fc::days(3))
#define HIVE_SAVINGS_WITHDRAW_REQUEST_LIMIT   100
#define HIVE_VOTING_MANA_REGENERATION_SECONDS (5*60*60*24) // 5 day
#define HIVE_MAX_VOTE_CHANGES                 5
#define HIVE_REVERSE_AUCTION_WINDOW_SECONDS_HF6 (60*30) /// 30 minutes
#define HIVE_REVERSE_AUCTION_WINDOW_SECONDS_HF20 (60*15) /// 15 minutes
#define HIVE_REVERSE_AUCTION_WINDOW_SECONDS_HF21 (60*5) /// 5 minutes
#define HIVE_MIN_VOTE_INTERVAL_SEC            3
#define HIVE_VOTE_DUST_THRESHOLD              (50000000)
#define HIVE_DOWNVOTE_POOL_PERCENT_HF21       (25*HIVE_1_PERCENT)

#define HIVE_DELAYED_VOTING_TOTAL_INTERVAL_SECONDS (60*60*24*30) // 30 days
#define HIVE_DELAYED_VOTING_INTERVAL_SECONDS       (60*60*24*1)  // 1 day

#define HIVE_MIN_ROOT_COMMENT_INTERVAL        (fc::seconds(60*5)) // 5 minutes
#define HIVE_MIN_REPLY_INTERVAL               (fc::seconds(20)) // 20 seconds
#define HIVE_MIN_REPLY_INTERVAL_HF20          (fc::seconds(3)) // 3 seconds
#define HIVE_MIN_COMMENT_EDIT_INTERVAL        (fc::seconds(3)) // 3 seconds
#define HIVE_POST_AVERAGE_WINDOW              (60*60*24u) // 1 day
#define HIVE_POST_WEIGHT_CONSTANT             (uint64_t(4*HIVE_100_PERCENT) * (4*HIVE_100_PERCENT))// (4*HIVE_100_PERCENT) -> 2 posts per 1 days, average 1 every 12 hours

#define HIVE_MAX_ACCOUNT_WITNESS_VOTES        30

#define HIVE_DEFAULT_HBD_INTEREST_RATE        (10*HIVE_1_PERCENT) ///< 10% APR

#define HIVE_INFLATION_RATE_START_PERCENT     (978) // Fixes block 7,000,000 to 9.5%
#define HIVE_INFLATION_RATE_STOP_PERCENT      (95) // 0.95%
#define HIVE_INFLATION_NARROWING_PERIOD       (250000) // Narrow 0.01% every 250k blocks
#define HIVE_CONTENT_REWARD_PERCENT_HF16      (75*HIVE_1_PERCENT) //75% of inflation, 7.125% inflation
#define HIVE_VESTING_FUND_PERCENT_HF16        (15*HIVE_1_PERCENT) //15% of inflation, 1.425% inflation
#define HIVE_PROPOSAL_FUND_PERCENT_HF0        (0)

#define HIVE_CONTENT_REWARD_PERCENT_HF21      (65*HIVE_1_PERCENT)
#define HIVE_PROPOSAL_FUND_PERCENT_HF21       (10*HIVE_1_PERCENT)

#define HIVE_HF21_CONVERGENT_LINEAR_RECENT_CLAIMS (fc::uint128_t(0,503600561838938636ull))
#define HIVE_CONTENT_CONSTANT_HF21            (fc::uint128_t(0,2000000000000ull))

#define HIVE_MINER_PAY_PERCENT                (HIVE_1_PERCENT) // 1%
#define HIVE_MAX_RATION_DECAY_RATE            (1000000)

#define HIVE_BANDWIDTH_AVERAGE_WINDOW_SECONDS (60*60*24*7) ///< 1 week
#define HIVE_BANDWIDTH_PRECISION              (uint64_t(1000000)) ///< 1 million
#define HIVE_MAX_COMMENT_DEPTH_PRE_HF17       6
#define HIVE_MAX_COMMENT_DEPTH                0xffff // 64k
#define HIVE_SOFT_MAX_COMMENT_DEPTH           0xff // 255

#define HIVE_MAX_RESERVE_RATIO                (20000)

#define HIVE_CREATE_ACCOUNT_WITH_HIVE_MODIFIER   30
#define HIVE_CREATE_ACCOUNT_DELEGATION_RATIO     5
#define HIVE_CREATE_ACCOUNT_DELEGATION_TIME      fc::days(30)

#define HIVE_MINING_REWARD                    asset( 1000, HIVE_SYMBOL )
#define HIVE_EQUIHASH_N                       140
#define HIVE_EQUIHASH_K                       6

#define HIVE_LIQUIDITY_TIMEOUT_SEC            (fc::seconds(60*60*24*7)) // After one week volume is set to 0
#define HIVE_MIN_LIQUIDITY_REWARD_PERIOD_SEC  (fc::seconds(60)) // 1 minute required on books to receive volume
#define HIVE_LIQUIDITY_REWARD_PERIOD_SEC      (60*60)
#define HIVE_LIQUIDITY_REWARD_BLOCKS          (HIVE_LIQUIDITY_REWARD_PERIOD_SEC/HIVE_BLOCK_INTERVAL)
#define HIVE_MIN_LIQUIDITY_REWARD             (asset( 1000*HIVE_LIQUIDITY_REWARD_BLOCKS, HIVE_SYMBOL )) // Minumum reward to be paid out to liquidity providers
#define HIVE_MIN_CONTENT_REWARD               HIVE_MINING_REWARD
#define HIVE_MIN_CURATE_REWARD                HIVE_MINING_REWARD
#define HIVE_MIN_PRODUCER_REWARD              HIVE_MINING_REWARD
#define HIVE_MIN_POW_REWARD                   HIVE_MINING_REWARD

#define HIVE_ACTIVE_CHALLENGE_FEE             asset( 2000, HIVE_SYMBOL )
#define HIVE_OWNER_CHALLENGE_FEE              asset( 30000, HIVE_SYMBOL )
#define HIVE_ACTIVE_CHALLENGE_COOLDOWN        fc::days(1)
#define HIVE_OWNER_CHALLENGE_COOLDOWN         fc::days(1)

#define HIVE_POST_REWARD_FUND_NAME            ("post")
#define HIVE_COMMENT_REWARD_FUND_NAME         ("comment")
#define HIVE_RECENT_RSHARES_DECAY_TIME_HF17   (fc::days(30))
#define HIVE_RECENT_RSHARES_DECAY_TIME_HF19   (fc::days(15))
#define HIVE_CONTENT_CONSTANT_HF0             (uint128_t(uint64_t(2000000000000ll)))
// note, if redefining these constants make sure calculate_claims doesn't overflow

// 5ccc e802 de5f
// int(expm1( log1p( 1 ) / BLOCKS_PER_YEAR ) * 2**HIVE_APR_PERCENT_SHIFT_PER_BLOCK / 100000 + 0.5)
// we use 100000 here instead of 10000 because we end up creating an additional 9x for vesting
#define HIVE_APR_PERCENT_MULTIPLY_PER_BLOCK   ( (uint64_t( 0x5ccc ) << 0x20) \
                                              | (uint64_t( 0xe802 ) << 0x10) \
                                              | (uint64_t( 0xde5f )        ) \
                                              )
// chosen to be the maximal value such that HIVE_APR_PERCENT_MULTIPLY_PER_BLOCK * 2**64 * 100000 < 2**128
#define HIVE_APR_PERCENT_SHIFT_PER_BLOCK      87

#define HIVE_APR_PERCENT_MULTIPLY_PER_ROUND   ( (uint64_t( 0x79cc ) << 0x20 ) \
                                              | (uint64_t( 0xf5c7 ) << 0x10 ) \
                                              | (uint64_t( 0x3480 )         ) \
                                              )

#define HIVE_APR_PERCENT_SHIFT_PER_ROUND      83

// We have different constants for hourly rewards
// i.e. hex(int(math.expm1( math.log1p( 1 ) / HOURS_PER_YEAR ) * 2**HIVE_APR_PERCENT_SHIFT_PER_HOUR / 100000 + 0.5))
#define HIVE_APR_PERCENT_MULTIPLY_PER_HOUR    ( (uint64_t( 0x6cc1 ) << 0x20) \
                                              | (uint64_t( 0x39a1 ) << 0x10) \
                                              | (uint64_t( 0x5cbd )        ) \
                                              )

// chosen to be the maximal value such that HIVE_APR_PERCENT_MULTIPLY_PER_HOUR * 2**64 * 100000 < 2**128
#define HIVE_APR_PERCENT_SHIFT_PER_HOUR       77

// These constants add up to GRAPHENE_100_PERCENT.  Each GRAPHENE_1_PERCENT is equivalent to 1% per year APY
// *including the corresponding 9x vesting rewards*
#define HIVE_CURATE_APR_PERCENT               3875
#define HIVE_CONTENT_APR_PERCENT              3875
#define HIVE_LIQUIDITY_APR_PERCENT            750
#define HIVE_PRODUCER_APR_PERCENT             750
#define HIVE_POW_APR_PERCENT                  750

#define HIVE_MIN_PAYOUT_HBD                   (asset(20,HBD_SYMBOL))

#define HIVE_HBD_STOP_PERCENT_HF14            (5*HIVE_1_PERCENT ) // Stop printing HBD at 5% Market Cap
#define HIVE_HBD_STOP_PERCENT_HF20            (10*HIVE_1_PERCENT ) // Stop printing HBD at 10% Market Cap
#define HIVE_HBD_START_PERCENT_HF14           (2*HIVE_1_PERCENT) // Start reducing printing of HBD at 2% Market Cap
#define HIVE_HBD_START_PERCENT_HF20           (9*HIVE_1_PERCENT) // Start reducing printing of HBD at 9% Market Cap

#define HIVE_MIN_ACCOUNT_NAME_LENGTH          3
#define HIVE_MAX_ACCOUNT_NAME_LENGTH          16

#define HIVE_MIN_PERMLINK_LENGTH              0
#define HIVE_MAX_PERMLINK_LENGTH              256
#define HIVE_MAX_WITNESS_URL_LENGTH           2048

#define HIVE_MAX_SHARE_SUPPLY                 int64_t(1000000000000000ll)
#define HIVE_MAX_SATOSHIS                     int64_t(4611686018427387903ll)
#define HIVE_MAX_SIG_CHECK_DEPTH              2
#define HIVE_MAX_SIG_CHECK_ACCOUNTS           125

#define HIVE_MIN_TRANSACTION_SIZE_LIMIT       1024
#define HIVE_SECONDS_PER_YEAR                 (uint64_t(60*60*24*365ll))

#define HIVE_HBD_INTEREST_COMPOUND_INTERVAL_SEC (60*60*24*30)
#define HIVE_MAX_TRANSACTION_SIZE             (1024*64)
#define HIVE_MIN_BLOCK_SIZE_LIMIT             (HIVE_MAX_TRANSACTION_SIZE)
#define HIVE_MAX_BLOCK_SIZE                   (HIVE_MAX_TRANSACTION_SIZE*HIVE_BLOCK_INTERVAL*2000)
#define HIVE_SOFT_MAX_BLOCK_SIZE              (2*1024*1024)
#define HIVE_MIN_BLOCK_SIZE                   115
#define HIVE_BLOCKS_PER_HOUR                  (60*60/HIVE_BLOCK_INTERVAL)
#define HIVE_FEED_INTERVAL_BLOCKS             (HIVE_BLOCKS_PER_HOUR)
#define HIVE_FEED_HISTORY_WINDOW_PRE_HF_16    (24*7) /// 7 days * 24 hours per day
#define HIVE_FEED_HISTORY_WINDOW              (12*7) // 3.5 days
#define HIVE_MAX_FEED_AGE_SECONDS             (60*60*24*7) // 7 days
#define HIVE_MIN_FEEDS                        (HIVE_MAX_WITNESSES/3) /// protects the network from conversions before price has been established
#define HIVE_CONVERSION_DELAY_PRE_HF_16       (fc::days(7))
#define HIVE_CONVERSION_DELAY                 (fc::hours(HIVE_FEED_HISTORY_WINDOW)) //3.5 day conversion

#define HIVE_MIN_UNDO_HISTORY                 10
#define HIVE_MAX_UNDO_HISTORY                 10000

#define HIVE_MIN_TRANSACTION_EXPIRATION_LIMIT (HIVE_BLOCK_INTERVAL * 5) // 5 transactions per block
#define HIVE_BLOCKCHAIN_PRECISION             uint64_t( 1000 )

#define HIVE_BLOCKCHAIN_PRECISION_DIGITS      3
#define HIVE_MAX_INSTANCE_ID                  (uint64_t(-1)>>16)
/** NOTE: making this a power of 2 (say 2^15) would greatly accelerate fee calcs */
#define HIVE_MAX_AUTHORITY_MEMBERSHIP         40
#define HIVE_MAX_ASSET_WHITELIST_AUTHORITIES  10
#define HIVE_MAX_URL_LENGTH                   127

#define HIVE_IRREVERSIBLE_THRESHOLD           (75 * HIVE_1_PERCENT)

#define HIVE_VIRTUAL_SCHEDULE_LAP_LENGTH      ( fc::uint128(uint64_t(-1)) )
#define HIVE_VIRTUAL_SCHEDULE_LAP_LENGTH2     ( fc::uint128::max_value() )

#define HIVE_INITIAL_VOTE_POWER_RATE          (40)
#define HIVE_REDUCED_VOTE_POWER_RATE          (10)

#define HIVE_MAX_LIMIT_ORDER_EXPIRATION       (60*60*24*28) // 28 days
#define HIVE_DELEGATION_RETURN_PERIOD_HF0     (HIVE_CASHOUT_WINDOW_SECONDS)
#define HIVE_DELEGATION_RETURN_PERIOD_HF20    (HIVE_VOTING_MANA_REGENERATION_SECONDS)

#define HIVE_RD_MIN_DECAY_BITS                6
#define HIVE_RD_MAX_DECAY_BITS                32
#define HIVE_RD_DECAY_DENOM_SHIFT             36
#define HIVE_RD_MAX_POOL_BITS                 64
#define HIVE_RD_MAX_BUDGET_1                  ((uint64_t(1) << (HIVE_RD_MAX_POOL_BITS + HIVE_RD_MIN_DECAY_BITS - HIVE_RD_DECAY_DENOM_SHIFT))-1)
#define HIVE_RD_MAX_BUDGET_2                  ((uint64_t(1) << (64-HIVE_RD_DECAY_DENOM_SHIFT))-1)
#define HIVE_RD_MAX_BUDGET_3                  (uint64_t( std::numeric_limits<int32_t>::max() ))
#define HIVE_RD_MAX_BUDGET                    (int32_t( std::min( { HIVE_RD_MAX_BUDGET_1, HIVE_RD_MAX_BUDGET_2, HIVE_RD_MAX_BUDGET_3 } )) )
#define HIVE_RD_MIN_DECAY                     (uint32_t(1) << HIVE_RD_MIN_DECAY_BITS)
#define HIVE_RD_MIN_BUDGET                    1
#define HIVE_RD_MAX_DECAY                     (uint32_t(0xFFFFFFFF))

#define HIVE_ACCOUNT_SUBSIDY_PRECISION        (HIVE_100_PERCENT)

// We want the global subsidy to run out first in normal (Poisson)
// conditions, so we boost the per-witness subsidy a little.
#define HIVE_WITNESS_SUBSIDY_BUDGET_PERCENT   (125 * HIVE_1_PERCENT)

// Since witness decay only procs once per round, multiplying the decay
// constant by the number of witnesses means the per-witness pools have
// the same effective decay rate in real-time terms.
#define HIVE_WITNESS_SUBSIDY_DECAY_PERCENT    (HIVE_MAX_WITNESSES * HIVE_100_PERCENT)

// 347321 corresponds to a 5-day halflife
#define HIVE_DEFAULT_ACCOUNT_SUBSIDY_DECAY    (347321)
// Default rate is 0.5 accounts per block
#define HIVE_DEFAULT_ACCOUNT_SUBSIDY_BUDGET   (797)
#define HIVE_DECAY_BACKSTOP_PERCENT           (90 * HIVE_1_PERCENT)

#define HIVE_BLOCK_GENERATION_POSTPONED_TX_LIMIT 5
#define HIVE_PENDING_TRANSACTION_EXECUTION_LIMIT fc::milliseconds(200)

#define HIVE_CUSTOM_OP_ID_MAX_LENGTH          (32)
#define HIVE_CUSTOM_OP_DATA_MAX_LENGTH        (8192)
#define HIVE_BENEFICIARY_LIMIT                (128)
#define HIVE_COMMENT_TITLE_LIMIT              (256)

#define HIVE_ONE_DAY_SECONDS                  (60*60*24) // One day in seconds

/**
 *  Reserved Account IDs with special meaning
 */
///@{
/// Represents the current witnesses
#define HIVE_MINER_ACCOUNT                    "miners"
/// Represents the canonical account with NO authority (nobody can access funds in null account)
#define HIVE_NULL_ACCOUNT                     "null"
/// Represents the canonical account with WILDCARD authority (anybody can access funds in temp account)
#define HIVE_TEMP_ACCOUNT                     "temp"
/// Represents the canonical account for specifying you will vote for directly (as opposed to a proxy)
#define HIVE_PROXY_TO_SELF_ACCOUNT            ""
/// Represents the canonical root post parent account
#define HIVE_ROOT_POST_PARENT                 (account_name_type())
/// Represents the account with NO authority which holds resources for payouts according to given proposals
//#define HIVE_TREASURY_ACCOUNT                 "steem.dao" //no longer constant, changed in HF24 - use database::get_treasury_name() instead
//note that old account is still considered a treasury (cannot be reused for other purposes), just all funds and actions are redirected to new one
//DO NOT USE the following constants anywhere other than inside database::get_treasury_name()
#define OBSOLETE_TREASURY_ACCOUNT             "steem.dao"
#define NEW_HIVE_TREASURY_ACCOUNT             "hive.fund"
///@}

/// HIVE PROPOSAL SYSTEM support

#define HIVE_TREASURY_FEE                          (10 * HIVE_BLOCKCHAIN_PRECISION)
#define HIVE_PROPOSAL_SUBJECT_MAX_LENGTH           80
/// Max number of IDs passed at once to the update_proposal_voter_operation or remove_proposal_operation.
#define HIVE_PROPOSAL_MAX_IDS_NUMBER               5
#define HIVE_PROPOSAL_FEE_INCREASE_DAYS            60
#define HIVE_PROPOSAL_FEE_INCREASE_DAYS_SEC        (60*60*24*HIVE_PROPOSAL_FEE_INCREASE_DAYS) /// 60 days
#define HIVE_PROPOSAL_FEE_INCREASE_AMOUNT          (1 * HIVE_BLOCKCHAIN_PRECISION)
#define HIVE_PROPOSAL_CONVERSION_RATE             (5 * HIVE_1_BASIS_POINT)

#ifdef HIVE_ENABLE_SMT

#define SMT_MAX_VOTABLE_ASSETS 2
#define SMT_VESTING_WITHDRAW_INTERVAL_SECONDS   (60*60*24*7) /// 1 week per interval
#define SMT_UPVOTE_LOCKOUT                      (60*60*12)   /// 12 hours
#define SMT_EMISSION_MIN_INTERVAL_SECONDS       (60*60*6)    /// 6 hours
#define SMT_EMIT_INDEFINITELY                   (std::numeric_limits<uint32_t>::max())
#define SMT_MAX_NOMINAL_VOTES_PER_DAY           (1000)
#define SMT_MAX_VOTES_PER_REGENERATION          ((SMT_MAX_NOMINAL_VOTES_PER_DAY * SMT_VESTING_WITHDRAW_INTERVAL_SECONDS) / 86400)
#define SMT_DEFAULT_VOTES_PER_REGEN_PERIOD      (50)
#define SMT_DEFAULT_PERCENT_CURATION_REWARDS    (25 * HIVE_1_PERCENT)

#endif /// HIVE_ENABLE_SMT

