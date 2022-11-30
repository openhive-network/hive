from ....local_tools import run_for


@run_for('testnet', 'mainnet_5m', 'mainnet_64m')
def test_get_feed_history(node, should_prepare):
    node.api.condenser.get_feed_history()
