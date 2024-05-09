from __future__ import annotations

import pytest

import test_tools as tt
from hive_local_tools import run_for
from hive_local_tools.api.message_format import as_string

ACCOUNTS = ["initminer", "alice"]

CORRECT_VALUES = [
    # FROM_, TO
    (ACCOUNTS[0], ACCOUNTS[1], 100),
    (ACCOUNTS[0], "", 100),
    # LIMIT
    (ACCOUNTS[0], ACCOUNTS[1], 1),
    (ACCOUNTS[0], ACCOUNTS[1], 1000),
]


@pytest.mark.skip(reason="Unimplemented functionality in wallet")
@pytest.fixture()
def ready_node(node: tt.InitNode | tt.RemoteNode, should_prepare: bool) -> tt.InitNode | tt.RemoteNode:
    if should_prepare:
        wallet = tt.Wallet(attach_to=node)
        create_account_and_delegate_its_rc(wallet, accounts=ACCOUNTS)
    return node


@pytest.mark.skip(reason="Unimplemented functionality in wallet")
@pytest.mark.parametrize(
    ("from_", "to", "limit"),
    [
        *CORRECT_VALUES,
        *as_string(CORRECT_VALUES),
        (ACCOUNTS[0], ACCOUNTS[1], True),  # bool is treated like numeric (0:1)
    ],
)
@run_for("testnet", "mainnet_5m", "live_mainnet")
def test_list_rc_direct_delegations_with_correct_value(
    ready_node: tt.InitNode | tt.RemoteNode, from_: str, to: str, limit: bool | int | str
) -> None:
    ready_node.api.rc.list_rc_direct_delegations(start=[from_, to], limit=limit)


@pytest.mark.skip(reason="Unimplemented functionality in wallet")
@pytest.mark.parametrize(
    ("from_", "to", "limit"),
    [
        # FROM_
        ("", "", 100),
        ("non-exist-acc", "", 100),
        # TO
        (ACCOUNTS[0], "non-exist-acc", 100),
        # LIMIT
        (ACCOUNTS[0], ACCOUNTS[1], -1),
        (ACCOUNTS[0], ACCOUNTS[1], 0),
        (ACCOUNTS[0], "", 1001),
    ],
)
@run_for("testnet", "mainnet_5m", "live_mainnet")
def test_list_rc_direct_delegations_with_incorrect_value(
    ready_node: tt.InitNode | tt.RemoteNode, from_: str, to: str, limit: int
) -> None:
    with pytest.raises(tt.exceptions.CommunicationError):
        ready_node.api.rc.list_rc_direct_delegations(start=[from_, to], limit=limit)


@pytest.mark.skip(reason="Unimplemented functionality in wallet")
@run_for("testnet", "mainnet_5m", "live_mainnet")
def test_list_rc_direct_delegations_with_additional_argument(ready_node: tt.InitNode | tt.RemoteNode) -> None:
    ready_node.api.rc.list_rc_direct_delegations(
        start=[ACCOUNTS[0], ACCOUNTS[1]],
        limit=100,
        additional_argument="additional-argument",
    )


@pytest.mark.skip(reason="Unimplemented functionality in wallet")
@pytest.mark.parametrize(
    ("from_", "to", "limit"),
    [
        # FROM_
        ("100", ACCOUNTS[1], 100),
        (100, ACCOUNTS[1], 100),
        ("", ACCOUNTS[1], 100),
        ([ACCOUNTS[0]], ACCOUNTS[1], 100),
        # TO
        (ACCOUNTS[0], 100, 100),
        (ACCOUNTS[0], "100", 100),
        (ACCOUNTS[0], [ACCOUNTS[1]], 100),
        # LIMIT
        (ACCOUNTS[0], ACCOUNTS[1], "incorrect_string_argument"),
        (ACCOUNTS[0], ACCOUNTS[1], "true"),
        (ACCOUNTS[0], ACCOUNTS[1], [100]),
    ],
)
@run_for("testnet", "mainnet_5m", "live_mainnet")
def test_list_rc_direct_delegations_with_incorrect_type_of_arguments(
    ready_node: tt.InitNode | tt.RemoteNode, from_: int | list | str, to: int | list | str, limit: int | list | str
) -> None:
    with pytest.raises(tt.exceptions.CommunicationError):
        ready_node.api.rc.list_rc_direct_delegations(start=[from_, to], limit=limit)


@pytest.mark.skip(reason="Unimplemented functionality in wallet")
@run_for("testnet", "mainnet_5m", "live_mainnet")
def test_list_rc_direct_delegations_with_missing_argument(ready_node: tt.InitNode | tt.RemoteNode) -> None:
    with pytest.raises(tt.exceptions.CommunicationError):
        ready_node.api.rc.list_rc_direct_delegations(start=[ACCOUNTS[0], ACCOUNTS[1]])


def create_account_and_delegate_its_rc(wallet: tt.Wallet, accounts: list) -> None:
    wallet.create_account(accounts[1], vests=tt.Asset.Test(50))
    wallet.api.delegate_rc(accounts[0], [accounts[1]], 5)
