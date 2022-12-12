from hive_local_tools import run_for


@run_for('testnet', 'mainnet_5m', 'live_mainnet')
def test_get_feed_history(node, should_prepare):
    node.api.condenser.get_feed_history()
