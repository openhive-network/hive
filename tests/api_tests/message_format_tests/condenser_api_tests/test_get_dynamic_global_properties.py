from ....local_tools import run_for


@run_for('testnet', 'mainnet_5m', 'mainnet_64m')
def test_get_dynamic_global_properties(node):
    node.api.condenser.get_dynamic_global_properties()
