from hive_local_tools import run_for


@run_for("testnet", "mainnet_5m", "live_mainnet")
def test_get_current_median_history_price(node):
    node.api.condenser.get_current_median_history_price()
