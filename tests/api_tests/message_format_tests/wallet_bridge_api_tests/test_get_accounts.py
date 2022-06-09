import pytest

import test_tools as tt

from .local_tools import as_string, run_for


ACCOUNTS = [f'account-{i}' for i in range(10)]

CORRECT_VALUES = [
    ACCOUNTS,
    [ACCOUNTS[0]],
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
def test_get_accounts_with_correct_value(prepared_node, account):
    wallet = tt.Wallet(attach_to=prepared_node)
    wallet.create_accounts(len(ACCOUNTS))

    prepared_node.api.wallet_bridge.get_accounts(account)


@pytest.mark.parametrize(
    'account_key', [
        [['array-as-argument']],
        100,
        True,
        'incorrect_string_argument'
    ]
)
@run_for('testnet')
def test_get_accounts_with_incorrect_type_of_argument(prepared_node, account_key):
    with pytest.raises(tt.exceptions.CommunicationError):
        prepared_node.api.wallet_bridge.get_accounts(account_key)
