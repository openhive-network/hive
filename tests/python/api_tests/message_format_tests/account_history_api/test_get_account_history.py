from __future__ import annotations

from typing import TYPE_CHECKING

import pytest
from beekeepy.exceptions import ErrorInResponseError

from hive_local_tools import run_for

if TYPE_CHECKING:
    import test_tools as tt

ACCOUNT = "initminer"


@pytest.mark.parametrize(
    ("start", "limit"),
    [
        (0, 1),
        (1.1, 1),
        (1, 1.1),
        (1000, 1000),
        (-1, 1000),
        ("-1", 1000),
        (-1, "1000"),
        (True, 1),  # bool is treated like numeric (0:1)
        (True, True),
        (None, True),  # none is treated like numeric (0)
    ],
)
@run_for("testnet", "mainnet_5m", "live_mainnet", enable_plugins=["account_history_api"])
def test_get_account_history_with_correct_values(
    node: tt.InitNode | tt.RemoteNode, start: bool | float | None | str, limit: bool | float | str
) -> None:
    node.api.account_history.get_account_history(
        account="initminer",
        start=start,
        limit=limit,
        include_reversible=True,
    )


@pytest.mark.parametrize(
    ("account_name", "start", "limit"),
    [
        # Invalid account name
        (None, 0, 100),
        ({}, 0, 100),
        ([ACCOUNT], 0, 100),
        # Invalid start
        (ACCOUNT, "", 100),
        (ACCOUNT, "alice", 100),
        (ACCOUNT, None, 100),
        (ACCOUNT, [], 100),
        (ACCOUNT, {}, 100),
        (ACCOUNT, "1.1", 100),
        # Invalid limit
        (ACCOUNT, 1, ""),
        (ACCOUNT, 1, "alice"),
        (ACCOUNT, 1, "1.1"),
        (ACCOUNT, 0, 0),
        (ACCOUNT, 1, 100),
        (ACCOUNT, 1000, 1001),
        (ACCOUNT, 0, None),
        (ACCOUNT, 0, [100]),
        (ACCOUNT, 0, {}),
    ],
)
@run_for("testnet", "mainnet_5m", "live_mainnet", enable_plugins=["account_history_api"])
def test_get_account_history_with_incorrect_values(
    node: tt.InitNode | tt.RemoteNode,
    account_name: dict | list | None | str,
    start: dict | int | list | None | str,
    limit: dict | int | list | None | str,
) -> None:
    with pytest.raises(ErrorInResponseError):
        node.api.account_history.get_account_history(
            account=account_name,
            start=start,
            limit=limit,
            include_reversible=True,
        )
