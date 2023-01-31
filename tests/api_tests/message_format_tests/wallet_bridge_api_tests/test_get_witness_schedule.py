from hive_local_tools import run_for
from hive_local_tools.api.message_format.wallet_bridge_api import prepare_node_with_witnesses
from hive_local_tools.api.message_format.wallet_bridge_api.constants import WITNESSES_NAMES


@run_for("testnet", "mainnet_5m", "live_mainnet")
def test_get_witness_schedule_with_correct_value_false(node, should_prepare):
    if should_prepare:
        node = prepare_node_with_witnesses(WITNESSES_NAMES)
    node.api.wallet_bridge.get_witness_schedule(False)


@run_for("testnet", "live_mainnet")
def test_get_witness_schedule_with_correct_value_true(node, should_prepare):
    if should_prepare:
        node = prepare_node_with_witnesses(WITNESSES_NAMES)
    node.api.wallet_bridge.get_witness_schedule(True)
