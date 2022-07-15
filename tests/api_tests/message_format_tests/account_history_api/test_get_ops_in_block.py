import pytest

import test_tools as tt

from ..local_tools import run_for

UINT64_MAX = 2 ** 64 - 1


@pytest.mark.parametrize(
    'block_num, virtual_operation', [
        (1, True),
        ('1', True),
        (1.1, True),
        (True, True),  # bool is treated like numeric (0:1)
        (None, True),
        (0, True),  # returns an empty response, blocks are numbered from 1
        (UINT64_MAX - 1, True),
        (UINT64_MAX - 1, 1),  # numeric is converted to bool
        (UINT64_MAX - 1, 2),  # numeric is converted to bool
    ]
)
@run_for('testnet', 'mainnet_5m', 'mainnet_64m')
def test_get_ops_in_block_with_correct_values(prepared_node, block_num, virtual_operation):
    prepared_node.api.account_history.get_ops_in_block(
        block_num=block_num,
        only_virtual=virtual_operation,
        include_reversible=True
    )


@pytest.mark.parametrize(
    'block_num, virtual_operation', [
        #  BLOCK NUMBER
        ('incorrect_string_argument', True),
        ('', True),
        ([0], True),
        ({}, True),

        #  VIRTUAL OPERATION
        (0, 'incorrect_string_argument'),
        (0, [True]),
        (0, {}),
        (1, '1'),

        #  PARAMETER RANGE EXCEEDED
        (-1, True),
        (UINT64_MAX + 1, True),
    ]
)
@run_for('testnet', 'mainnet_5m', 'mainnet_64m')
def test_get_ops_in_block_with_incorrect_type_of_arguments(prepared_node, block_num, virtual_operation):
    with pytest.raises(tt.exceptions.CommunicationError):
        prepared_node.api.account_history.get_ops_in_block(
            block_num=block_num,
            only_virtual=virtual_operation,
            include_reversible=True
        )
