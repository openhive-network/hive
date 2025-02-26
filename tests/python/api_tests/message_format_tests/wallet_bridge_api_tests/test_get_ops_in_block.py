from __future__ import annotations

from typing import TYPE_CHECKING

import pytest
from beekeepy.exceptions import ErrorInResponseError

from hive_local_tools import run_for
from hive_local_tools.api.message_format import as_string

if TYPE_CHECKING:
    import test_tools as tt

UINT64_MAX = 2**64 - 1


CORRECT_VALUES = [
    #  BLOCK NUMBER
    (0, True),
    (UINT64_MAX, True),
]


@pytest.mark.parametrize(
    ("block_number", "virtual_operation"),
    [
        *CORRECT_VALUES,
        *as_string(CORRECT_VALUES),
        (True, True),  # bool is treated like numeric (0:1)
        (UINT64_MAX, 1),  # numeric is converted to bool
        (UINT64_MAX, 2),  # numeric is converted to bool
    ],
)
@run_for("testnet", "mainnet_5m", "live_mainnet", enable_plugins=["account_history_api"])
def test_get_ops_in_block_with_correct_value(
    node: tt.InitNode | tt.RemoteNode, should_prepare: bool, block_number: bool | int, virtual_operation: bool | int
) -> None:
    if should_prepare:
        node.wait_for_block_with_number(22)  # Waiting for next witness schedule
    node.api.wallet_bridge.get_ops_in_block(block_number, virtual_operation)


@pytest.mark.parametrize(
    ("block_number", "virtual_operation"),
    [
        #  BLOCK NUMBER
        (UINT64_MAX + 1, True),
    ],
)
@run_for("testnet", "mainnet_5m", "live_mainnet")
def test_get_ops_in_block_with_incorrect_value(
    node: tt.InitNode | tt.RemoteNode, block_number: int, virtual_operation: bool
) -> None:
    with pytest.raises(ErrorInResponseError):
        node.api.wallet_bridge.get_ops_in_block(block_number, virtual_operation)


@pytest.mark.parametrize(
    ("block_number", "virtual_operation"),
    [
        #  BLOCK NUMBER
        ("incorrect_string_argument", True),
        ([0], True),
        #  VIRTUAL OPERATION
        (0, "incorrect_string_argument"),
        (0, [True]),
    ],
)
@run_for("testnet", "mainnet_5m", "live_mainnet")
def test_get_ops_in_block_with_incorrect_type_of_arguments(
    node: tt.InitNode | tt.RemoteNode, block_number: list | str, virtual_operation: bool | list | str
) -> None:
    with pytest.raises(ErrorInResponseError):
        node.api.wallet_bridge.get_ops_in_block(block_number, virtual_operation)
