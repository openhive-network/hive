import pytest

import test_tools as tt

from ....local_tools import run_for

UINT64_MAX = 2 ** 64 - 1


@pytest.mark.parametrize(
    "block_num, only_virtual", [
        # Valid block_num
        (1, True),
        ("1", True),
        (1.1, True),
        (True, True),  # block number given as bool is converted to number
        (None, True),  # block number given as None is converted to 0
        (0, True),  # returns an empty response, blocks are numbered from 1
        (UINT64_MAX, True),

        # Valid only_virtual
        (1, True),  # virtual operations given as bool is converted to number (True:1, False:0)
        (1, False),
        (1, None),  # None is converted to 0
        (1, 0),  # virtual_operation as number is converted to bool
        (1, 1),
        (1, 2),
    ]
)
@run_for('testnet', 'mainnet_5m', 'mainnet_64m')
def test_get_ops_in_block(node, should_prepare, block_num, only_virtual):
    if should_prepare:
        # Wait until block containing above transaction will become irreversible.
        node.wait_number_of_blocks(21)
    node.api.condenser.get_ops_in_block(block_num, only_virtual)


@pytest.mark.parametrize(
    "block_num, only_virtual", [
        # Invalid block_num
        ("incorrect_string_argument", True),
        ("", True),
        ([1], True),
        ((1,), True),
        ({}, True),

        # Invalid only_virtual
        (1, "incorrect_string_argument"),
        (1, [True]),
        (1, (True,)),
        (1, {}),
        (1, "1"),
    ]
)
@run_for('testnet', 'mainnet_5m', 'mainnet_64m')
def test_get_ops_in_block_with_incorrect_types_of_argument(node, block_num, only_virtual):
    with pytest.raises(tt.exceptions.CommunicationError):
        node.api.condenser.get_ops_in_block(block_num, only_virtual)


@run_for('testnet', 'mainnet_5m', 'mainnet_64m')
def test_get_ops_in_block_with_additional_argument(node):
    with pytest.raises(tt.exceptions.CommunicationError):
        node.api.condenser.get_ops_in_block(0, False, "additional_argument")


@run_for('testnet', 'mainnet_5m', 'mainnet_64m')
def test_get_ops_in_block_without_arguments(node):
    with pytest.raises(tt.exceptions.CommunicationError):
        node.api.condenser.get_ops_in_block()


@run_for('testnet', 'mainnet_5m', 'mainnet_64m')
def test_get_ops_in_block_with_default_second_argument(node):
    node.api.condenser.get_ops_in_block(1)
