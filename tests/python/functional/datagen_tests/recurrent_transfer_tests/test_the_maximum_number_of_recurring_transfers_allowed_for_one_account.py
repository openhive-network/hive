from __future__ import annotations

import math
from pathlib import Path
from typing import TYPE_CHECKING

import pytest

import test_tools as tt
from hive_local_tools.constants import MAX_OPEN_RECURRENT_TRANSFERS, MAX_RECURRENT_TRANSFERS_PER_BLOCK

if TYPE_CHECKING:
    from hive_local_tools.functional.python.datagen.recurrent_transfer import ReplayedNodeMaker


@pytest.fixture(scope="session")
def block_log() -> tt.BlockLog:
    """Artifacts must be generated before running tests in parallel to avoid race conditions."""
    block_log_directory = Path(__file__).parent / "block_logs/block_log_recurrent_transfer_everyone_to_everyone"
    block_log = tt.BlockLog(block_log_directory / "block_log")
    block_log.generate_artifacts()
    return block_log


@pytest.mark.flaky(reruns=5, reruns_delay=30)
def test_the_maximum_number_of_recurring_transfers_allowed_for_one_account(
    block_log: tt.BlockLog, replayed_node: ReplayedNodeMaker
) -> None:
    """
    Test scenario: block log that was replayed contains ordered recurrent transfers.
      1) replay block_log it contains outstanding recurring transfers,
      2) wait for all recurrent transfers to be processed,
      3) assert if there is any overdue recurring transfer.
    """
    # during this replay, before entering live mode node processes a lots of recurrent transfers, therefore the timeout has been increased.
    replayed_node: tt.InitNode = replayed_node(
        absolute_start_time=block_log.get_head_block_time() + tt.Time.days(2),
        time_multiplier=45,
        timeout=1200,
    )
    wallet = tt.Wallet(attach_to=replayed_node)

    all_accounts_names = wallet.list_accounts()

    blocks_to_wait = math.ceil(
        len(all_accounts_names) * MAX_OPEN_RECURRENT_TRANSFERS / MAX_RECURRENT_TRANSFERS_PER_BLOCK
    )
    tt.logger.info(
        f"start waiting, headblock: {replayed_node.get_last_block_number()}, blocks to wait for: {blocks_to_wait}"
    )
    replayed_node.wait_number_of_blocks(blocks_to_wait)
    tt.logger.info(f"finish waiting, headblock: {replayed_node.get_last_block_number()}")

    for account_name in all_accounts_names:
        assert replayed_node.api.wallet_bridge.find_recurrent_transfers(account_name) == []
