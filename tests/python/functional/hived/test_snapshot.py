from __future__ import annotations

from typing import TYPE_CHECKING

import test_tools as tt
from hive_local_tools.functional.python.compare_snapshot import compare_snapshots_contents

if TYPE_CHECKING:
    from pathlib import Path

    from test_tools import Snapshot


def test_snapshots_content_binary(block_log: Path) -> None:
    node: list[tt.ApiNode] = []
    snap: list[Snapshot] = []

    node.append(tt.ApiNode())
    node[0].config.plugin.append("state_snapshot")
    node[0].run(exit_before_synchronization=True, replay_from=block_log)

    snap.append(node[0].dump_snapshot(name="snapshot1", close=True))

    node.append(tt.ApiNode())
    node[1].run(load_snapshot_from=snap[0], exit_before_synchronization=True)

    snap.append(node[1].dump_snapshot(name="snapshot2", close=True))

    compare_snapshots_contents(snap[0], snap[1])


def test_snapshots_existing_dir(block_log: Path, block_log_length: int) -> None:
    def clear_state(node: tt.InitNode):
        from os.path import join as join_paths
        from shutil import rmtree

        rmtree(join_paths(str(node.directory), "blockchain", "shared_memory.bin"), ignore_errors=True)

    error_message = "is not empty. Creating snapshot rejected."

    node = tt.InitNode()
    node.config.plugin.append("state_snapshot")
    node.run(exit_before_synchronization=True, replay_from=block_log)
    node.close()

    snap_0 = node.dump_snapshot(close=True)

    clear_state(node)
    node.run(load_snapshot_from=snap_0)
    node.wait_for_block_with_number(block_log_length + 5)
    node.close()

    node.dump_snapshot(close=True)
    node.close()
    with open(node.directory / "stderr.log") as file:
        assert error_message in file.read(999999)
        """
    for line in file:
      if ERROR_MESSAGE in line:
        ERROR_MESSAGE = 'FOUND'
        break
    """

    # assert ERROR_MESSAGE == 'FOUND'

    clear_state(node)
    node.run(load_snapshot_from=snap_0)

    node.wait_for_block_with_number(block_log_length + 7)
    node.close()
