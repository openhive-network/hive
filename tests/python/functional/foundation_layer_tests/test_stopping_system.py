from typing import Final
import pytest

import test_tools as tt
from hive_local_tools.functional import connect_nodes


def test_stop_at_self_generated_specified_block() -> None:
    """
        0                               20
        │-----------generate------------│
    ────●───────────────────────────────■────────▶[t]
      start                            stop
    """
    node = tt.InitNode()
    node.run(stop_at_block=20)

    node.wait_for_block_with_number(20)

    assert node.is_running
    assert node.get_last_block_number() == 20
    with pytest.raises(TimeoutError):
        node.wait_for_block_with_number(20 + 5, timeout=30)  # wait time needed to generate 5 blocks.


@pytest.mark.parametrize(
    "relative_stop_offset",
    [
        pytest.param(+5, id="stop_after_head"),
        pytest.param(0, id="stop_on_head"),
        pytest.param(-5, id="stop_before_head"),
    ],
)
def test_stop_at_specified_block_after_restart(relative_stop_offset: int) -> None:
    """
    Case 1 (stop_at_block = 35)
        0               30             30          35
        │------gen------│              │----gen----│
    ────●───────────────●──────────────■───────────■──▶[t]
      start            close         restart       stop

    Case 2 (stop_at_block = 30)
        0               30             30
        │------gen------│              │
    ────●───────────────●──────────────■──────────────▶[t]
      start            close         restart with parameter stop_at_block=30, node start on state in API-mode

    Case 3:  (stop_at_block = 25)
        0               30             30
        │------gen------│              │
    ────●───────────────●──────────────■──────────────▶[t]
      start            close         restart with parameter stop_at_block=25, node start on state in API-mode
    """
    node = tt.InitNode()
    node.run()

    restart_block: int = 30
    node.wait_for_irreversible_block(restart_block)
    node.close()

    requested_stop = restart_block + relative_stop_offset

    node.run(
        stop_at_block=requested_stop,
        timeout=120 if relative_stop_offset else node.DEFAULT_WAIT_FOR_LIVE_TIMEOUT,
    )

    expected_stop = max(requested_stop, restart_block)  # Node should never stop before head_block

    node.wait_for_block_with_number(expected_stop)

    assert node.is_running(), "Node shouldn't be closed."
    assert (
        node.get_last_block_number() == expected_stop
    ), f"Node should be stop at block {expected_stop}, not generate more."

    with pytest.raises(TimeoutError):
        node.wait_for_block_with_number(expected_stop + 5, timeout=30)  # wait time needed to generate 5 blocks.


@pytest.mark.parametrize(
    "relative_stop_offset",
    [
        +5,  # stop after headblock (stop=35, head=30)
        0,  # stop exactly on headblock (stop=30, head=30)
        -5,  # stop before headblock (stop=25, head=30)
    ],
)
def test_stop_at_specified_block_after_force_replay(relative_stop_offset: int) -> None:
    """
    `--force-replay` — delete all node data from node datadir, except block_log, and start replay on fresh data

    Case 1 (stop = 35)
        0                30                      30             35
        │-------gen------│──────force-replay─────│------gen-----│
    ────●────────────────●───────────────────────●──────────────■────────▶[t]
      start           restart                                   stop

    Case 2 (stop = 30)
        0                30                      30
        │-------gen------│──────force-replay─────│
    ────●────────────────●───────────────────────■──────────────────────▶[t]
      start           restart                    stop

    Case 3 (stop = 25)
        0                30                      25
        │-------gen------│──────force-replay─────│
    ────●────────────────●───────────────────────■───────────────────────▶[t]
      start           restart                    stop
    """
    node = tt.InitNode()
    node.run()

    restart_block: int = 30
    node.wait_for_irreversible_block(restart_block)
    node.close()

    requested_stop = restart_block + relative_stop_offset

    node.run(
        stop_at_block=requested_stop,
        timeout=120 if relative_stop_offset else node.DEFAULT_WAIT_FOR_LIVE_TIMEOUT,
        arguments=["--force-replay"],
    )

    node.wait_for_block_with_number(requested_stop)

    assert node.is_running(), "Node shouldn't be closed."
    assert (
        node.get_last_block_number() == requested_stop
    ), f"Node should be stop at block {requested_stop}, not generate more."
    with pytest.raises(TimeoutError):
        node.wait_for_block_with_number(requested_stop + 5, timeout=30)  # wait time needed to generate 5 blocks.


@pytest.mark.parametrize(
    "relative_stop_offset",
    [
        +5,  # stop after headblock (stop=35, head=30)
        0,  # stop exactly on headblock (stop=30, head=30)
        -5,  # stop before headblock (stop=25, head=30)
    ],
)
def test_stop_at_specified_block_after_replay_blockchain(relative_stop_offset: int) -> None:
    """
    `--replay-blockchain` — check current state and block_log head, if state is behind block_log,
                            the node continues replay from the block_log head position.

    Case 1 (stop = 35)
        0                30                           30             35
        │-------gen------│──────replay-blockchain─────│------gen-----│
    ────●────────────────●────────────────────────────●──────────────■────────▶[t]
      start           restart                                        stop

    Case 2 (stop = 30)
        0                30                           30
        │-------gen------│──────replay-blockchain─────│
    ────●────────────────●────────────────────────────■──────────────────────▶[t]
      start           restart                         stop

    Case 3 (stop = 25) In this case, even though stop_at_block is set to 25,
           the node will stop at block 30 → at the end of the state, and start API-mode

        0                30                           30
        │-------gen------│──────replay-blockchain─────│
    ────●────────────────●────────────────────────────■───────────────────────▶[t]
      start           restart                         stop
    """
    node = tt.InitNode()
    node.run()

    restart_block: int = 30
    node.wait_for_irreversible_block(restart_block)
    node.close()

    requested_stop = restart_block + relative_stop_offset

    node.run(
        stop_at_block=requested_stop,
        timeout=120 if relative_stop_offset else node.DEFAULT_WAIT_FOR_LIVE_TIMEOUT,
        arguments=["--replay-blockchain"],
    )

    expected_stop = max(requested_stop, restart_block)  # Node should never stop before head_block
    node.wait_for_block_with_number(expected_stop)

    assert node.is_running(), "Node shouldn't be closed."
    assert (
        node.get_last_block_number() == expected_stop
    ), f"Node should be stop at block {expected_stop}, not generate more."
    with pytest.raises(TimeoutError):
        node.wait_for_block_with_number(expected_stop + 5, timeout=30)  # wait time needed to generate 5 blocks.


@pytest.mark.parametrize(
    "relative_stop_offset",
    [
        20,  # stop after block_log headblock
        0,  # stop at finish block_log
        -20,  # stop before block_log headblock
    ],
)
def test_stop_at_specified_block_after_replay(
    block_log_empty_430_split: tt.BlockLog, relative_stop_offset: int
) -> None:
    """
        │─────────────replay───────│────│
    ────●──────────────────────────■────●──────────────▶[t]
      start                       stop

        │────────────────replay──────────────────│
    ────●────────────────────────────────────────■─────▶[t]
      start                                     stop

        │─────────────replay────────────│--sync--│
    ────●───────────────────────────────●────────■────▶[t]
      start                                     stop
    """
    node = tt.InitNode()
    node.run(
        replay_from=block_log_empty_430_split,
        stop_at_block=block_log_empty_430_split.get_head_block_number() + relative_stop_offset,
        timeout=120 if relative_stop_offset else node.DEFAULT_WAIT_FOR_LIVE_TIMEOUT,
    )

    node.wait_for_block_with_number(block_log_empty_430_split.get_head_block_number() + relative_stop_offset)

    assert node.get_last_block_number() == block_log_empty_430_split.get_head_block_number() + relative_stop_offset
    with pytest.raises(TimeoutError):
        node.wait_for_block_with_number(
            block_log_empty_430_split.get_head_block_number() + relative_stop_offset + 5, timeout=30
        )  # wait time needed to generate 5 blocks.


@pytest.mark.parametrize(
    "relative_stop_offset",
    [
        20,  # stop after block_log headblock
        0,  # stop at finish block_log
        -20,  # stop before block_log headblock
    ],
)
def test_stop_at_specified_block_after_snapshot(snapshot_430_blocks: tt.Snapshot, relative_stop_offset: int) -> None:
    """
    The snapshot contains the blockchain state at block 430. When the node is started with
    --stop-at-block=<value>, the stop limit cannot force the node to stop before the snapshot’s
    head block. A node must never stop earlier than the block height stored in the snapshot.

    If --stop-at-block is set to a value lower than the snapshot head (e.g. --stop-at-block=400),
    the node must still stop at block 430, because this is the minimum valid state boundary.

    However, if --stop-at-block specifies a value higher than the snapshot head (e.g. 450),
    the node should enter normal sync mode and fetch the missing blocks via P2P until it
    reaches the specified stop-at-block height.

    Case 1:
        0                               430   450
        │─────────load-snapshot──────────│-gen-│
    ────●────────────────────────────────●─────■──▶[t]
      start                                   stop

    Case2:
        0                               430
        │─────────load-snapshot──────────│
    ────●────────────────────────────────■────────▶[t]
      start

    Case 3:
        0                        410    430
        │─────────load-snapshot──────────│
    ────●─────────────────────────■──────●────────▶[t]
      start
    """
    node = tt.InitNode()

    requested_stop = 430 + relative_stop_offset

    node.run(
        load_snapshot_from=snapshot_430_blocks,
        stop_at_block=requested_stop,
        timeout=20 * 3,
    )

    expected_stop = max(requested_stop, 430)  # Node should never stop before head_block
    node.wait_for_block_with_number(expected_stop)

    assert node.api.database.get_dynamic_global_properties().head_block_number == expected_stop
    with pytest.raises(TimeoutError):
        node.wait_for_block_with_number(expected_stop + 5, timeout=30)  # wait time needed to generate 5 blocks.


@pytest.mark.parametrize(
    "wait_for_live",
    [
        True,  # stop node at sync mode
        False,  # stop node at live mode
    ],
)
def test_stop_at_blok_in_sync_mode(block_log_empty_430_split: tt.BlockLog, wait_for_live: bool) -> None:
    """
    api-node connected to init-node:

    ────────────────────live─────────────────────▶ init-node
    --------------------sync--------------●~~~~~~▶ api-node
                                         stop

    ────────────────────live─────────────────────▶ init-node
    --------------------sync---─────live─────●~~~▶ api-node
                                            stop
    """
    block_log_headblock_time = tt.StartTimeControl(start_time=block_log_empty_430_split.get_head_block_time())

    requested_stop = (
        block_log_empty_430_split.get_head_block_number() + 30
        if wait_for_live
        else block_log_empty_430_split.get_head_block_number()
    )

    node = tt.InitNode()
    node.run(replay_from=block_log_empty_430_split, time_control=block_log_headblock_time)
    node.wait_for_block_with_number(block_log_empty_430_split.get_head_block_number() + 10)

    api_node = tt.ApiNode()
    connect_nodes(first_node=node, second_node=api_node)
    api_node.run(
        time_control=tt.StartTimeControl(start_time=node.get_head_block_time()),
        stop_at_block=requested_stop,
        timeout=120,
        wait_for_live=wait_for_live,
    )

    statuses = get_node_statuses(api_node)
    assert ("entering live mode" in statuses) == wait_for_live
    api_node.wait_for_block_with_number(requested_stop)

    assert api_node.get_last_block_number() <= node.get_last_block_number()
    assert node.is_running(), "Node shouldn't be closed."
    assert api_node.is_running(), "Node shouldn't be closed."
    assert (
        api_node.get_last_block_number() == requested_stop
    ), f"Node should be stop at block {requested_stop}, not generate more."
    with pytest.raises(TimeoutError):
        api_node.wait_for_block_with_number(requested_stop + 5, timeout=30)  # wait time needed to generate 5 blocks.


def get_node_statuses(node: tt.InitNode | tt.ApiNode) -> list[str]:
    return [s.status for s in node.api.app_status_api.get_app_status().statuses]
