import pytest

from ..local_tools import run_for


@pytest.mark.parametrize('block_num', [1, 5_000_000, 60_000_0000])
@run_for('testnet', 'mainnet_5m', 'mainnet_64m')
def test_get_block_header(prepared_node, block_num):
    prepared_node.api.block.get_block_header(block_num=block_num)
