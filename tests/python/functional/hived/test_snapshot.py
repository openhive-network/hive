from __future__ import annotations

import os
from typing import TYPE_CHECKING, Final

import pytest

import test_tools as tt
from hive_local_tools.constants import BASE_ACCOUNTS
from hive_local_tools.functional.python.compare_snapshot import compare_snapshots_contents
from beekeepy.exceptions import FailedToStartExecutableError

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
    node.run(exit_before_synchronization=True, replay_from=block_log, arguments=["--force-replay"])
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


def test_snapshots_has_more_plugins(block_log: Path, block_log_length: int) -> None:
    def clear_state(node: tt.InitNode):
        from os.path import join as join_paths
        from shutil import rmtree

        rmtree(join_paths(str(node.directory), "blockchain", "shared_memory.bin"), ignore_errors=True)

    node = tt.InitNode()
    # extra plugin with new indexes: `block_log_info`
    node.config.plugin.append("block_log_info")
    node.run(exit_before_synchronization=True, replay_from=block_log)
    node.close()

    snap_0 = node.dump_snapshot(close=True)

    clear_state(node)
    node.config.plugin.remove("block_log_info")
    node.run(exit_before_synchronization=True, replay_from=block_log)
    node.run(load_snapshot_from=snap_0)
    node.wait_for_block_with_number(block_log_length + 5)
    node.close()

    # two files should be created which will show difference between state definitions in shm and current hived
    assert os.path.isfile(node.directory / "current_decoded_types_details.log")
    assert os.path.isfile(node.directory / "loaded_from_shm_decoded_types_details.log")

    warning_msg = "Snapshot has more plugins than current hived configuration"
    assert warning_msg in (node.directory / "stderr.log").read_text()


def test_snapshots_has_less_plugins(block_log: Path, block_log_length: int) -> None:
    def clear_state(node: tt.InitNode):
        from os.path import join as join_paths
        from shutil import rmtree

        rmtree(join_paths(str(node.directory), "blockchain", "shared_memory.bin"), ignore_errors=True)

    node = tt.InitNode()
    node.run(exit_before_synchronization=True, replay_from=block_log)
    node.close()

    snap_0 = node.dump_snapshot(close=True)

    clear_state(node)
    # add extra plugin to node - now snapshot has less plugins
    node.config.plugin.append("block_log_info")
    node.run(exit_before_synchronization=True, replay_from=block_log, arguments=["--force-replay"])

    with pytest.raises(FailedToStartExecutableError):
        node.run(load_snapshot_from=snap_0, exit_before_synchronization=True)

    # two files should be created which will show difference between state definitions in shm and current hived
    assert os.path.isfile(node.directory / "current_decoded_types_details.log")
    assert os.path.isfile(node.directory / "loaded_from_shm_decoded_types_details.log")

    error_msg_1 = "Snapshot misses plugins"
    error_msg_2 = "State objects definitions from shared memory file mismatch current version of app"

    log = (node.directory / "stderr.log").read_text()
    assert error_msg_1 in log
    assert error_msg_2 in log


def test_dump_snapshot_on_enabled_account_history_plugin_node() -> None:
    number_of_accounts: Final[int] = 10

    orig_node = tt.InitNode()
    orig_node.config.plugin.append("account_history_api")
    orig_node.run()

    wallet = tt.Wallet(attach_to=orig_node)
    wallet.create_accounts(number_of_accounts=number_of_accounts, name_base="account")

    assert (
        len(orig_node.api.database.list_accounts(start="", limit=100, order="by_name").accounts)
        == len(BASE_ACCOUNTS) + number_of_accounts
    )

    hb = orig_node.get_last_block_number()
    orig_node.wait_for_irreversible_block(hb)

    snap_0 = orig_node.dump_snapshot(close=True)

    snap_node = tt.InitNode()
    snap_node.config.plugin.append("account_history_api")
    snap_node.run(load_snapshot_from=snap_0)
    snap_node.wait_for_block_with_number(hb + 5)

    assert (
        len(snap_node.api.database.list_accounts(start="", limit=100, order="by_name").accounts)
        == len(BASE_ACCOUNTS) + number_of_accounts
    )
