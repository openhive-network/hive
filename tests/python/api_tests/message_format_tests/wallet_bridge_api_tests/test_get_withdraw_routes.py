from __future__ import annotations

import pytest
from helpy.exceptions import ErrorInResponseError

import test_tools as tt
from hive_local_tools import run_for
from hive_local_tools.api.message_format import as_string

CORRECT_VALUES = [
    #  ACCOUNT NAME
    ("alice", "all"),
    ("non-exist-acc", "all"),
    ("", "all"),
    (100, "all"),
    (True, "all"),
    #  WITHDRAW ROUTE TYPE
    ("alice", "all"),
    ("alice", "incoming"),
    ("alice", "outgoing"),
    ("alice", 0),
    ("alice", 2),
]


@pytest.mark.parametrize(
    ("account_name", "withdraw_route_type"),
    [
        *CORRECT_VALUES,
        *as_string(CORRECT_VALUES),
        ("alice", False),
        ("alice", True),
    ],
)
@run_for("testnet", "mainnet_5m", "live_mainnet")
def test_get_withdraw_routes_with_correct_value(
    node: tt.InitNode | tt.RemoteNode,
    should_prepare: bool,
    account_name: bool | int | str,
    withdraw_route_type: bool | int | str,
) -> None:
    if should_prepare:
        wallet = tt.Wallet(attach_to=node)
        create_accounts_and_set_withdraw_vesting_route_between_them(wallet, from_="alice", to="bob")
    node.api.wallet_bridge.get_withdraw_routes(account_name, withdraw_route_type)


@pytest.mark.parametrize(
    ("account_name", "withdraw_route_type"),
    [
        #  WITHDRAW ROUTE TYPE
        ("alice", "non-exist-argument"),
        ("alice", "true"),
        ("alice", ""),
        ("alice", "100"),
        ("alice", "-1"),
        ("alice", "3"),
    ],
)
@run_for("testnet", "mainnet_5m", "live_mainnet")
def test_get_withdraw_routes_with_incorrect_value(
    node: tt.InitNode | tt.RemoteNode, should_prepare: bool, account_name: str, withdraw_route_type: str
) -> None:
    if should_prepare:
        wallet = tt.Wallet(attach_to=node)
        create_accounts_and_set_withdraw_vesting_route_between_them(wallet, from_="alice", to="bob")

    with pytest.raises(ErrorInResponseError):
        node.api.wallet_bridge.get_withdraw_routes(account_name, withdraw_route_type)


@pytest.mark.parametrize(
    ("account_name", "withdraw_route_type"),
    [
        #  ACCOUNT NAME
        (["alice"], "all"),
        #  WITHDRAW ROUTE TYPE
        ("alice", ["all"]),
        ("alice", 100),
    ],
)
@run_for("testnet", "mainnet_5m", "live_mainnet")
def test_get_withdraw_routes_with_incorrect_type_of_arguments(
    node: tt.InitNode | tt.RemoteNode,
    should_prepare: bool,
    account_name: list | str,
    withdraw_route_type: int | list | str,
) -> None:
    if should_prepare:
        wallet = tt.Wallet(attach_to=node)
        create_accounts_and_set_withdraw_vesting_route_between_them(wallet, from_="alice", to="bob")

    with pytest.raises(ErrorInResponseError):
        node.api.wallet_bridge.get_withdraw_routes(account_name, withdraw_route_type)


@run_for("testnet", "mainnet_5m", "live_mainnet")
def test_get_withdraw_routes_with_additional_argument(node: tt.InitNode | tt.RemoteNode, should_prepare: bool) -> None:
    if should_prepare:
        wallet = tt.Wallet(attach_to=node)
        create_accounts_and_set_withdraw_vesting_route_between_them(wallet, from_="alice", to="bob")

    node.api.wallet_bridge.get_withdraw_routes("alice", "all", "additional_argument")


@run_for("testnet", "mainnet_5m", "live_mainnet")
def test_get_withdraw_routes_with_missing_argument(node: tt.InitNode | tt.RemoteNode, should_prepare: bool) -> None:
    if should_prepare:
        wallet = tt.Wallet(attach_to=node)
        create_accounts_and_set_withdraw_vesting_route_between_them(wallet, from_="alice", to="bob")

    with pytest.raises(ErrorInResponseError):
        node.api.wallet_bridge.get_withdraw_routes("alice")


def create_accounts_and_set_withdraw_vesting_route_between_them(wallet: tt.Wallet, from_: str, to: str) -> None:
    with wallet.in_single_transaction():
        wallet.api.create_account("initminer", from_, "{}")
        wallet.api.create_account("initminer", to, "{}")

    wallet.api.transfer_to_vesting("initminer", from_, tt.Asset.Test(500))
    wallet.api.set_withdraw_vesting_route(from_, to, 30, True)
