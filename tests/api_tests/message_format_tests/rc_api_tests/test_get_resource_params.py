from ..local_tools import run_for


# Resource credits (RC) were introduced after block with number 5000000, that's why this test is performed only on
# testnet and current mainnet.
@run_for('testnet', 'mainnet_64m')
def test_get_resource_params(prepared_node):
    prepared_node.api.rc.get_resource_params()
