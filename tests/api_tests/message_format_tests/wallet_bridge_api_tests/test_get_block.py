import pytest

import test_tools as tt
from hive_local_tools import run_for
from hive_local_tools.api.message_format import as_string

UINT64_MAX = 2**64 - 1

CORRECT_VALUES = [
        -1,
        0,
        1,
        10,
        UINT64_MAX,
    ]


@pytest.mark.parametrize(
    'block_number', [
        *CORRECT_VALUES,
        *as_string(CORRECT_VALUES),
        True,
    ]
)
@run_for("testnet", "mainnet_5m", "live_mainnet")
def test_get_block_with_correct_value(node, should_prepare, block_number):
    if should_prepare:
        if int(block_number) < 2:  # To get existing block for block ids: 0 and 1.
            node.wait_for_block_with_number(2)

    node.api.wallet_bridge.get_block(block_number)


@pytest.mark.parametrize(
    'block_number', [
        UINT64_MAX+1,
    ]
)
@run_for("testnet", "mainnet_5m", "live_mainnet")
def test_get_block_with_incorrect_value(node, block_number):
    with pytest.raises(tt.exceptions.CommunicationError):
        node.api.wallet_bridge.get_block(block_number)


@pytest.mark.parametrize(
    'block_number', [
        [0],
        'incorrect_string_argument',
        'true'
    ]
)
@run_for("testnet", "mainnet_5m", "live_mainnet")
def test_get_block_with_incorrect_type_of_argument(node, block_number):
    with pytest.raises(tt.exceptions.CommunicationError):
        node.api.wallet_bridge.get_block(block_number)
