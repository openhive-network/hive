from __future__ import annotations

import os
from pathlib import Path

import test_tools as tt
from hive_local_tools.functional.python.datagen.recalculation_proposal_vote_tests import (
    get_next_maintenance_time,
    wait_for_maintenance_block,
)
from hive_local_tools.functional.python.operation import get_virtual_operations
from schemas.operations.virtual import ProposalPayOperation


def test_recalculation_proposal_votes() -> None:
    destination_variable = os.environ.get("TESTING_BLOCK_LOGS_DESTINATION")
    assert destination_variable is not None, "Path TESTING_BLOCK_LOGS_DESTINATION must be set!"
    block_log_directory = Path(destination_variable) / "recalculation_proposal_vote"
    block_log = tt.BlockLog(block_log_directory, "auto")
    acs = tt.AlternateChainSpecs.parse_file(block_log_directory / tt.AlternateChainSpecs.FILENAME)

    node = tt.InitNode()

    # load block_log to node
    node.config.shared_file_size = "2G"
    node.config.plugin.append("account_history_api")
    node.config.block_log_split = 9999
    node.run(replay_from=block_log, exit_before_synchronization=True, alternate_chain_specs=acs)

    # run node with specific timestamp ( end of block_log )
    node.run(time_control=tt.StartTimeControl(start_time="head_block_time"), alternate_chain_specs=acs)

    tt.logger.info(f"next_maintenance_time: {get_next_maintenance_time(node)}")
    tt.logger.info(f"LBN: {node.get_last_block_number()}")
    tt.logger.info(f"HBT: {node.get_head_block_time()}")

    # the first maintenance block has the number 107 ( processed in block_log )
    vops_in_first_maintenance_block = get_virtual_operations(
        node, ProposalPayOperation, start_block=107, end_block=108
    )
    proposal_id_from_first_maintenance_block = vops_in_first_maintenance_block[0]["op"]["value"]["proposal_id"]
    receiver_from_first_maintenance_block = vops_in_first_maintenance_block[0]["op"]["value"]["receiver"]

    lbn = node.get_last_block_number()
    assert len(get_virtual_operations(node, ProposalPayOperation, start_block=lbn, end_block=lbn + 100)) == 0

    wait_for_maintenance_block(node, get_next_maintenance_time(node))
    maintenance_block_number = node.get_last_block_number()

    tt.logger.info(f"next_maintenance_time: {get_next_maintenance_time(node)}")
    tt.logger.info(f"LBN: {node.get_last_block_number()}")
    tt.logger.info(f"HBT: {node.get_head_block_time()}")

    vops_in_second_maintenance_block = get_virtual_operations(
        node,
        ProposalPayOperation,
        start_block=maintenance_block_number-1,
        end_block=maintenance_block_number,
    )
    assert len(vops_in_second_maintenance_block) > 0
    proposal_id_from_second_maintenance_block = vops_in_second_maintenance_block[0]["op"]["value"]["proposal_id"]
    receiver_from_second_maintenance_block = vops_in_second_maintenance_block[0]["op"]["value"]["receiver"]
    assert proposal_id_from_first_maintenance_block != proposal_id_from_second_maintenance_block
    assert receiver_from_first_maintenance_block != receiver_from_second_maintenance_block
