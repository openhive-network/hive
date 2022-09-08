from .local_tools import run_for


@run_for('testnet_replayed')
def test_get_active_witnesses_with_correct_value(prepared_node):
    prepared_node.api.wallet_bridge.get_active_witnesses()
