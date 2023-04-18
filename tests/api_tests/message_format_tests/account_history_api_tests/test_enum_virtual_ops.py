import pytest

import test_tools as tt

from hive_local_tools import run_for


@pytest.mark.parametrize(
    'block_range_begin, block_range_end, group_by_block', [
        # Valid block ranges
        (0, 100, True),  # returns blocks from the first available block. Blocks are numbered from 1.
        ('0', 100, True),
        (1, 100, True),
        (1, 2001, True),
        (True, 100, True),
        (None, 100, True),
        (1, '100', True),
        ('1', 100, True),
        ('1', '100', True),
        (1.1, '100', True),

        # Valid group_by_block values
        (1, 100, True),
        (1, 100, False),
        (1, 100, 'true'),
        (1, 100, 'false'),
        (1, 100, 1),
        (1, 100, 0),
    ]
)
@run_for('testnet', 'mainnet_5m', 'live_mainnet', enable_plugins=['account_history_api'])
def test_enum_virtual_ops_with_correct_values(node, block_range_begin, block_range_end, group_by_block):
    node.api.account_history.enum_virtual_ops(
        block_range_begin=block_range_begin,
        block_range_end=block_range_end,
        group_by_block=group_by_block,
        include_reversible=True,
    )


@pytest.mark.parametrize(
    'block_range_begin, block_range_end, group_by_block', [
        # Invalid block ranges
        (1, 1, True),
        (100, 1, True),
        (1, 2002, True),  # Maximum limit (2000 results) has been exceeded
        (-1, 100, True),
        (-1, -100, True),
        (1, '', True),
        ('', 100, True),
        (1, 'alice', True),
        ('alice', 100, True),
        ('1.1', 100, True),
        (1.1, '100.1', True),
        (['alice'], 100, True),
        ({}, 100, True),

        # Invalid group_by_block values
        (0, 100, ''),
        (0, 100, []),
        (0, 100, {}),
        (0, 100, '1'),
        (0, 100, '0'),
    ]
)
@run_for('testnet', 'mainnet_5m', 'live_mainnet', enable_plugins=['account_history_api'])
def test_enum_virtual_ops_with_incorrect_values(node, block_range_begin, block_range_end, group_by_block):
    with pytest.raises(tt.exceptions.CommunicationError):
        node.api.account_history.enum_virtual_ops(
            block_range_begin=block_range_begin,
            block_range_end=block_range_end,
            group_by_block=group_by_block,
            include_reversible=True,
        )
