from .local_tools import run_for


@run_for('testnet', 'mainnet_5m', 'mainnet_64m')
def test_get_chain_properties(prepared_node):
    prepared_node.api.wallet_bridge.get_chain_properties()
