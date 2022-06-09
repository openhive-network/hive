from .local_tools import prepare_node_with_witnesses, run_for


WITNESSES_NAMES = [f'witness-{i}' for i in range(20)]  # 21-st is initminer


@run_for('testnet')
def test_get_active_witnesses_with_correct_value(prepared_node):
    prepared_node = prepare_node_with_witnesses(WITNESSES_NAMES)
    prepared_node.api.wallet_bridge.get_active_witnesses()
