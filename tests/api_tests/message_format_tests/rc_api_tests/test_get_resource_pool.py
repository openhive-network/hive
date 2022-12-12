from hive_local_tools import run_for


# Resource credits (RC) were introduced after block with number 5000000, that's why this test is performed only on
# testnet and current mainnet.
@run_for('testnet', 'live_mainnet')
def test_get_resource_pool(node):
    node.api.rc.get_resource_pool()
