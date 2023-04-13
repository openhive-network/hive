import pytest

import test_tools as tt
from hive_local_tools import run_for
from hive_local_tools.api.message_format import as_string
from hive_local_tools.api.message_format.wallet_bridge_api import prepare_node_with_witnesses
from hive_local_tools.api.message_format.wallet_bridge_api.constants import WITNESSES_NAMES


CORRECT_VALUES = [
        # WITNESS ACCOUNT
        (WITNESSES_NAMES[0], 100),
        (WITNESSES_NAMES[-1], 100),
        ('non-exist-acc', 100),
        ('true', 100),
        ('', 100),
        (100, 100),
        (True, 100),
        ('100', 100),

        # LIMIT
        (WITNESSES_NAMES[0], 1),
        (WITNESSES_NAMES[0], 1000),
]


@pytest.mark.parametrize(
    'witness_account, limit', [
        *CORRECT_VALUES,
        *as_string(CORRECT_VALUES),
        (WITNESSES_NAMES[0], True),  # bool is treated like numeric (0:1)
    ]
)
@run_for("testnet", "mainnet_5m", "live_mainnet")
def test_list_witnesses_with_correct_value(node, witness_account, limit, should_prepare):
    if should_prepare:
        node = prepare_node_with_witnesses(node, WITNESSES_NAMES)
    node.api.wallet_bridge.list_witnesses(witness_account, limit)


@pytest.mark.parametrize(
    'witness_account, limit', [
        # LIMIT
        (WITNESSES_NAMES[0], 0),
        (WITNESSES_NAMES[0], 1001),
    ]
)
@run_for("testnet", "mainnet_5m", "live_mainnet")
def test_list_witnesses_with_incorrect_value(node, witness_account, limit, should_prepare):
    if should_prepare:
        node = prepare_node_with_witnesses(node, WITNESSES_NAMES)
    with pytest.raises(tt.exceptions.CommunicationError):
        node.api.wallet_bridge.list_witnesses(witness_account, limit)


@pytest.mark.parametrize(
    'witness_account, limit', [
        # WITNESS ACCOUNT
        ([WITNESSES_NAMES[0]], 100),

        # LIMIT
        (WITNESSES_NAMES[0], 'incorrect_string_argument'),
        (WITNESSES_NAMES[0], 'true'),
    ]
)
@run_for("testnet", "mainnet_5m", "live_mainnet")
def test_list_witnesses_with_incorrect_type_of_arguments(node, witness_account, limit, should_prepare):
    if should_prepare:
        node = prepare_node_with_witnesses(node, WITNESSES_NAMES)
    with pytest.raises(tt.exceptions.CommunicationError):
        node.api.wallet_bridge.list_witnesses(witness_account, limit)
