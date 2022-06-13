import pytest

import test_tools as tt

from .local_tools import as_string


ACCOUNTS = [f'account-{i}' for i in range(10)]

CORRECT_VALUES = [
    ACCOUNTS[0],
    'non-exist-acc',
    '',
    100,
    True,
]


@pytest.mark.parametrize(
    'account', [
        *CORRECT_VALUES,
        *as_string(CORRECT_VALUES),
    ]
)
@pytest.mark.testnet
def test_get_account_correct_value(node, wallet, account):
    wallet.create_accounts(len(ACCOUNTS))
    node.api.wallet_bridge.get_account(account)


@pytest.mark.remote_node_5m
def test_get_account_correct_value_64m(node5m):
    response = node5m.api.wallet_bridge.get_account('gtg')


@pytest.mark.remote_node_64m
def test_get_account_correct_value_64(node64m):
    response = node64m.api.wallet_bridge.get_account('gtg')


@pytest.mark.parametrize(
    'account', [
        ['example_array']
    ]
)
@pytest.mark.testnet
def test_get_account_incorrect_type_of_argument(node, account):
    with pytest.raises(tt.exceptions.CommunicationError):
        node.api.wallet_bridge.get_account(account)
