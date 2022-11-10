import pytest

import test_tools as tt

from ..local_tools import as_string

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
def test_get_accounts_with_correct_value(node, wallet, account):
    wallet.create_accounts(len(ACCOUNTS))

    node.api.wallet_bridge.get_accounts(account)


@pytest.mark.parametrize(
    'account_key', [
        [['array-as-argument']],
        100,
        True,
        'incorrect_string_argument'
    ]
)
def test_get_accounts_with_incorrect_type_of_argument(node, account_key):
    with pytest.raises(tt.exceptions.CommunicationError):
        node.api.wallet_bridge.get_accounts(account_key)
