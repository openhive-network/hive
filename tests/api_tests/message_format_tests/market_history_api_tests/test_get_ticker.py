from ..local_tools import run_for


@run_for('testnet', 'mainnet_5m', 'mainnet_64m')
def test_get_ticker(node):
    node.api.market_history.get_ticker()
