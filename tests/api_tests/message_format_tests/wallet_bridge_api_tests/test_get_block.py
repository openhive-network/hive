import pytest

import test_tools as tt

from .local_tools import as_string, run_for


UINT64_MAX = 2**64 - 1

CORRECT_VALUES = [
        -1,
        0,
        1,
        10,
        5 * 1000**2,  # 5 million block
        64 * 1000**2,  # 64 million block
        UINT64_MAX,
    ]


@pytest.mark.parametrize(
    'block_number', [
        *CORRECT_VALUES,
        *as_string(CORRECT_VALUES),
        True,
    ]
)
@run_for('testnet', 'mainnet_5m', 'mainnet_64m')
def test_get_block_with_correct_value(prepared_node, should_prepare, block_number):
    if should_prepare:
        if int(block_number) < 2:  # To get existing block for block ids: 0 and 1.
            prepared_node.wait_for_block_with_number(2)
    result = prepared_node.api.wallet_bridge.get_block(block_number)
    if len(result) != 0:
        assert len(result['block']) > 0


@pytest.mark.parametrize(
    'block_number', [
        UINT64_MAX+1,
    ]
)
@run_for('testnet', 'mainnet_5m', 'mainnet_64m')
def test_get_block_with_incorrect_value(prepared_node, block_number):
    with pytest.raises(tt.exceptions.CommunicationError):
        prepared_node.api.wallet_bridge.get_block(block_number)


@pytest.mark.parametrize(
    'block_number', [
        [0],
        'incorrect_string_argument',
        'true'
    ]
)
@run_for('testnet', 'mainnet_5m', 'mainnet_64m')
def test_get_block_with_incorrect_type_of_argument(prepared_node, block_number):
    with pytest.raises(tt.exceptions.CommunicationError):
        prepared_node.api.wallet_bridge.get_block(block_number)
