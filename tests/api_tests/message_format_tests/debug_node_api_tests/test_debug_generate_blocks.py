import test_tools as tt

from hive_local_tools import run_for


# Tests for debug_node API can be performed only in testnet
@run_for('testnet')
def test_debug_generate_blocks(node):
    head_block_number_before_generation_of_blocks = node.api.wallet_bridge.get_dynamic_global_properties()[
        'head_block_number']
    blocks_to_generate = 100
    node.api.debug_node.debug_generate_blocks(debug_key=tt.Account('initminer').private_key, count=blocks_to_generate,
                                              skip=0, miss_blocks=0, edit_if_needed=True)
    head_block_number_after_generation_of_blocks = node.api.condenser.get_dynamic_global_properties()[
        'head_block_number']
    assert head_block_number_after_generation_of_blocks == head_block_number_before_generation_of_blocks + \
           blocks_to_generate
