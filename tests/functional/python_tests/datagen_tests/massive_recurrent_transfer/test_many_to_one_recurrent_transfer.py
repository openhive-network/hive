import math
from pathlib import Path

import test_tools as tt

from hive_local_tools.functional.python.datagen.massive_recurrent_transfer import ReplayedNodeMaker


def test_many_to_one_recurrent_transfer(replayed_node: ReplayedNodeMaker):
    receiver_accounts = ["bank"]
    base_accounts = ['hive.fund', 'initminer', 'miners', 'null', 'steem.dao', 'temp']

    block_log_directory = Path(__file__).parent / "block_logs/block_log_containing_many_to_one_recurrent_transfers"
    with open(block_log_directory / "timestamp", encoding='utf-8') as file:
        timestamp = tt.Time.parse(file.read())

    replayed_node = replayed_node(
        block_log_directory,
        absolute_start_time=timestamp + tt.Time.days(2),
        time_multiplier=50,
    )
    wallet = tt.Wallet(attach_to=replayed_node)

    number_of_created_accounts = len(wallet.list_accounts()) - len(base_accounts) - len(receiver_accounts)
    replayed_node.wait_number_of_blocks(math.ceil(number_of_created_accounts / 1000))

    bank_balance_after_the_second_top_up_with_recurrent_transfers = (
        replayed_node.api.wallet_bridge.get_account("bank")["balance"]
    )

    assert int(bank_balance_after_the_second_top_up_with_recurrent_transfers['amount']) == number_of_created_accounts * 2
