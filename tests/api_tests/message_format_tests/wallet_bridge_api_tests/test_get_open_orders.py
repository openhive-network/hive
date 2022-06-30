import pytest

import test_tools as tt

from .local_tools import as_string, create_account_and_create_order, run_for

ACCOUNT_NAME = 't3ran13'  # Account having open orders on two mainnet nodes (5m and 64m)

CORRECT_VALUES = [
    '',
    'non-exist-acc',
    'alice',
    100,
    True,
]


@pytest.mark.parametrize(
    'account_name', [
        ACCOUNT_NAME,
        *CORRECT_VALUES,
        *as_string(CORRECT_VALUES),
    ]
)
@run_for('testnet', 'mainnet_5m', 'mainnet_64m')
def test_get_open_orders_with_correct_value(prepared_node, should_prepare, account_name):
    if should_prepare:
        wallet = tt.Wallet(attach_to=prepared_node)
        create_account_and_create_order(wallet, account_name=ACCOUNT_NAME)
    prepared_node.api.wallet_bridge.get_open_orders(account_name)


@pytest.mark.parametrize(
    'account_name', [
        [ACCOUNT_NAME]
    ]
)
@run_for('testnet', 'mainnet_5m', 'mainnet_64m')
def test_get_open_orders_with_incorrect_type_of_argument(prepared_node, should_prepare, account_name):
    if should_prepare:
        wallet = tt.Wallet(attach_to=prepared_node)
        create_account_and_create_order(wallet, account_name=ACCOUNT_NAME)

    with pytest.raises(tt.exceptions.CommunicationError):
        prepared_node.api.wallet_bridge.get_open_orders(account_name)
