from __future__ import annotations

from typing import Final

import pytest
import test_tools as tt

from wax.helpy.exceptions import BlockWaitTimeoutError


def test_raising_timeout(node: tt.InitNode) -> None:
    # ACT & ASSERT
    with pytest.raises(TimeoutError):
        node.wait_for_irreversible_block(100, timeout=0.1)


@pytest.mark.parametrize("include_number_param", [False, True], ids=("current_head_block", "block_with_number"))
def test_raising_block_wait_timeout_error(include_number_param: bool, node: tt.InitNode) -> None:
    # ARRANGE
    blocks_to_wait: Final[int] = 1
    expected_number_of_blocks = 21
    assert (
        blocks_to_wait < expected_number_of_blocks
    ), "blocks_to_wait should be lower than 21, else irreversible block number increases"

    last_block_number = node.get_last_block_number()

    # ACT & ASSERT
    with pytest.raises(BlockWaitTimeoutError):
        node.wait_for_irreversible_block(
            number=last_block_number if include_number_param else None, max_blocks_to_wait=blocks_to_wait
        )


@pytest.mark.parametrize("include_number_param", [False, True], ids=("current_head_block", "block_with_number"))
def test_waiting_for_irreversible_block(include_number_param: bool, node: tt.InitNode) -> None:
    # ARRANGE
    last_block_number = node.get_last_block_number()

    # ACT
    node.wait_for_irreversible_block(number=last_block_number if include_number_param else None)

    # ASSERT
    assert node.get_last_irreversible_block_number() >= last_block_number


@pytest.mark.parametrize(
    ("before_30", "include_number_param", "blocks_to_wait"),
    [
        (True, True, 21),
        (True, False, 21),
        (False, True, 0),
        (False, False, 0),
    ],
    ids=(
        "before_obi_when_head_<_30_with_number_given",
        "before_obi_when_head_<_30",
        "after_obi_when_head_>=_30_with_number_given",
        "after_obi_when_head_>=_30",
    ),
)
def test_waiting_for_irreversible_with_both_number_and_max_blocks_to_wait_args(
    before_30: bool, include_number_param: bool, blocks_to_wait: int, node: tt.InitNode
) -> None:
    """
    If head block number is < 30, then block will be considered as irreversible if there were 21 blocks produced after.

    Later, OBI is applied (at head block number 30) and given node is the block producer, so blocks will be considered
    as irreversible immediately.
    """
    # ARRANGE
    if not before_30:
        node.wait_for_block_with_number(30)

    last_block_number = node.get_last_block_number()

    # ACT
    node.wait_for_irreversible_block(
        last_block_number if include_number_param else None, max_blocks_to_wait=blocks_to_wait
    )

    # ASSERT
    last_irreversible_block_number = node.get_last_irreversible_block_number()
    tt.logger.info(f"Last head block number: {node.get_last_block_number()}")
    tt.logger.info(f"Last irreversible block number: {last_irreversible_block_number}")
    assert last_irreversible_block_number >= last_block_number
