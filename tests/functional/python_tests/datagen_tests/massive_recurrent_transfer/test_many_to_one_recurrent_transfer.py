import math
from pathlib import Path

import test_tools as tt

from hive_local_tools.functional.python.datagen.massive_recurrent_transfer import ReplayedNodeMaker
from hive_local_tools.constants import MAX_RECURRENT_TRANSFERS_PER_BLOCK
from .block_logs.block_log_containing_many_to_one_recurrent_transfers import generate_block_log as bl

def test_many_to_one_recurrent_transfer(replayed_node: ReplayedNodeMaker):
    block_log_directory = Path(bl.__file__).parent
    with open(block_log_directory / "timestamp", encoding="utf-8") as file:
        timestamp = tt.Time.parse(file.read())

    replayed_node = replayed_node(
        block_log_directory,
        absolute_start_time=timestamp + tt.Time.days(2),
        time_multiplier=50,
    )
    wallet = tt.Wallet(attach_to=replayed_node)

    replayed_node.wait_number_of_blocks(math.ceil(bl.NUMBER_OF_SENDER_ACCOUNTS / MAX_RECURRENT_TRANSFERS_PER_BLOCK))

    receiver_balance_after_the_second_top_up = (
        replayed_node.api.condenser.get_accounts([bl.RECEIVER_ACCOUNT_NAME])[0]["balance"]
    )

    expected_balance = bl.NUMBER_OF_SENDER_ACCOUNTS * bl.SINGLE_TRANSFER_AMOUNT * 2
    assert receiver_balance_after_the_second_top_up == expected_balance
