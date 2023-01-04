import pytest

from hive_local_tools import run_for


@pytest.mark.parametrize("block_num", [1, 5_000_000, 60_000_000])
@run_for("testnet", "mainnet_5m", "live_mainnet")
def test_get_block(node, block_num):
    node.api.block.get_block(block_num=block_num)
