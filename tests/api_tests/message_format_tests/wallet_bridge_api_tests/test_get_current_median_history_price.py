from .local_tools import run_for


@run_for('testnet')
def test_get_current_median_history_price(prepared_node):
    prepared_node.api.wallet_bridge.get_current_median_history_price()
