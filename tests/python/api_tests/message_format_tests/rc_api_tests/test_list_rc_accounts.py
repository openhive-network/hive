from __future__ import annotations

import pytest
from helpy.exceptions import ErrorInResponseError

import test_tools as tt
from hive_local_tools import run_for
from hive_local_tools.api.message_format import as_string

ACCOUNT = "alice"

CORRECT_VALUES = [
    # RC ACCOUNT
    (ACCOUNT, 100),
    ("non-exist-acc", 100),
    ("true", 100),
    ("", 100),
    (100, 100),
    (True, 100),
    # LIMIT
    (ACCOUNT, 1),
    (ACCOUNT, 1000),
]


@pytest.fixture()
def ready_node(node: tt.InitNode | tt.RemoteNode, should_prepare: bool) -> tt.InitNode | tt.RemoteNode:
    if should_prepare:
        wallet = tt.Wallet(attach_to=node)
        wallet.create_account(ACCOUNT)
    return node


@pytest.mark.parametrize(
    ("rc_account", "limit"),
    [
        *CORRECT_VALUES,
        *as_string(CORRECT_VALUES),
        (ACCOUNT, True),  # bool is treated like numeric (0:1)
    ],
)
@run_for("testnet", "mainnet_5m", "live_mainnet")
def test_list_rc_accounts_with_correct_values(
    ready_node: tt.InitNode | tt.RemoteNode, rc_account: bool | int | str, limit: int
) -> None:
    ready_node.api.rc.list_rc_accounts(start=rc_account, limit=limit)


@pytest.mark.parametrize(
    ("rc_account", "limit"),
    [
        # LIMIT
        (ACCOUNT, -1),
        (ACCOUNT, 0),
        (ACCOUNT, 1001),
    ],
)
@run_for("testnet", "mainnet_5m", "live_mainnet")
def test_list_rc_accounts_with_incorrect_values(
    ready_node: tt.InitNode | tt.RemoteNode, rc_account: str, limit: int
) -> None:
    with pytest.raises(ErrorInResponseError):
        ready_node.api.rc.list_rc_accounts(start=rc_account, limit=limit)


@pytest.mark.parametrize(
    ("rc_account", "limit"),
    [
        # WITNESS
        (["example-array"], 100),
        # LIMIT
        (ACCOUNT, "incorrect_string_argument"),
        (ACCOUNT, [100]),
        (ACCOUNT, "true"),
    ],
)
@run_for("testnet", "mainnet_5m", "live_mainnet")
def test_list_rc_accounts_with_incorrect_type_of_arguments(
    ready_node: tt.InitNode | tt.RemoteNode, rc_account: list | str, limit: int | list | str
) -> None:
    with pytest.raises(ErrorInResponseError):
        ready_node.api.rc.list_rc_accounts(start=rc_account, limit=limit)


@run_for("testnet", "mainnet_5m", "live_mainnet")
def test_list_rc_account_with_additional_argument(ready_node: tt.InitNode | tt.RemoteNode) -> None:
    ready_node.api.rc.list_rc_accounts(start=ACCOUNT, limit=100, additional_argument="additional_argument")


@run_for("testnet", "mainnet_5m", "live_mainnet")
def test_list_rc_account_with_missing_argument(ready_node: tt.InitNode | tt.RemoteNode) -> None:
    with pytest.raises(ErrorInResponseError):
        ready_node.api.rc.list_rc_accounts(start=ACCOUNT)
