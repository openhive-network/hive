from hive_local_tools import run_for


@run_for("testnet", "mainnet_5m", "live_mainnet")
def test_get_volume(node):
    node.api.market_history.get_volume()
