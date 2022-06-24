from .local_tools import run_for


@run_for('testnet_replayed', 'mainnet_5m', 'mainnet_64m')
def test_get_witness_schedule_with_correct_value(prepared_node):
    result = prepared_node.api.wallet_bridge.get_witness_schedule()
    current_shuffled_witnesses = result['current_shuffled_witnesses']

    assert len(current_shuffled_witnesses) == 21
