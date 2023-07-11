from hive_local_tools import run_for


# Tests for debug_node API can be performed only in testnet
@run_for("testnet")
def test_debug_get_witness_schedule(node):
    assert node.api.debug_node.debug_get_witness_schedule()
