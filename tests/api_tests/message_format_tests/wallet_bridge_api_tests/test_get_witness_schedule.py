from ....local_tools import replay_prepared_block_log, run_for


@run_for('testnet')
@replay_prepared_block_log
def test_get_witness_schedule_with_correct_value(node):
    node.api.wallet_bridge.get_witness_schedule()
