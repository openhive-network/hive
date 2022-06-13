import pytest

from test_tools import exceptions

from .local_tools import as_string, create_account_and_create_order


CORRECT_VALUES = [
    '',
    'non-exist-acc',
    'alice',
    100,
    True,
]


@pytest.mark.parametrize(
    'account_name', [
        *CORRECT_VALUES,
        *as_string(CORRECT_VALUES),
    ]
)
@pytest.mark.testnet
def test_get_open_orders_with_correct_value(node, wallet, account_name):
    create_account_and_create_order(wallet, account_name='alice')
    r = node.api.wallet_bridge.get_open_orders(account_name)
    print()


@pytest.mark.remote_node_5m
def test_get_open_orders_with_correct_value_5m(node5m):
    r = node5m.api.wallet_bridge.get_open_orders('gtg')
    assert r == []


@pytest.mark.remote_node_64m
def test_get_open_orders_with_correct_value_64m(node64m):
    node64m.api.wallet_bridge.get_open_orders('gtg')


@pytest.mark.parametrize(
    'account_name', [
        ['alice']
    ]
)
@pytest.mark.testnet
def test_get_open_orders_with_incorrect_type_of_argument(node, wallet, account_name):
    create_account_and_create_order(wallet, account_name='alice')

    with pytest.raises(exceptions.CommunicationError):
        node.api.wallet_bridge.get_open_orders(account_name)
