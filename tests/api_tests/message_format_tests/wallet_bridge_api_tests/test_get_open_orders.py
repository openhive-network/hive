import pytest

import test_tools as tt
from hive_local_tools.api.message_format import as_string
from hive_local_tools.api.message_format.wallet_bridge_api import create_account_and_create_order

CORRECT_VALUES = [
    "",
    "non-exist-acc",
    "alice",
    100,
    True,
]


@pytest.mark.parametrize(
    "account_name",
    [
        *CORRECT_VALUES,
        *as_string(CORRECT_VALUES),
    ],
)
def test_get_open_orders_with_correct_value(node, wallet, account_name):
    create_account_and_create_order(wallet, account_name="alice")
    node.api.wallet_bridge.get_open_orders(account_name)


@pytest.mark.parametrize("account_name", [["alice"]])
def test_get_open_orders_with_incorrect_type_of_argument(node, wallet, account_name):
    create_account_and_create_order(wallet, account_name="alice")

    with pytest.raises(tt.exceptions.CommunicationError):
        node.api.wallet_bridge.get_open_orders(account_name)
