from hive_local_tools import run_for


@run_for("testnet")
def test_get_chain_properties(node):
    node.api.wallet_bridge.get_chain_properties()
