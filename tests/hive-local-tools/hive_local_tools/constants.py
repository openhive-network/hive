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
