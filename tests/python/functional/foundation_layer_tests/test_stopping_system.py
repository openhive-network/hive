from typing import Final
import pytest

import test_tools as tt
from hive_local_tools.functional import connect_nodes

symbols: str = "■ ▶ ◆"


@pytest.fixture()
def snapshot_430(block_log_empty_430_split: tt.BlockLog) -> tt.Snapshot:
    node = tt.InitNode()
    node.run(replay_from=block_log_empty_430_split)
    snap = node.dump_snapshot(close=True)
    return snap


@pytest.mark.skip
def test_start_from_zero() -> None:
    """
        0                               20
        │-----------generate------------│
    ────●───────────────────────────────■────────▶[t]
      start                            stop
    """
    node = tt.InitNode()
    node.run(stop_at_block=20)

    assert node.is_running
    assert node.get_last_block_number == 20


@pytest.mark.skip
@pytest.mark.parametrize(
    "relative_stop_offset",
    [
        +5,  # stop after headblock      (stop=35, head=30)
        0,  # stop exactly on headblock (stop=30, head=30)
        -5,  # stop before headblock     (stop=25, head=30)
    ],
)
def test_witness_node_from_restart(relative_stop_offset: int) -> None:
    """
        0                30             35
        │-------gen------│------gen-----│
    ────●────────────────●──────────────■────────▶[t]
      start           restart          stop

        0                30             25
        │-------gen------│──────rep─────│
    ────●────────────────●──────────────■────────▶[t] # po restarcie miał się zatrzymać, a idzie dalej (-20)
      start           restart          stop

        0                30             30
        │-------gen------│──────rep─────│
    ────●────────────────●──────────────■────────▶[t] # po restarcie miał się zatrzymać, a idzie dalej (0)
      start           restart          stop
    """
    node = tt.InitNode()
    node.run()

    node.wait_for_irreversible_block(30)
    stop = node.get_last_block_number() + relative_stop_offset

    node.close()
    node.run(
        stop_at_block=stop + relative_stop_offset,
        timeout=120 if relative_stop_offset else node.DEFAULT_WAIT_FOR_LIVE_TIMEOUT,
    )

    assert node.is_running(), "Node, shouldn't be closed."
    assert (
        node.get_last_block_number() == stop + relative_stop_offset
    ), f"Node should be stop at block {stop + relative_stop_offset}, not generate more."
    with pytest.raises(TimeoutError):
        node.wait_for_block_with_number(
            stop + relative_stop_offset + 1, timeout=6
        )  # wait time needed to generate 2 blocks.


@pytest.mark.skip
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
        │─────────────replay───────│────│
    ────●──────────────────────────■────●────────▶[t]
      start                       stop

        │─────────────replay────────────│
    ────●───────────────────────────────■────────▶[t]
      start                            stop

        │─────────────replay────────────│----│
    ────●───────────────────────────────●────■───▶[t]
      start                                 stop

    """
    node = tt.InitNode()
    node.run(
        replay_from=block_log_empty_430_split,
        stop_at_block=block_log_empty_430_split.get_head_block_number() + relative_stop_offset,
        timeout=120 if relative_stop_offset else node.DEFAULT_WAIT_FOR_LIVE_TIMEOUT,
    )

    assert node.get_last_block_number() == block_log_empty_430_split.get_head_block_number() + relative_stop_offset


@pytest.mark.skip()
def test_witness_node_from_snapshot(block_log_empty_430_split: tt.BlockLog, snapshot_430: tt.Snapshot) -> None:
    """
        │------------generate-----------│
    ────●───────────────────────────────■────────▶[t] # po załadowaniu snapa, zaczyna zatrzymuje się na 431
    load-snapshot                      stop

        │─────────load-snapshot──────────│
    ────●──────────────────────────■─────■────────▶[t] # po załadowaniu snapa, zaczyna zatrzymuje się na 431
      start                            stop

    """
    node = tt.InitNode()
    node.run(
        load_snapshot_from=snapshot_430,
        # 430-30 po załadowaniu snapa, zaczyna zatrzymuje się na 431
        # 430+30 po załadowaniu snapa, zaczyna zatrzymuje się na 431
        # 430 po załadowaniu snapa, zaczyna zatrzymuje się na 431
        stop_at_block=430,
        timeout=20 * 3,
    )
    assert (
        node.api.database.get_dynamic_global_properties().head_block_number
        == block_log_empty_430_split.get_head_block_number()
    )


@pytest.mark.skip
@pytest.mark.parametrize(
    "relative_stop_offset",
    [
        20,  # stop after block_log headblock
        0,  # stop at finish block_log
        -20,  # stop before block_log headblock
    ],
)
def test_stop_at_sync(block_log_empty_430_split: tt.BlockLog, relative_stop_offset: int) -> None:
    """
    api-node connected to init-node:

    ────────────────────live─────────────────────▶ init-node
    --------------------sync--------------●------▶  api-node
                                         stop
    """
    block_log_headblock_time = tt.StartTimeControl(start_time=block_log_empty_430_split.get_head_block_time())
    stop: Final[int] = block_log_empty_430_split.get_head_block_number() + relative_stop_offset

    node = tt.InitNode()
    node.run(replay_from=block_log_empty_430_split, time_control=block_log_headblock_time)
    node.wait_for_block_with_number(block_log_empty_430_split.get_head_block_number() + 10)

    api_node = tt.ApiNode()
    connect_nodes(first_node=node, second_node=api_node)
    api_node.run(
        time_control=block_log_headblock_time,
        stop_at_block=stop,
        timeout=120 if relative_stop_offset else node.DEFAULT_WAIT_FOR_LIVE_TIMEOUT,
    )

    assert api_node.get_last_block_number() <= node.get_last_block_number()
    assert node.is_running(), "Node, shouldn't be closed."
    assert api_node.get_last_block_number() == stop, f"Node should be stop at block {stop}, not generate more."

    with pytest.raises(TimeoutError):
        api_node.wait_for_block_with_number(stop + 1, timeout=6)  # wait time needed to generate 2 blocks.


@pytest.mark.parametrize(
    "relative_stop_offset",
    [
        20,  # stop after block_log headblock
        0,  # stop at finish block_log
        -20,  # stop before block_log headblock
    ],
)
@pytest.mark.parametrize("state", ["sync", "restart", "replay", "snapshot"])
def test_stop_at_load_replay(block_log_empty_430_split: tt.BlockLog, relative_stop_offset: int, state: str) -> None:
    # [restart-0] - TimeoutError: ApiNode0: API mode not activated on time.
    # [restart--20] - TimeoutError: ApiNode0: API mode not activated on time.
    # [replay-20] - TimeoutError: Waiting too long for block 450
    # [snapshot-20] - TimeoutError: Synchronization not started on time.
    # [snapshot-0] - TimeoutError: Synchronization not started on time.
    # [snapshot--20] - TimeoutError: Synchronization not started
    """
    This test coverage 4 test-cases:

      1) api-node connected to init-node:

        ────────────────────live─────────────────────▶ init-node
        --------------------sync--------------●------▶ api-node
                                            stop


      2) replayed api-node connected to init-node:

        ────────────────────live─────────────────────▶ init-node
        ──────────────●-----------sync--------●------▶  api-node
                loaded-replay                stop


      3) restarted with state api-node connected to init-node:

        ────────────────────live─────────────────────▶ init-node
        ──────────────●-----------sync--------●------▶  api-node
                    restart                   stop


      4) snapshoted api-node connected to init-node

        ────────────────────live─────────────────────▶ init-node
        ──────────────●-----------sync--------●------▶  api-node
                loaded-snapshot              stop
    """
    block_log_headblock_time = tt.StartTimeControl(start_time=block_log_empty_430_split.get_head_block_time())
    stop: Final[int] = block_log_empty_430_split.get_head_block_number() + relative_stop_offset

    node = tt.InitNode()

    node.run(replay_from=block_log_empty_430_split, time_control=block_log_headblock_time)

    node.wait_for_block_with_number(block_log_empty_430_split.get_head_block_number() + 10)

    api_node = tt.ApiNode()
    connect_nodes(first_node=node, second_node=api_node)

    if state == "sync":
        api_node.run(
            time_control=block_log_headblock_time,
            stop_at_block=stop,
            timeout=120 if relative_stop_offset else node.DEFAULT_WAIT_FOR_LIVE_TIMEOUT,
        )
    elif state == "restart":
        api_node.run(
            replay_from=block_log_empty_430_split,
            time_control=block_log_headblock_time,
            exit_at_block=block_log_empty_430_split.get_head_block_number(),
        )
        api_node.close()
        api_node.run(
            stop_at_block=stop,
            timeout=120 if relative_stop_offset else node.DEFAULT_WAIT_FOR_LIVE_TIMEOUT,
        )
    elif state == "replay":
        api_node.run(
            replay_from=block_log_empty_430_split,
            time_control=block_log_headblock_time,
            stop_at_block=stop,
            timeout=120 if relative_stop_offset else node.DEFAULT_WAIT_FOR_LIVE_TIMEOUT,
        )
    elif state == "snapshot":
        snapshot = node.dump_snapshot()
        api_node.run(
            load_snapshot_from=snapshot,
            stop_at_block=stop,
            timeout=120,
        )

    if relative_stop_offset:
        api_node.wait_for_block_with_number(stop, timeout=120)

    assert api_node.get_last_block_number() <= node.get_last_block_number()
    assert node.is_running(), "Node, shouldn't be closed."
    assert api_node.get_last_block_number() == stop, f"Node should be stop at block {stop}, not generate more."

    with pytest.raises(TimeoutError):
        api_node.wait_for_block_with_number(stop + 1, timeout=6)  # wait time needed to generate 2 blocks.


def descriptions():
    """
    ────────────────────live─────────────────────▶ init-node
    ──────────────●-----sync------◆~~~~~p2p~~~●~~▶  api-node
    """

    # black --config /workspace/hive/tests/python/hive-local-tools/pyproject.toml test_stopping_system.py
    ...
