import pytest

import test_tools as tt

from ..local_tools import run_for


@pytest.mark.parametrize(
    'block_range_begin, block_range_end', [
        (1, 100),
        (1, 2001),
        (True, 100),
        (1, '100'),
        ('1', 100),
        ('1', '100'),
    ]
)
@run_for('testnet', 'mainnet_5m', 'mainnet_64m')
def test_enum_virtual_ops(prepared_node, block_range_begin, block_range_end):
    prepared_node.api.account_history.enum_virtual_ops(
        block_range_begin=block_range_begin,
        block_range_end=block_range_end,
        include_reversible=True
    )


@pytest.mark.parametrize(
    'block_range_begin, block_range_end ', [
        (1, 1),
        (100, 1),
        (1, 2002),  # Maximum limit is 2000 results
        (-1, 100),
        (-1, -100),
        (1, ''),
        ('', 100),
        (1, 'alice'),
        ('alice', 100),
    ]
)
@run_for('testnet', 'mainnet_5m', 'mainnet_64m')
def test_enum_virtual_ops_with_incorrect_values(prepared_node, block_range_begin, block_range_end):
    with pytest.raises(tt.exceptions.CommunicationError):
        prepared_node.api.account_history.enum_virtual_ops(
            block_range_begin=block_range_begin,
            block_range_end=block_range_end,
            include_reversible=True
        )
