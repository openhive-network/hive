from __future__ import annotations

from typing import TYPE_CHECKING

import pytest
from beekeepy.exceptions import ErrorInResponseError

from hive_local_tools import run_for

if TYPE_CHECKING:
    import test_tools as tt

UINT64_MAX = 2**64 - 1


@pytest.mark.parametrize(
    ("block_num", "only_virtual"),
    [
        # Valid block_num
        (1, True),
        ("1", True),
        (1.1, True),
        (True, True),  # block number given as bool is converted to number
        (None, True),  # block number given as None is converted to 0
        (0, True),  # returns an empty response, blocks are numbered from 1
        (UINT64_MAX, True),
        # Valid only_virtual
        # (1, True),  # tested above; virtual operations given as bool is converted to number (True:1, False:0)
        (1, False),
        (1, None),  # None is converted to 0
        (1, 0),  # virtual_operation as number is converted to bool
        (1, 1),
        (1, 2),
    ],
)
@run_for("testnet", "mainnet_5m", "live_mainnet", enable_plugins=["account_history_api"])
def test_get_ops_in_block(
    node: tt.InitNode | tt.RemoteNode,
    should_prepare: bool,
    block_num: bool | int | None | str,
    only_virtual: bool | int | None,
) -> None:
    if should_prepare:
        node.wait_for_irreversible_block()
    node.api.condenser.get_ops_in_block(block_num, only_virtual)


@pytest.mark.parametrize(
    ("block_num", "only_virtual"),
    [
        # Invalid block_num
        ("incorrect_string_argument", True),
        ("", True),
        ([1], True),
        ((1,), True),
        ({}, True),
        # Invalid only_virtual
        (1, "incorrect_string_argument"),
        (1, [True]),
        (1, (True,)),
        (1, {}),
        (1, "1"),
    ],
)
@run_for("testnet", "mainnet_5m", "live_mainnet", enable_plugins=["account_history_api"])
def test_get_ops_in_block_with_incorrect_types_of_argument(
    node: tt.InitNode | tt.RemoteNode, block_num: dict | int | str | tuple, only_virtual: dict | list | str | tuple
) -> None:
    with pytest.raises(ErrorInResponseError):
        node.api.condenser.get_ops_in_block(block_num, only_virtual)


@run_for("testnet", "mainnet_5m", "live_mainnet", enable_plugins=["account_history_api"])
def test_get_ops_in_block_with_additional_argument(node: tt.InitNode | tt.RemoteNode) -> None:
    with pytest.raises(ErrorInResponseError):
        node.api.condenser.get_ops_in_block(0, False, "additional_argument")


@run_for("testnet", "mainnet_5m", "live_mainnet", enable_plugins=["account_history_api"])
def test_get_ops_in_block_without_arguments(node: tt.InitNode | tt.RemoteNode) -> None:
    with pytest.raises(ErrorInResponseError):
        node.api.condenser.get_ops_in_block()


@run_for("testnet", "mainnet_5m", "live_mainnet", enable_plugins=["account_history_api"])
def test_get_ops_in_block_with_default_second_argument(node: tt.InitNode | tt.RemoteNode) -> None:
    node.api.condenser.get_ops_in_block(1)
