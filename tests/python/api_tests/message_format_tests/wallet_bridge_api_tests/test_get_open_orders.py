from __future__ import annotations

import pytest

import test_tools as tt
from hive_local_tools import run_for
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
@run_for("testnet", "mainnet_5m", "live_mainnet")
def test_get_open_orders_with_correct_value(node, should_prepare, account_name):
    if should_prepare:
        wallet = tt.Wallet(attach_to=node)
        create_account_and_create_order(wallet, account_name="alice")
    node.api.wallet_bridge.get_open_orders(account_name)


@pytest.mark.parametrize("account_name", [["alice"]])
@run_for("testnet", "mainnet_5m", "live_mainnet")
def test_get_open_orders_with_incorrect_type_of_argument(node, should_prepare, account_name):
    if should_prepare:
        wallet = tt.Wallet(attach_to=node)
        create_account_and_create_order(wallet, account_name="alice")

    with pytest.raises(tt.exceptions.CommunicationError):
        node.api.wallet_bridge.get_open_orders(account_name)
