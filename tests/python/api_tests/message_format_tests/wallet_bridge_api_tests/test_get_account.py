from __future__ import annotations

import pytest
from beekeepy.exceptions import ErrorInResponseError

import test_tools as tt
from hive_local_tools import run_for
from hive_local_tools.api.message_format import as_string
from hive_local_tools.api.message_format.wallet_bridge_api.constants import ACCOUNTS, MAINNET_ACCOUNT

CORRECT_VALUES = [
    ACCOUNTS[0],
    MAINNET_ACCOUNT,
    "non-exist-acc",
    "",
    100,
    True,
]


@pytest.mark.parametrize(
    "account",
    [
        *CORRECT_VALUES,
        *as_string(CORRECT_VALUES),
    ],
)
@run_for("testnet", "mainnet_5m", "live_mainnet")
def test_get_account_with_correct_value(
    node: tt.InitNode | tt.RemoteNode, should_prepare: bool, account: bool | int | str
) -> None:
    if should_prepare:
        wallet = tt.Wallet(attach_to=node)
        wallet.create_accounts(len(ACCOUNTS))
    node.api.wallet_bridge.get_account(account)


@pytest.mark.parametrize("account", [["example_array"]])
@run_for("testnet", "mainnet_5m", "live_mainnet")
def test_get_account_incorrect_type_of_argument(node: tt.InitNode | tt.RemoteNode, account: list) -> None:
    with pytest.raises(ErrorInResponseError):
        node.api.wallet_bridge.get_account(account)
