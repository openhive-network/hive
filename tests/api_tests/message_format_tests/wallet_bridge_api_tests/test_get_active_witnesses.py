from ....local_tools import run_for, replay_prepared_block_log


@run_for('testnet')
@replay_prepared_block_log
def test_get_active_witnesses_with_correct_value(node):
    node.api.wallet_bridge.get_active_witnesses()
