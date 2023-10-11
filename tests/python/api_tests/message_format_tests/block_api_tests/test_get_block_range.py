from __future__ import annotations

import pytest

from hive_local_tools import run_for


@pytest.mark.parametrize("starting_block", [1, 5_000_000, 60_000_000])
@run_for("testnet", "mainnet_5m", "live_mainnet")
def test_get_block_range(node, starting_block):
    node.api.block.get_block_range(starting_block_num=starting_block, count=100)
