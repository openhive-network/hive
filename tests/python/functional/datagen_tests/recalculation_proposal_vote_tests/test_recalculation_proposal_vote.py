from __future__ import annotations

from pathlib import Path

import test_tools as tt
from hive_local_tools.functional.python.datagen.recalculation_proposal_vote_tests import (
    get_next_maintenance_time,
    wait_for_maintenance_block,
)
from hive_local_tools.functional.python.operation import get_virtual_operations
from schemas.operations.virtual import ProposalPayOperation


def test_recalculation_proposal_votes() -> None:
    node = tt.InitNode()
    block_log_directory = Path(__file__).parent.joinpath("block_log")
    block_log = tt.BlockLog(block_log_directory, "auto")

    # load block_log to node
    node.config.shared_file_size = "2G"
    node.config.plugin.append("account_history_api")
    node.config.block_log_split = 9999
    node.run(replay_from=block_log, exit_before_synchronization=True)

    # run node with specific timestamp ( end of block_log )
    node.run(time_control=tt.StartTimeControl(start_time="head_block_time"))

    tt.logger.info(f"next_maintenance_time: {get_next_maintenance_time(node)}")
    tt.logger.info(f"LBN: {node.get_last_block_number()}")
    tt.logger.info(f"HBT: {node.get_head_block_time()}")

    # the first maintenance block has the number 1262 ( processed in block_log )
    vops_in_first_maintenance_block = get_virtual_operations(
        node, ProposalPayOperation, start_block=1262, end_block=1263
    )
    proposal_id_from_first_maintenance_block = vops_in_first_maintenance_block[0]["op"]["value"]["proposal_id"]
    receiver_from_first_maintenance_block = vops_in_first_maintenance_block[0]["op"]["value"]["receiver"]

    lbn = node.get_last_block_number()
    assert len(get_virtual_operations(node, ProposalPayOperation, start_block=lbn, end_block=lbn + 100)) == 0

    wait_for_maintenance_block(node, get_next_maintenance_time(node))

    tt.logger.info(f"next_maintenance_time: {get_next_maintenance_time(node)}")
    tt.logger.info(f"LBN: {node.get_last_block_number()}")
    tt.logger.info(f"HBT: {node.get_head_block_time()}")

    assert len(get_virtual_operations(node, ProposalPayOperation, start_block=2300, end_block=2500)) > 0

    vops_in_second_maintenance_block = get_virtual_operations(
        node,
        ProposalPayOperation,
        start_block=node.get_last_block_number() - 100,
        end_block=node.get_last_block_number() + 100,
    )
    proposal_id_from_second_maintenance_block = vops_in_second_maintenance_block[0]["op"]["value"]["proposal_id"]
    receiver_from_second_maintenance_block = vops_in_second_maintenance_block[0]["op"]["value"]["receiver"]
    assert proposal_id_from_first_maintenance_block != proposal_id_from_second_maintenance_block
    assert receiver_from_first_maintenance_block != receiver_from_second_maintenance_block
