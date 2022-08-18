from hive_local_tools import run_for


# Tests for debug_node API can be performed only in testnet
@run_for('testnet')
def test_debug_get_witness_schedule(node):
    debug_api_method_reponse = node.api.debug_node.debug_get_witness_schedule()
    database_api_method_response = node.api.database.get_witness_schedule()
    assert debug_api_method_reponse == database_api_method_response
