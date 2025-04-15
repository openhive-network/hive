import pytest

import test_tools as tt
from python.functional.conftest import block_log_empty_430_split


@pytest.mark.parametrize(
    "relative_stop_offset",
    [
        20,  # stop after block_log headblock
        0,  # stop at finish block_log
        -20,  # stop before block_log headblock
    ],
)
def test_witness_node_from_restart(block_log_empty_430_split: tt.BlockLog, relative_stop_offset: int) -> None:
    """
        ■ ▶ ◆ ■

      │──────────│──────────│
    ──●──────────●──────────●──➜
      start      restart    stop
    """
    node = tt.InitNode()
    node.run()
    node.wait_for_irreversible_block(30)
    hb = node.get_last_block_number()
    node.close()
    node.run(stop_at_block=hb)
    # assert node.get_last_block_number() == 25
    print()


@pytest.mark.parametrize(
    "relative_stop_offset",
    [
        20,  # stop after block_log headblock
        0,  # stop at finish block_log
        -20,  # stop before block_log headblock
    ],
)
def test_witness_node_from_replay(block_log_empty_430_split: tt.BlockLog, relative_stop_offset: int) -> None:
    """
        │─────────────replay────────────│
    ────●──────────────────────────■────●────────▶t
      start                       stop

        │─────────────replay────────────│
    ────●───────────────────────────────■────────▶t
      start                            stop

        │─────────────replay────────────│
    ────●───────────────────────────────●────■───▶t
      start                                 stop

    """
    node = tt.InitNode()
    node.run(
        replay_from=block_log_empty_430_split,
        stop_at_block=block_log_empty_430_split.get_head_block_number() + relative_stop_offset,
        timeout=120 if relative_stop_offset else node.DEFAULT_WAIT_FOR_LIVE_TIMEOUT,
    )

    assert node.get_last_block_number() == block_log_empty_430_split.get_head_block_number() + relative_stop_offset


def test_witness_node_from_replay_testXD(block_log_empty_430_split: tt.BlockLog) -> None:
    """
        │─────────────replay────────────│
    ────●───────────────────────────────●────■───▶t
      start                                 stop
    """
    node = tt.InitNode()
    node.run(
        replay_from=block_log_empty_430_split,
        stop_at_block=block_log_empty_430_split.get_head_block_number() + 20,
        timeout=20 * 3,
    )
    assert node.get_last_block_number() == block_log_empty_430_split.get_head_block_number() + 20
