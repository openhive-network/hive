import test_tools as tt

from hive_local_tools import run_for


@run_for('testnet')
def test_debug_get_head_block(node):
    wallet = tt.Wallet(attach_to=node)
    wallet.api.create_account('initminer', 'alice', '{}')
    response = node.api.debug_node.debug_get_head_block()['block']
    assert response['previous'] == node.api.condenser.get_block(node.get_last_block_number())['previous']
