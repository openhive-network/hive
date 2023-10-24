from __future__ import annotations

from typing import TYPE_CHECKING

import pytest

from hive_local_tools import run_for

if TYPE_CHECKING:
    import test_tools as tt


@pytest.mark.parametrize("starting_block", [1, 5_000_000, 60_000_000])
@run_for("testnet", "mainnet_5m", "live_mainnet")
def test_get_block_range(node: tt.InitNode | tt.RemoteNode, starting_block: int) -> None:
    node.api.block.get_block_range(starting_block_num=starting_block, count=100)
