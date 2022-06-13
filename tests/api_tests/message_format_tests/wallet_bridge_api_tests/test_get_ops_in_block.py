import pytest

import test_tools as tt

from .local_tools import as_string


UINT64_MAX = 2 ** 64 - 1


CORRECT_VALUES = [
    #  BLOCK NUMBER
    (0, True),
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
@pytest.mark.testnet
def test_get_ops_in_block_with_correct_value(node, block_number, virtual_operation):
    node.wait_for_block_with_number(22)  # Waiting for next witness schedule
    node.api.wallet_bridge.get_ops_in_block(block_number, virtual_operation)


@pytest.mark.reamote_node_5m
def test_get_ops_in_block_with_correct_value_5m(node5m):
    node5m.api.wallet_bridge.get_ops_in_block(1, False)


@pytest.mark.reamote_node_64m
def test_get_ops_in_block_with_correct_value_64m(node64m):
    node64m.api.wallet_bridge.get_ops_in_block(1, False)


@pytest.mark.parametrize(
    'block_number, virtual_operation', [
        #  BLOCK NUMBER
        (UINT64_MAX + 1, True),
    ]
)
@pytest.mark.testnet
def test_get_ops_in_block_with_incorrect_value(node, block_number, virtual_operation):
    with pytest.raises(tt.exceptions.CommunicationError):
        node.api.wallet_bridge.get_ops_in_block(block_number, virtual_operation)


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
@pytest.mark.testnet
def test_get_ops_in_block_with_incorrect_type_of_arguments(node, block_number, virtual_operation):
    with pytest.raises(tt.exceptions.CommunicationError):
        node.api.wallet_bridge.get_ops_in_block(block_number, virtual_operation)
