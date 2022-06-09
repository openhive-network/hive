import pytest

from .local_tools import prepare_node_with_witnesses, run_for


WITNESSES_NAMES = [f'witness-{i}' for i in range(20)]  # 21-st is initminer


@run_for('testnet')
def test_get_witness_schedule_with_correct_value(prepared_node, should_prepare):
    if should_prepare:
        prepared_node = prepare_node_with_witnesses(WITNESSES_NAMES)
    prepared_node.api.wallet_bridge.get_witness_schedule()
