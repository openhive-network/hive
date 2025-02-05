from __future__ import annotations

from typing import TYPE_CHECKING

import pytest
from helpy.exceptions import ErrorInResponseError

from hive_local_tools import run_for
from hive_local_tools.api.message_format import as_string

if TYPE_CHECKING:
    import test_tools as tt

UINT64_MAX = 2**64 - 1

CORRECT_VALUES = [
    -1,
    0,
    1,
    10,
    UINT64_MAX,
]


@pytest.mark.parametrize(
    "block_number",
    [
        *CORRECT_VALUES,
        *as_string(CORRECT_VALUES),
        True,
    ],
)
@run_for("testnet", "mainnet_5m", "live_mainnet")
def test_get_block_with_correct_value(
    node: tt.InitNode | tt.RemoteNode, should_prepare: bool, block_number: bool | int | str
) -> None:
    if should_prepare and int(block_number) < 2:  # To get existing block for block ids: 0 and 1.
        node.wait_for_block_with_number(2)

    node.api.wallet_bridge.get_block(block_number)


@pytest.mark.parametrize(
    "block_number",
    [
        UINT64_MAX + 1,
    ],
)
@run_for("testnet", "mainnet_5m", "live_mainnet")
def test_get_block_with_incorrect_value(node: tt.InitNode | tt.RemoteNode, block_number: int) -> None:
    with pytest.raises(ErrorInResponseError):
        node.api.wallet_bridge.get_block(block_number)


@pytest.mark.parametrize("block_number", [[0], "incorrect_string_argument", "true"])
@run_for("testnet", "mainnet_5m", "live_mainnet")
def test_get_block_with_incorrect_type_of_argument(node: tt.InitNode | tt.RemoteNode, block_number: list | str) -> None:
    with pytest.raises(ErrorInResponseError):
        node.api.wallet_bridge.get_block(block_number)
