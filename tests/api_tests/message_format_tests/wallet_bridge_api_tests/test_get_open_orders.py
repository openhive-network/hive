import pytest

import test_tools as tt

from .local_tools import as_string, create_account_and_create_order, run_for


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
@run_for('testnet')
def test_get_open_orders_with_correct_value(prepared_node, account_name):
    wallet = tt.Wallet(attach_to=prepared_node)
    create_account_and_create_order(wallet, account_name='alice')
    prepared_node.api.wallet_bridge.get_open_orders(account_name)


@pytest.mark.parametrize(
    'account_name', [
        ['alice']
    ]
)
@run_for('testnet')
def test_get_open_orders_with_incorrect_type_of_argument(prepared_node, account_name):
    wallet = tt.Wallet(attach_to=prepared_node)
    create_account_and_create_order(wallet, account_name='alice')

    with pytest.raises(tt.exceptions.CommunicationError):
        prepared_node.api.wallet_bridge.get_open_orders(account_name)
