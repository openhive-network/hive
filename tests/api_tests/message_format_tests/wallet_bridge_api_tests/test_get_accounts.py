import pytest

import test_tools as tt

from .local_tools import as_string, run_for


TESTNET_ACCOUNTS = [f'account-{i}' for i in range(10)]
MAINNET_ACCOUNT = 'gtg'

CORRECT_VALUES = [
    TESTNET_ACCOUNTS,
    [TESTNET_ACCOUNTS[0]],
    ['non-exist-acc'],
    [''],
    [100],
    [True],
]


@pytest.mark.parametrize(
    'account', [
        *CORRECT_VALUES,
        *as_string(CORRECT_VALUES),
    ]
)
@run_for('testnet')
def test_get_accounts_with_correct_value_in_testnet(prepared_node, account):
    wallet = tt.Wallet(attach_to=prepared_node)
    wallet.create_accounts(len(TESTNET_ACCOUNTS))

    prepared_node.api.wallet_bridge.get_accounts(account)


@run_for('mainnet_5m', 'mainnet_64m')
def test_get_accounts_with_correct_value_in_mainnet(prepared_node):
    prepared_node.api.wallet_bridge.get_accounts([MAINNET_ACCOUNT])


@pytest.mark.parametrize(
    'account_key', [
        [['array-as-argument']],
        100,
        True,
        'incorrect_string_argument'
    ]
)
@run_for('testnet', 'mainnet_5m', 'mainnet_64m')
def test_get_accounts_with_incorrect_type_of_argument(prepared_node, account_key):
    with pytest.raises(tt.exceptions.CommunicationError):
        prepared_node.api.wallet_bridge.get_accounts(account_key)
