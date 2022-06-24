import pytest

import test_tools as tt
from hive_local_tools import run_for
from hive_local_tools.api.message_format import as_string
from hive_local_tools.api.message_format.wallet_bridge_api.constants import WITNESSES_NAMES
from hive_local_tools.api.message_format.wallet_bridge_api import prepare_node_with_witnesses


CORRECT_VALUES = [
    WITNESSES_NAMES[0],
    WITNESSES_NAMES[-1],
    'non-exist-acc',
    '',
    100,
    True,
]


@pytest.mark.parametrize(
    'witness_account', [
        *CORRECT_VALUES,
        *as_string(CORRECT_VALUES),
    ],
)
@run_for("testnet", "mainnet_5m", "live_mainnet")
def test_get_witness_with_correct_value(node, witness_account, should_prepare):
    if should_prepare:
        node = prepare_node_with_witnesses(WITNESSES_NAMES)
    node.api.wallet_bridge.get_witness(witness_account)


@pytest.mark.parametrize(
    'witness_account', [
        ['example-array']
    ]
)
@run_for("testnet", "mainnet_5m", "live_mainnet")
def test_get_witness_with_incorrect_type_of_argument(node, witness_account):
    with pytest.raises(tt.exceptions.CommunicationError):
        node.api.wallet_bridge.get_witness(witness_account)
