from __future__ import annotations

import pytest

import test_tools as tt


@pytest.fixture()
def wallet(node):
    return tt.Wallet(attach_to=node)


@pytest.fixture()
def snapshot_430_blocks(block_log_empty_430_split: tt.BlockLog) -> tt.Snapshot:
    node = tt.InitNode()
    node.run(replay_from=block_log_empty_430_split, stop_at_block=430, max_retries=3)
    snapshot = node.dump_snapshot(close=True)
    return snapshot
