from __future__ import annotations

import pytest
from beekeepy.exceptions import FailedToStartExecutableError

import test_tools as tt


@pytest.mark.parametrize(
    "fixture_name, block_log_split, block_num",
    [
        ("block_log_empty_30_mono", -1, 30),
        ("block_log_empty_430_split", 9999, 430),
    ],
    ids=["monolithic", "split"],
)
def test_clears_state_with_resync_blockchain_option(
    request: pytest.FixtureRequest,
    fixture_name: str,
    block_log_split: int,  # -1=monolithic, 0=LIB only, 1-9999=number of split parts
    block_num: int,
) -> None:
    """
    Test that --resync-blockchain option clears both chain database AND block log.

    Test sequence:
    1. Replay blockchain from the provided block_log fixture up to `block_num`.
    2. Verify that the block `block_num` exists via API.
    3. Restart the node with `--resync-blockchain` option and stop at block 10.
    4. Verify that the original `block_num` is no longer available (confirming block log deletion).
    5. Verify that the head block is consistent with the new replay range (<= 30).
    """
    block_log: tt.BlockLog = request.getfixturevalue(fixture_name)

    # 1. Start node and replay to block_num
    node = tt.InitNode()
    node.config.block_log_split = block_log_split

    node.run(replay_from=block_log, wait_for_live=True)

    # 2. Verify block_num exists
    block_last = node.api.block.get_block(block_num=block_num)
    assert block_last.is_set() is True, f"Block {block_num} should exist after replay"
    assert isinstance(block_last.block.block_id, str), f"Block {block_num} block_id should be a string"
    tt.logger.info(f"Block {block_num} exists: {block_last.block.block_id}")

    node.wait_for_irreversible_block(block_num)
    # 3. Close node
    node.close()

    # 4. Restart with fresh data, use --resync-blockchain option
    tt.logger.info("Restarting with --resync-blockchain option...")
    node.run(stop_at_block=10, arguments=["--resync-blockchain"])

    # 5. Verify block_num no longer exists
    # After resync, the node started from scratch and generates blocks from block 0
    # The original block_num should not be available
    block_last_after = node.api.block.get_block(block_num=block_num)
    assert (
        block_last_after.is_set() is False
    ), f"Block {block_num} should not exist after resync (block_log was deleted)"

    # Verify we're actually at block 10 (or close to it)
    head_block_num = node.get_last_block_number()
    assert head_block_num <= 30, f"Head block should be at most 30, got {head_block_num}"
    tt.logger.info(f"Head block is {head_block_num} (stopped at 10 approx)")
