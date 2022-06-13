import pytest

from test_tools import exceptions

from .local_tools import as_string


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
@pytest.mark.testnet
def test_get_accounts_with_correct_value(node, wallet, account):
    wallet.create_accounts(len(ACCOUNTS))

    node.api.wallet_bridge.get_accounts(account)


@pytest.mark.remote_node_5m
def test_get_accounts_with_correct_value_5m(node5m):
    node5m.api.wallet_bridge.get_accounts(['gtg'])


@pytest.mark.remote_node_64m
def test_get_accounts_with_correct_value_64m(node64m):
    node64m.api.wallet_bridge.get_accounts(['gtg'])


@pytest.mark.parametrize(
    'account_key', [
        [['array-as-argument']],
        100,
        True,
        'incorrect_string_argument'
    ]
)
@pytest.mark.testnet
def test_get_accounts_with_incorrect_type_of_argument(node, account_key):
    with pytest.raises(exceptions.CommunicationError):
        node.api.wallet_bridge.get_accounts(account_key)
