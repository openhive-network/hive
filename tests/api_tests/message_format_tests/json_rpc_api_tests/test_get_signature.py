from ..local_tools import run_for


@run_for('testnet', 'mainnet_5m', 'mainnet_64m')
def test_get_signature(prepared_node):
    for method in prepared_node.api.jsonrpc.get_methods():
        prepared_node.api.jsonrpc.get_signature(method=method)
