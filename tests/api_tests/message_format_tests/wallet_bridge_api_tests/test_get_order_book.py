import pytest

import test_tools as tt
from hive_local_tools import run_for
from hive_local_tools.api.message_format import as_string
from hive_local_tools.api.message_format.wallet_bridge_api import create_account_and_create_order

CORRECT_VALUES = [
        1,
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
@run_for("testnet")
def test_get_order_book_with_correct_value_testnet(node, orders_limit):
    wallet = tt.Wallet(attach_to=node)
    create_account_and_create_order(wallet, account_name='alice')
    node.api.wallet_bridge.get_order_book(orders_limit)


@pytest.mark.parametrize(
    'orders_limit', [
        0,
        501,
    ]
)
@run_for("testnet")
def test_get_order_book_with_incorrect_value(node, orders_limit):
    with pytest.raises(tt.exceptions.CommunicationError):
        node.api.wallet_bridge.get_order_book(orders_limit)


@pytest.mark.parametrize(
    'orders_limit', [
        [1],
        'incorrect_string_argument',
        'true'
    ]
)
@run_for("testnet")
def test_get_order_book_with_incorrect_type_of_argument(node, orders_limit):
    with pytest.raises(tt.exceptions.CommunicationError):
        node.api.wallet_bridge.get_order_book(orders_limit)
