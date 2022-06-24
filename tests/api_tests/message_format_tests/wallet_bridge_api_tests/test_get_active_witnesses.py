import pytest

from .local_tools import run_for


WITNESSES_NAMES = [f'witness-{i}' for i in range(20)]  # 21-st is initminer


@run_for('testnet', 'mainnet_5m', 'mainnet_64m')
@pytest.mark.prepare_witnesses_in_node_config(WITNESSES_NAMES)
def test_get_active_witnesses_with_correct_value(prepared_node, should_prepare):
    if should_prepare:
        prepared_node.wait_number_of_blocks(21)
    witnesses = prepared_node.api.wallet_bridge.get_active_witnesses()['witnesses']
    assert len(witnesses) == 21
