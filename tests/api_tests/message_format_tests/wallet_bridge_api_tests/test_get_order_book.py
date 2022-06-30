import pytest

import test_tools as tt

from .local_tools import as_string, create_account_and_create_order, run_for

ACCOUNT_NAME = 'alice'

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
@run_for('testnet', 'mainnet_5m', 'mainnet_64m')
def test_get_order_book_with_correct_value(prepared_node, should_prepare, orders_limit):
    if should_prepare:
        wallet = tt.Wallet(attach_to=prepared_node)
        create_account_and_create_order(wallet, account_name=ACCOUNT_NAME)
    prepared_node.api.wallet_bridge.get_order_book(orders_limit)


@pytest.mark.parametrize(
    'orders_limit', [
        -1,
        501,
    ]
)
@run_for('testnet', 'mainnet_5m', 'mainnet_64m')
def test_get_order_book_with_incorrect_value(prepared_node, orders_limit):
    with pytest.raises(tt.exceptions.CommunicationError):
        prepared_node.api.wallet_bridge.get_order_book(orders_limit)


@pytest.mark.parametrize(
    'orders_limit', [
        [0],
        'incorrect_string_argument',
        'true'
    ]
)
@run_for('testnet', 'mainnet_5m', 'mainnet_64m')
def test_get_order_book_with_incorrect_type_of_argument(prepared_node, orders_limit):
    with pytest.raises(tt.exceptions.CommunicationError):
        prepared_node.api.wallet_bridge.get_order_book(orders_limit)
