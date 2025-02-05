from __future__ import annotations

import pytest
from helpy.exceptions import ErrorInResponseError

import test_tools as tt
from hive_local_tools import run_for
from hive_local_tools.api.message_format import as_string
from hive_local_tools.api.message_format.wallet_bridge_api.constants import ACCOUNTS, MAINNET_ACCOUNT

CORRECT_VALUES = [
    ACCOUNTS,
    [MAINNET_ACCOUNT],
    [ACCOUNTS[0]],
    ["non-exist-acc"],
    [""],
    [100],
    [True],
]


@pytest.mark.parametrize(
    "account",
    [
        *CORRECT_VALUES,
        *as_string(CORRECT_VALUES),
    ],
)
@run_for("testnet", "mainnet_5m", "live_mainnet")
def test_get_accounts_with_correct_value(
    node: tt.InitNode | tt.RemoteNode, should_prepare: bool, account: list
) -> None:
    if should_prepare:
        wallet = tt.Wallet(attach_to=node)
        wallet.create_accounts(len(ACCOUNTS))

    node.api.wallet_bridge.get_accounts(account)


@pytest.mark.parametrize("account_key", [[["array-as-argument"]], 100, True, "incorrect_string_argument"])
@run_for("testnet", "mainnet_5m", "live_mainnet")
def test_get_accounts_with_incorrect_type_of_argument(
    node: tt.InitNode | tt.RemoteNode, account_key: bool | int | list
) -> None:
    with pytest.raises(ErrorInResponseError):
        node.api.wallet_bridge.get_accounts(account_key)
