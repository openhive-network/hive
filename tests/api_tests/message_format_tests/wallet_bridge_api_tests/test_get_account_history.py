import pytest

import test_tools as tt

from .local_tools import as_string, run_for

from .block_log.generate_block_log import ACCOUNTS


TESTNET_ACCOUNTS = [f'account-{i}' for i in range(10)]
MAINNET_ACCOUNT = 'gtg'


UINT64_MAX = 2**64 - 1

CORRECT_VALUES = [
        # ACCOUNT
        (TESTNET_ACCOUNTS[0], -1, 1000),
        ('non-exist-acc', -1, 1000),
        ('', -1, 1000),
        (100, -1, 1000),
        (True, -1, 1000),

        # FROM
        (TESTNET_ACCOUNTS[0], UINT64_MAX, 1000),
        (TESTNET_ACCOUNTS[0], 2, 1),
        (TESTNET_ACCOUNTS[0], 0, 1),
        (TESTNET_ACCOUNTS[0], -1, 1000),
        (TESTNET_ACCOUNTS[5], -2, 1000),

        # LIMIT
        (TESTNET_ACCOUNTS[0], -1, 0),
        (TESTNET_ACCOUNTS[0], -1, 1000),
]


@pytest.mark.parametrize(
    'account, from_, limit', [
        *CORRECT_VALUES,
        *as_string(CORRECT_VALUES),
        (TESTNET_ACCOUNTS[0], True, 1),  # bool is treated like numeric (0:1)
        (TESTNET_ACCOUNTS[0], -1, True),  # bool is treated like numeric (0:1)
    ]
)
@run_for('testnet_replayed', 'mainnet_5m', 'mainnet_64m')
def test_get_account_history_with_correct_value(prepared_node, account, from_, limit):
    # A replayed node was used. Because of this, there is no need to wait for transactions
    # to become visible in `get_account_history`
    prepared_node.api.wallet_bridge.get_account_history(account, from_, limit)



@pytest.mark.parametrize(
    'account, from_, limit', [
        # FROM
        (TESTNET_ACCOUNTS[5], UINT64_MAX + 1, 1000),

        # LIMIT
        (TESTNET_ACCOUNTS[5], -1, -1),
        (TESTNET_ACCOUNTS[0], 1, 0),
        (TESTNET_ACCOUNTS[5], -1, 1001),
    ]
)
@run_for('testnet', 'mainnet_5m', 'mainnet_64m')
def test_get_account_history_with_incorrect_value(prepared_node, should_prepare, account, from_, limit):
    if not should_prepare:
        account = MAINNET_ACCOUNT
    with pytest.raises(tt.exceptions.CommunicationError):
        prepared_node.api.wallet_bridge.get_account_history(account, from_, limit)


@pytest.mark.parametrize(
    'account, from_, limit', [
        # ACCOUNT
        (['example_array'], -1, 1000),

        # FROM
        (TESTNET_ACCOUNTS[5], 'example-string-argument', 1000),
        (TESTNET_ACCOUNTS[5], 'true', 1000),
        (TESTNET_ACCOUNTS[5], [-1], 1000),

        # LIMIT
        (TESTNET_ACCOUNTS[5], -1, 'example-string-argument'),
        (TESTNET_ACCOUNTS[5], -1, [1000]),
    ]
)
@run_for('testnet', 'mainnet_5m', 'mainnet_64m')
def test_get_account_history_with_incorrect_type_of_argument(prepared_node, should_prepare, account, from_, limit):
    if should_prepare:
        wallet = tt.Wallet(attach_to=prepared_node)
        wallet.create_accounts(len(TESTNET_ACCOUNTS))
    elif account in TESTNET_ACCOUNTS:
        account = MAINNET_ACCOUNT
    with pytest.raises(tt.exceptions.CommunicationError):
        prepared_node.api.wallet_bridge.get_account_history(account, from_, limit)
