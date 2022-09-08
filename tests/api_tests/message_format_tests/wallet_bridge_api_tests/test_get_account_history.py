import pytest

import test_tools as tt

from .local_tools import as_string, run_for

from .block_log.generate_block_log import ACCOUNTS


UINT64_MAX = 2**64 - 1

CORRECT_VALUES = [
        # ACCOUNT
        (ACCOUNTS[0], -1, 1000),
        ('non-exist-acc', -1, 1000),
        ('', -1, 1000),
        (100, -1, 1000),
        (True, -1, 1000),

        # FROM
        (ACCOUNTS[0], UINT64_MAX, 1000),
        (ACCOUNTS[0], 2, 1),
        (ACCOUNTS[0], 0, 1),
        (ACCOUNTS[0], -1, 1000),
        (ACCOUNTS[5], -2, 1000),

        # LIMIT
        (ACCOUNTS[0], -1, 0),
        (ACCOUNTS[0], -1, 1000),
]


@pytest.mark.parametrize(
    'account, from_, limit', [
        *CORRECT_VALUES,
        *as_string(CORRECT_VALUES),
        (ACCOUNTS[0], True, 1),  # bool is treated like numeric (0:1)
        (ACCOUNTS[0], -1, True),  # bool is treated like numeric (0:1)
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
        (ACCOUNTS[5], UINT64_MAX+1, 1000),

        # LIMIT
        (ACCOUNTS[5], -1, -1),
        (ACCOUNTS[0], 1, 0),
        (ACCOUNTS[5], -1, 1001),
    ]
)
@run_for('testnet')
def test_get_account_history_with_incorrect_value(prepared_node, account, from_, limit):
    with pytest.raises(tt.exceptions.CommunicationError):
        prepared_node.api.wallet_bridge.get_account_history(account, from_, limit)


@pytest.mark.parametrize(
    'account, from_, limit', [
        # ACCOUNT
        (['example_array'], -1, 1000),

        # FROM
        (ACCOUNTS[5], 'example-string-argument', 1000),
        (ACCOUNTS[5], 'true', 1000),
        (ACCOUNTS[5], [-1], 1000),

        # LIMIT
        (ACCOUNTS[5], -1, 'example-string-argument'),
        (ACCOUNTS[5], -1, [1000]),
    ]
)
@run_for('testnet')
def test_get_account_history_with_incorrect_type_of_argument(prepared_node, account, from_, limit):
    wallet = tt.Wallet(attach_to=prepared_node)
    wallet.create_accounts(len(ACCOUNTS))

    with pytest.raises(tt.exceptions.CommunicationError):
        prepared_node.api.wallet_bridge.get_account_history(account, from_, limit)
