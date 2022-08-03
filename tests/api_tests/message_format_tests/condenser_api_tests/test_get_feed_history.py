from ..local_tools import run_for


@run_for('testnet', 'mainnet_5m', 'mainnet_64m')
def test_get_feed_history(prepared_node, should_prepare):
    prepared_node.api.condenser.get_feed_history()
