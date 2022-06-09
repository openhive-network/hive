from .local_tools import run_for


@run_for('testnet')
def test_get_version(prepared_node):
    prepared_node.api.wallet_bridge.get_version()
