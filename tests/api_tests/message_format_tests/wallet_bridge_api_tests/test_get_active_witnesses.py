from .local_tools import run_for


@run_for('testnet_replayed', 'mainnet_5m', 'mainnet_64m')
def test_get_active_witnesses_with_correct_value(prepared_node):
    witnesses = prepared_node.api.wallet_bridge.get_active_witnesses()['witnesses']
    assert len(witnesses) == 21
