from ....local_tools import run_for


@run_for('testnet', 'mainnet_5m', 'mainnet_64m')
def test_get_current_median_history_price(node):
    node.api.condenser.get_current_median_history_price()
