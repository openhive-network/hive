from __future__ import annotations

from typing import TYPE_CHECKING

import pytest

from hive_local_tools import run_for

if TYPE_CHECKING:
    import test_tools as tt


@pytest.mark.parametrize("block_num", [1, 5_000_000, 60_000_000])
@run_for("testnet", "mainnet_5m", "live_mainnet")
def test_get_block(node: tt.InitNode | tt.RemoteNode, block_num: int) -> None:
    node.api.block.get_block(block_num=block_num)
