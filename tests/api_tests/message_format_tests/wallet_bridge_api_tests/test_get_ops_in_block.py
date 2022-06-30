import pytest

import test_tools as tt

from .local_tools import as_string, run_for


UINT64_MAX = 2 ** 64 - 1


CORRECT_VALUES = [
    #  BLOCK NUMBER
    (0, True),
    (1, True),
    (5 * 1000 ** 2, True),  # 5 million block
    (64 * 1000 ** 2, True),  # 64 million block
    (UINT64_MAX, True),
]


@pytest.mark.parametrize(
    'block_number, virtual_operation', [
        *CORRECT_VALUES,
        *as_string(CORRECT_VALUES),
        (True, True),  # bool is treated like numeric (0:1)
        (UINT64_MAX, 1),  # numeric is converted to bool
        (UINT64_MAX, 2)  # numeric is converted to bool
    ]
)
@run_for('testnet', 'mainnet_5m', 'mainnet_64m')
def test_get_ops_in_block_with_correct_value(prepared_node, should_prepare, block_number, virtual_operation):
    if should_prepare:
        prepared_node.wait_for_block_with_number(22)
    prepared_node.api.wallet_bridge.get_ops_in_block(block_number, virtual_operation)


@pytest.mark.parametrize(
    'block_number, virtual_operation', [
        #  BLOCK NUMBER
        (UINT64_MAX + 1, True),
    ]
)
@run_for('testnet', 'mainnet_5m', 'mainnet_64m')
def test_get_ops_in_block_with_incorrect_value(prepared_node, block_number, virtual_operation):
    with pytest.raises(tt.exceptions.CommunicationError):
        prepared_node.api.wallet_bridge.get_ops_in_block(block_number, virtual_operation)


@pytest.mark.parametrize(
    'block_number, virtual_operation', [
        #  BLOCK NUMBER
        ('incorrect_string_argument', True),
        ([0], True),

        #  VIRTUAL OPERATION
        (0, 'incorrect_string_argument'),
        (0, [True]),
    ]
)
@run_for('testnet', 'mainnet_5m', 'mainnet_64m')
def test_get_ops_in_block_with_incorrect_type_of_arguments(prepared_node, block_number, virtual_operation):
    with pytest.raises(tt.exceptions.CommunicationError):
        prepared_node.api.wallet_bridge.get_ops_in_block(block_number, virtual_operation)
