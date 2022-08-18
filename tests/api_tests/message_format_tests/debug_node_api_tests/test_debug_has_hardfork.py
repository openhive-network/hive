from hive_local_tools import run_for


# Tests for debug_node API can be performed only in testnet
@run_for('testnet')
def test_debug_has_hardfork(node):
    node.api.debug_node.debug_has_hardfork(hardfork_id=0)
