from pathlib import Path
from typing import Final
import math

import pytest

import test_tools as tt
from hive_local_tools.constants import BASE_ACCOUNTS, MAX_OPEN_RECURRENT_TRANSFERS, MAX_RECURRENT_TRANSFERS_PER_BLOCK
from hive_local_tools.functional.python.datagen.recurrent_transfer import ReplayedNodeMaker

AMOUNT_SELECTED_ACCOUNTS: Final[int] = 250
TIME_MULTIPLIER: Final[int] = 90


@pytest.mark.parametrize("limited_run", [False])  # Change to False if you want to run a full test run
def test_the_maximum_number_of_recurring_transfers_allowed_for_one_account(replayed_node: ReplayedNodeMaker, limited_run):
    """
    Test scenario: block log that was replayed contains ordered recurrent transfers.
      1) replay block_log it contains outstanding recurring transfers,
      2a) limited_run:
            - wait for first part of recurrent transfers to be processed {AMOUNT_SELECTED_ACCOUNTS},
            - assert if there is any overdue recurring transfer (from selected_accounts).
      2b) full_run:
            - wait for all recurrent transfers to be processed,
            - assert if there is any overdue recurring transfer (from all accounts).
    """
    block_log_directory = Path(__file__).parent / "block_logs/block_log_recurrent_transfer_everyone_to_everyone"

    with open(block_log_directory / "timestamp", encoding='utf-8') as file:
        timestamp = tt.Time.parse(file.read())

    # during this replay, before entering live mode node processes a lots of recurrent transfers, therefore the timeout has been increased.
    replayed_node = replayed_node(
        block_log_directory,
        absolute_start_time=timestamp + tt.Time.days(2),
        timeout=600,
        time_multiplier=TIME_MULTIPLIER,
    )

    number_of_accounts_to_check = (
        AMOUNT_SELECTED_ACCOUNTS
        if limited_run
        else len(tt.Wallet(attach_to=replayed_node).list_accounts()) - len(BASE_ACCOUNTS)
    )

    wait_for_recurrent_transfers_processing(replayed_node, accounts_to_processing=number_of_accounts_to_check)
    __assert_recurrent_transfers_processed_successfully(replayed_node, number_of_accounts_to_check)


def wait_for_recurrent_transfers_processing(node: tt.InitNode, accounts_to_processing: int):
    tt.logger.info(f"wait for recurrent transfers for {accounts_to_processing} accounts to be processed")
    blocks_to_wait = math.ceil(accounts_to_processing) * MAX_OPEN_RECURRENT_TRANSFERS / MAX_RECURRENT_TRANSFERS_PER_BLOCK
    tt.logger.info(f"start waiting, head_block: {node.get_last_block_number()}, blocks to wait for: {blocks_to_wait}")
    tt.logger.info(f"wait minutes: {(blocks_to_wait * 3) / 60}")
    node.wait_number_of_blocks(blocks_to_wait)
    tt.logger.info(f"finish waiting, head_block: {node.get_last_block_number()}")


def __assert_recurrent_transfers_processed_successfully(node: tt.InitNode, number_of_accounts: int):
    for num in range(number_of_accounts):
        assert node.api.wallet_bridge.find_recurrent_transfers(f"account-{num}") == []
