from __future__ import annotations

from typing import Final

import pytest

import test_tools as tt

NUMBER_OF_REVERSIBLE_BLOCKS: Final[int] = 21


@pytest.mark.testnet()
@pytest.mark.timeout(120)
def test_option_exit_at_block() -> None:
    node = tt.InitNode()
    node.run(exit_at_block=5 + NUMBER_OF_REVERSIBLE_BLOCKS)

    # block_log saved only irreversible blocks
    assert node.block_log.get_head_block_number() == 5, "Incorrect number of blocks in saved block_log."
    assert not node.is_running(), "Node still running, it should not."


@pytest.mark.testnet()
def test_option_stop_at_block() -> None:
    node = tt.InitNode()
    node.run(stop_at_block=5)
    node.wait_for_block_with_number(5, timeout=40)

    assert node.is_running(), "Node, shouldn't be closed."
    with pytest.raises(TimeoutError):
        node.wait_for_block_with_number(6, timeout=6)  # wait time needed to generate 2 blocks.
    assert node.get_last_block_number() == 5, "Node should be stop at block 10, not generate more."
