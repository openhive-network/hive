from __future__ import annotations

import pytest
from test_tools.exceptions import FailedToStartExecutableError

import test_tools as tt
# UNSET is a sentinel value used by hiveio_api (msgspec) for optional fields
# not present in the API response — distinct from None which means "field present, value null".
from schemas.convert import UNSET


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
    assert block_last.block is not None, f"Block {block_num} should exist after replay"
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
        block_last_after.block is None or block_last_after.block is UNSET
    ), f"Block {block_num} should not exist after resync (block_log was deleted)"

    # Verify we're actually at block 10 (or close to it)
    head_block_num = node.get_last_block_number()
    assert head_block_num <= 30, f"Head block should be at most 30, got {head_block_num}"
    tt.logger.info(f"Head block is {head_block_num} (stopped at 10 approx)")


def test_resync_blockchain_fails_with_mixed_mode(
    block_log_empty_30_mono: tt.BlockLog,
) -> None:
    """
    Test that using a monolithic block log with a split configuration raises an exception.
    """
    node = tt.InitNode()
    # Configure split mode (9999) but provide monolithic block log
    node.config.block_log_split = 9999

    # First run should succeed (auto-split happens here)
    node.run(replay_from=block_log_empty_30_mono, wait_for_live=True)
    node.close()

    # Expected failure description:
    # The --resync-blockchain option removes the generated split block log files (because config is split)
    # but DOES NOT remove the original monolithic block log file (which is still there).
    # When the node restarts, it sees an empty state (cleaned by resync) but finds the monolithic block log.
    # It attempts to use it, but since state is empty and block log is present, it likely causes a mismatch
    # or re-initialization error (head block mismatch with state).

    with pytest.raises(FailedToStartExecutableError):
        # Restart with --resync-blockchain
        node.run(stop_at_block=10, arguments=["--resync-blockchain"])

    # Verify the specific error message in log
    # The error confirms that the node found an inconsistency between the block log (which was not removed)
    # and the state (which was reset).
    log_content = (node.directory / "stderr.log").read_text()
    expected_error = "Error: Headblock and statefile are inconsistent"
    assert expected_error in log_content, f"Expected '{expected_error}' in stderr"

    tt.logger.info("Successfully verified 'Headblock and statefile are inconsistent' error in logs.")
    node.close()
