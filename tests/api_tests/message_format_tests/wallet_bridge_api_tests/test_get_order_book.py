import pytest

import test_tools as tt

from .local_tools import as_string, create_account_and_create_order


CORRECT_VALUES = [
        0,
        10,
        500,
    ]


@pytest.mark.parametrize(
    'orders_limit', [
        * CORRECT_VALUES,
        *as_string(CORRECT_VALUES),
        True,  # bool is treated like numeric (0:1)
    ]
)
@pytest.mark.testnet
def test_get_order_book_with_correct_value(node,node5m, wallet, orders_limit):
    create_account_and_create_order(wallet, account_name='alice')
    node.api.wallet_bridge.get_order_book(orders_limit)


#TODO  Price(AssetHive(), AssetHbd()), problem -> get_order_book['bids'][order_price] -> Price(AssetHbd(), AssetHive())
@pytest.mark.remote_node_5m
def test_get_order_book_with_correct_value_5m(node5m):
    node5m.api.wallet_bridge.get_order_book(1)


@pytest.mark.remote_node_64m
def test_get_order_book_with_correct_value_64m(node64m):
    node64m.api.wallet_bridge.get_order_book(1)


@pytest.mark.parametrize(
    'orders_limit', [
        -1,
        501,
    ]
)
@pytest.mark.testnet
def test_get_order_book_with_incorrect_value(node, orders_limit):
    with pytest.raises(tt.exceptions.CommunicationError):
        node.api.wallet_bridge.get_order_book(orders_limit)


@pytest.mark.parametrize(
    'orders_limit', [
        [0],
        'incorrect_string_argument',
        'true'
    ]
)
@pytest.mark.testnet
def test_get_order_book_with_incorrect_type_of_argument(node, orders_limit):
    with pytest.raises(tt.exceptions.CommunicationError):
        node.api.wallet_bridge.get_order_book(orders_limit)
