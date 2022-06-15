import pytest

import test_tools as tt

from .local_tools import as_string, run_for


TESTNET_ACCOUNTS = [f'account-{i}' for i in range(10)]

CORRECT_VALUES = [
        # LOWERBOUND ACCOUNT
        (TESTNET_ACCOUNTS[0], 100),
        ('non-exist-acc', 100),
        ('', 100),
        ('true', 100),
        (True, 100),
        (100, 100),

        # LIMIT
        (TESTNET_ACCOUNTS[0], 0),
        (TESTNET_ACCOUNTS[0], 1000),
]


@pytest.mark.parametrize(
    'lowerbound_account, limit', [
        *CORRECT_VALUES,
        *as_string(CORRECT_VALUES),
        (TESTNET_ACCOUNTS[0], True),  # bool is treated like numeric (0:1)
    ]
)
@run_for('testnet', 'mainnet_5m', 'mainnet_64m')
def test_list_accounts_with_correct_values(prepared_node, should_prepare, lowerbound_account, limit):
    if should_prepare:
        wallet = tt.Wallet(attach_to=prepared_node)
        wallet.create_accounts(len(TESTNET_ACCOUNTS))

    result = prepared_node.api.wallet_bridge.list_accounts(lowerbound_account, limit)
    if len(result) != 0:
        assert len(result) >= 1


@pytest.mark.parametrize(
    'lowerbound_account, limit', [
        # LIMIT
        (TESTNET_ACCOUNTS[0], -1),
        (TESTNET_ACCOUNTS[0], 1001),
    ]
)
@run_for('testnet', 'mainnet_5m', 'mainnet_64m')
def test_list_accounts_with_incorrect_values(prepared_node, lowerbound_account, should_prepare, limit):
    if should_prepare:
        wallet = tt.Wallet(attach_to=prepared_node)
        wallet.create_accounts(len(TESTNET_ACCOUNTS))

    with pytest.raises(tt.exceptions.CommunicationError):
        prepared_node.api.wallet_bridge.list_accounts(lowerbound_account, limit)


@pytest.mark.parametrize(
    'lowerbound_account, limit', [
        # LOWERBOUND ACCOUNT
        (['example-array'], 100),

        # LIMIT
        (TESTNET_ACCOUNTS[0], 'incorrect_string_argument'),
        (TESTNET_ACCOUNTS[0], [100]),
    ]
)
@run_for('testnet', 'mainnet_5m', 'mainnet_64m')
def test_list_accounts_with_incorrect_type_of_argument(prepared_node, lowerbound_account, limit):
    with pytest.raises(tt.exceptions.CommunicationError):
        prepared_node.api.wallet_bridge.list_accounts(lowerbound_account, limit)
