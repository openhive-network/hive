from __future__ import annotations

import test_tools as tt
from hive_local_tools import run_for


# Tests for debug_node API can be performed only in testnet
@run_for("testnet")
def test_debug_generate_blocks_until(node: tt.InitNode) -> None:
    hbn_offset = node.api.wallet_bridge.get_dynamic_global_properties().head_block_number
    head_block_time_offset_s = 60
    expected_blocks_to_generate = (head_block_time_offset_s / 3)
    lower_bound = hbn_offset + expected_blocks_to_generate
    upper_bound = hbn_offset + expected_blocks_to_generate + 1
    tt.logger.info(f"Head block number before generation of blocks: {hbn_offset}")
    tt.logger.info(f"Apporved range: {lower_bound} - {upper_bound}")
    node.api.debug_node.debug_generate_blocks_until(
        debug_key=tt.Account("initminer").private_key,
        head_block_time=tt.Time.from_now(seconds=head_block_time_offset_s),
        generate_sparsely="false",
    )
    head_block_number_after_generation_of_blocks = (
        node.api.wallet_bridge.get_dynamic_global_properties().head_block_number
    )
    tt.logger.info(
        f"Head block number after generation of blocks: {head_block_number_after_generation_of_blocks}"
    )
    assert lower_bound <= head_block_number_after_generation_of_blocks <= upper_bound
