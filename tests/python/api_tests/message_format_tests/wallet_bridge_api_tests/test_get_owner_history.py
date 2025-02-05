from __future__ import annotations

import pytest
from helpy.exceptions import ErrorInResponseError

import test_tools as tt
from hive_local_tools import run_for
from hive_local_tools.api.message_format import as_string

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
def test_get_owner_history_with_correct_value(
    node: tt.InitNode | tt.RemoteNode, should_prepare: bool, account_name: bool | int | str
) -> None:
    if should_prepare:
        wallet = tt.Wallet(attach_to=node)
        create_and_update_account(wallet, account_name="alice")
    node.api.wallet_bridge.get_owner_history(account_name)


@pytest.mark.parametrize("account_name", [["alice"]])
@run_for("testnet", "mainnet_5m", "live_mainnet")
def test_get_owner_history_with_incorrect_type_of_argument(
    node: tt.InitNode | tt.RemoteNode, should_prepare: bool, account_name: list
) -> None:
    if should_prepare:
        wallet = tt.Wallet(attach_to=node)
        create_and_update_account(wallet, account_name="alice")
    with pytest.raises(ErrorInResponseError):
        node.api.wallet_bridge.get_owner_history(account_name)


def create_and_update_account(wallet: tt.Wallet, account_name: str) -> None:
    wallet.api.create_account("initminer", account_name, "{}")
    wallet.api.transfer_to_vesting("initminer", account_name, tt.Asset.Test(500))
    key = "STM8grZpsMPnH7sxbMVZHWEu1D26F3GwLW1fYnZEuwzT4Rtd57AER"
    wallet.api.update_account(account_name, "{}", key, key, key, key)
