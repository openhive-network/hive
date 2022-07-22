import pytest

import test_tools as tt

from ..local_tools import run_for

UINT32_MAX = 2 ** 32 - 1


@pytest.mark.parametrize(
    'block_num, virtual_operation, include_reversible', [
        # Valid block num
        (1, True, False),
        ('1', True, False),
        (1.1, True, False),
        (True, True, False),  # bool is treated like numeric (0:1)
        (None, True, False),
        (0, True, False),  # returns an empty response, blocks are numbered from 1

        # Valid virtual operations
        (UINT32_MAX, True, False),
        (UINT32_MAX, False, False),
        (UINT32_MAX, 0, False),  # numeric in virtual operations is converted to bool
        (UINT32_MAX, 1, False),
        (UINT32_MAX, 2, False),

        # Valid include reversible
        (1, True, True),
        (1, True, False),
        (1, True, 'true'),
        (1, True, 'false'),
        (UINT32_MAX, True, True),
        (UINT32_MAX, True, False),
        (UINT32_MAX, True, 'true'),
        (UINT32_MAX, True, 'false'),
        (UINT32_MAX, True, 0),
        (UINT32_MAX, True, 1),
    ]
)
@run_for('testnet', 'mainnet_5m', 'mainnet_64m')
def test_get_ops_in_block_with_correct_values(prepared_node, block_num, virtual_operation, include_reversible):
    prepared_node.api.account_history.get_ops_in_block(
        block_num=block_num,
        only_virtual=virtual_operation,
        include_reversible=include_reversible,
    )


@pytest.mark.parametrize(
    'block_num, virtual_operation, include_reversible', [
        # Invalid block number
        ('incorrect_string_argument', True, False),
        ('', True, False),
        ([0], True, False),
        ({}, True, False),

        # Invalid virtual operation
        (0, 'incorrect_string_argument', False),
        (0, [True], False),
        (0, {}, False),
        (1, '1', False),

        # Invalid parameter range exceeded
        (-1, True, False),
        (UINT32_MAX + 1, True, False),

        # Invalid include reversible
        (1, True, ''),
        (1, True, 'TRUE'),
        (1, True, 'FALSE'),
        (1, True, []),
        (1, True, {}),
    ]
)
@run_for('testnet', 'mainnet_5m', 'mainnet_64m')
def test_get_ops_in_block_with_incorrect_type_of_arguments(prepared_node, block_num, virtual_operation, include_reversible):
    with pytest.raises(tt.exceptions.CommunicationError):
        prepared_node.api.account_history.get_ops_in_block(
            block_num=block_num,
            only_virtual=virtual_operation,
            include_reversible=include_reversible,
        )
