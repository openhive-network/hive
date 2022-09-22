import pytest

from ..local_tools import run_for


@pytest.mark.parametrize('block_num', [1, 5_000_000, 60_000_000])
@run_for('testnet', 'mainnet_5m', 'mainnet_64m')
def test_get_block(prepared_node, block_num):
    prepared_node.api.block.get_block(block_num=block_num)
