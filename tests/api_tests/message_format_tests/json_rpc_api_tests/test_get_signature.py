from ....local_tools import run_for


@run_for('testnet', 'mainnet_5m', 'mainnet_64m')
def test_get_signature(node):
    for method in node.api.jsonrpc.get_methods():
        node.api.jsonrpc.get_signature(method=method)
