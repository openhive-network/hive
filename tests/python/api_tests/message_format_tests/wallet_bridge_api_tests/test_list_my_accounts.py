from __future__ import annotations

import pytest
from helpy.exceptions import ErrorInResponseError

import test_tools as tt
from hive_local_tools import run_for
from hive_local_tools.api.message_format.wallet_bridge_api.constants import ACCOUNTS


@pytest.mark.parametrize(
    "accounts",
    [
        [ACCOUNTS[0]],
        ACCOUNTS,
    ],
)
@run_for("testnet", "mainnet_5m", "live_mainnet")
def test_list_my_accounts_with_correct_value(
    node: tt.InitNode | tt.RemoteNode, should_prepare: bool, accounts: list
) -> None:
    if should_prepare:
        wallet = tt.Wallet(attach_to=node)
        wallet.create_accounts(len(ACCOUNTS))

    memo_keys = []
    accounts = node.api.wallet_bridge.list_accounts(ACCOUNTS[0], 10)
    for account in accounts:
        memo_keys.append(node.api.wallet_bridge.get_account(account)["memo_key"])

    node.api.wallet_bridge.list_my_accounts(memo_keys)


@pytest.mark.parametrize(
    "account_key",
    [
        "STM6jTCfK3P5xYFJQvavAwz5M8KR6EW3TcmSesArj9LJVGAq85qor",  # non exist key with correct format
        "STM6jTCfK3P5xYFJQvavAwz5M8KR6EW3TcmSesArj9LJVGAq85qord",  # non exist key with too much signs
        "TS6jTCfK3P5xYFJQvavAwz5M8KR6EW3TcmSesArj9LJVGAq85qor",  # non exist key with less signs
        "",
        "true",
    ],
)
@run_for("testnet", "mainnet_5m", "live_mainnet")
def test_list_my_accounts_with_incorrect_values(
    node: tt.InitNode | tt.RemoteNode, should_prepare: bool, account_key: str
) -> None:
    if should_prepare:
        wallet = tt.Wallet(attach_to=node)
        wallet.create_accounts(len(ACCOUNTS))

    with pytest.raises(ErrorInResponseError):
        node.api.wallet_bridge.list_my_accounts([account_key])


@pytest.mark.parametrize("account_key", [["example-array"], 100, True, "incorrect_string_argument"])
@run_for("testnet", "mainnet_5m", "live_mainnet")
def test_list_my_accounts_with_incorrect_type_of_argument(
    node: tt.InitNode | tt.RemoteNode, account_key: bool | int | list | str
) -> None:
    with pytest.raises(ErrorInResponseError):
        node.api.wallet_bridge.list_my_accounts([account_key])


@run_for("testnet", "mainnet_5m", "live_mainnet")
def test_list_my_accounts_with_additional_argument(node: tt.InitNode | tt.RemoteNode, should_prepare: bool) -> None:
    if should_prepare:
        wallet = tt.Wallet(attach_to=node)
        wallet.create_accounts(len(ACCOUNTS))

    memo_keys = []
    accounts = node.api.wallet_bridge.list_accounts(ACCOUNTS[0], 10)
    for account in accounts:
        memo_keys.append(node.api.wallet_bridge.get_account(account)["memo_key"])

    node.api.wallet_bridge.list_my_accounts(memo_keys, "additional_argument")


@run_for("testnet", "mainnet_5m", "live_mainnet")
def test_list_my_accounts_with_missing_argument(node: tt.InitNode | tt.RemoteNode) -> None:
    with pytest.raises(ErrorInResponseError):
        node.api.wallet_bridge.list_my_accounts()
