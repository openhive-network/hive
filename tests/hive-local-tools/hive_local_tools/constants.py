"""
This file contains global constants closely related to the behavior of the blockchain, its configuration and specific
rules prevailing in the hive network. e.g.:
 - base accounts created at the start of the test network,
 - single block generation time,
 - number of witnesses in the schedule.

Do not place here variables strictly related to specific test cases. Their place is depending on the scope - module
or package in which they will be used. e.g.:
 - create in the test .py module - when used by tests only in this file,
 - create in proper place in the hive-local-tools - when used by tests in multiple modules/packages.
"""

from typing import Final


TRANSACTION_TEMPLATE: Final[dict] = {
    "ref_block_num": 0,
    "ref_block_prefix": 0,
    "expiration": "1970-01-01T00:00:00",
    "operations": [],
    "extensions": [],
    "signatures": [],
    "transaction_id": "0000000000000000000000000000000000000000",
    "block_num": 0,
    "transaction_num": 0,
}

BASE_ACCOUNTS: Final[list] = ['hive.fund', 'initminer', 'miners', 'null', 'steem.dao', 'temp']

MAX_RECURRENT_TRANSFERS_PER_BLOCK: Final[int] = 1000

MAX_OPEN_RECURRENT_TRANSFERS: Final[int] = 255

# waiting time for approval decline voting rights (60 seconds in testnet / 30 days on mainnet)
TIME_REQUIRED_TO_DECLINE_VOTING_RIGHTS: Final[int] = 21
MAX_CONSECUTIVE_RECURRENT_TRANSFER_FAILURES: Final[int] = 10

MAX_RECURRENT_TRANSFER_END_DATE: Final[int] = 730

# time required to expiration of account recovery request (12 second in testnet / 1 day on mainnet)
ACCOUNT_RECOVERY_REQUEST_EXPIRATION_PERIOD: Final[int] = 4

# time required to change account recovery (60 seconds in testnet / 30 days on mainnet)
OWNER_AUTH_RECOVERY_PERIOD: Final[int] = 21

# minimum time to reconfirm recovery account operation (6 seconds on testnet /  60 minutes on the mainnet)
OWNER_UPDATE_LIMIT: Final[int] = 2

DELAYED_VOTING_TOTAL_INTERVAL_SECONDS: Final[int] = 60 * 60 * 24 * 1 # 1 day in testnet / 30 days on mainnet

DELAYED_VOTING_INTERVAL_SECONDS: Final[int] = DELAYED_VOTING_TOTAL_INTERVAL_SECONDS / 30 # 48 min in testnet / 1 day on mainnet

filters_enum_virtual_ops = {
    "fill_convert_request_operation": 0x000001,
    "author_reward_operation": 0x000002,
    "curation_reward_operation": 0x000004,
    "comment_reward_operation": 0x000008,
    "liquidity_reward_operation": 0x000010,
    "interest_operation": 0x000020,
    "fill_vesting_withdraw_operation": 0x000040,
    "fill_order_operation": 0x000080,
    "shutdown_witness_operation": 0x000100,
    "fill_transfer_from_savings_operation": 0x000200,
    "hardfork_operation": 0x000400,
    "comment_payout_update_operation": 0x000800,
    "return_vesting_delegation_operation": 0x001000,
    "comment_benefactor_reward_operation": 0x002000,
    "producer_reward_operation": 0x004000,
    "clear_null_account_balance_operation": 0x008000,
    "proposal_pay_operation": 0x010000,
    "sps_fund_operation": 0x020000,
    "hardfork_hive_operation": 0x040000,
    "hardfork_hive_restore_operation": 0x080000,
    "delayed_voting_operation": 0x100000,
    "consolidate_treasury_balance_operation": 0x200000,
    "effective_comment_vote_operation": 0x400000,
    "ineffective_delete_comment_operation": 0x800000,
    "sps_convert_operation": 0x1000000,
    "dhf_funding_operation": 0x0020000,
    "dhf_conversion_operation": 0x1000000,
    "expired_account_notification_operation": 0x2000000,
    "changed_recovery_account_operation": 0x4000000,
    "transfer_to_vesting_completed_operation": 0x8000000,
    "pow_reward_operation": 0x10000000,
    "vesting_shares_split_operation": 0x20000000,
    "account_created_operation": 0x40000000,
    "fill_collateralized_convert_request_operation": 0x80000000,
    "system_warning_operation": 0x100000000,
    "fill_recurrent_transfer_operation": 0x200000000,
    "failed_recurrent_transfer_operation": 0x400000000,
    "limit_order_cancelled_operation": 0x800000000,
    "producer_missed_operation": 0x1000000000,
    "proposal_fee_operation": 0x2000000000,
    "collateralized_convert_immediate_conversion_operation": 0x4000000000,
    "escrow_approved_operation": 0x8000000000,
    "escrow_rejected_operation": 0x10000000000,
    "proxy_cleared_operation": 0x20000000000,
}
