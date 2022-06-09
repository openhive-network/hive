from .local_tools import run_for


@run_for('testnet')
def test_get_dynamic_global_properties(prepared_node):
    prepared_node.api.wallet_bridge.get_dynamic_global_properties()
