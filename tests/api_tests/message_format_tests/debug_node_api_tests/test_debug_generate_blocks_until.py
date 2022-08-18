import test_tools as tt

from hive_local_tools import run_for


# Tests for debug_node API can be performed only in testnet
@run_for('testnet')
def test_debug_generate_blocks_until(node):
    node.api.debug_node.debug_generate_blocks_until(debug_key=tt.Account('initminer').private_key,
                                                    head_block_time=tt.Time.from_now(minutes=1),
                                                    generate_sparsely='false')
    head_block_number_after_generation_of_blocks = node.api.wallet_bridge.get_dynamic_global_properties()[
        'head_block_number']
    assert 21 <= head_block_number_after_generation_of_blocks <= 22
