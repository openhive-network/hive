from __future__ import annotations

import os
import math
from pathlib import Path
from typing import TYPE_CHECKING

import test_tools as tt
from hive_local_tools.constants import MAX_RECURRENT_TRANSFERS_PER_BLOCK

from .block_logs.block_log_containing_many_to_one_recurrent_transfers import generate_block_log as bl

if TYPE_CHECKING:
    from hive_local_tools.functional.python.datagen.recurrent_transfer import ReplayedNodeMaker


def test_many_to_one_recurrent_transfer(replayed_node: ReplayedNodeMaker) -> None:
    destination_variable = os.environ.get("TESTING_BLOCK_LOGS_DESTINATION")
    assert destination_variable is not None, "Path TESTING_BLOCK_LOGS_DESTINATION must be set!"
    block_log_directory = Path(destination_variable) / "recurrent_many_to_one"
    block_log = tt.BlockLog(block_log_directory, "auto")

    replayed_node: tt.InitNode = replayed_node(
        block_log_directory,
        absolute_start_time=block_log.get_head_block_time() + tt.Time.days(2),
        time_multiplier=50,
    )

    replayed_node.wait_number_of_blocks(math.ceil(bl.NUMBER_OF_SENDER_ACCOUNTS / MAX_RECURRENT_TRANSFERS_PER_BLOCK))

    receiver_balance_after_the_second_top_up = replayed_node.api.wallet_bridge.get_accounts([bl.RECEIVER_ACCOUNT_NAME])[
        0
    ].balance

    expected_balance = bl.NUMBER_OF_SENDER_ACCOUNTS * bl.SINGLE_TRANSFER_AMOUNT * 2
    assert receiver_balance_after_the_second_top_up == expected_balance
