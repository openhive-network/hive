from __future__ import annotations

import time
from pathlib import Path
from typing import Any, Final

import pytest

import test_tools as tt


def test_dump_config() -> None:
    node = tt.InitNode()
    old_config = node.config.json(exclude={"notifications_endpoint"})
    node.run()
    node.wait_number_of_blocks(2)
    node.close()
    node.dump_config()
    assert node.config.json(exclude={"notifications_endpoint"}) == old_config


def test_no_warning_about_deprecated_flag_exit_after_replay_when_it_is_not_used() -> None:
    node = tt.InitNode()
    node.run()

    with open(node.directory / "stderr.txt") as file:
        stderr = file.read()

    warning = "flag `--exit-after-replay` is deprecated, please consider usage of `--exit-before-sync`"
    assert warning not in stderr


def test_warning_about_deprecated_flag_exit_after_replay(block_log: Path) -> None:
    node = tt.ApiNode()

    node.run(replay_from=block_log, arguments=["--exit-after-replay"])

    with open(node.directory / "stderr.txt") as file:
        stderr = file.read()

    warning = "flag `--exit-after-replay` is deprecated, please consider usage of `--exit-before-sync`"
    assert warning in stderr


@pytest.mark.parametrize(
    "way_to_stop",
    [
        {"arguments": ["--exit-after-replay"]},
        {"exit_before_synchronization": True},
    ],
)
def test_stop_after_replay(way_to_stop: dict[str, Any], block_log: Path, block_log_length: int) -> None:
    network = tt.Network()
    node_which_should_not_synchronize = tt.ApiNode(network=network)
    node_with_new_blocks = tt.InitNode(network=network)

    # Generate new blocks, which are not present in source block log,
    # to check if other node will try to download them (synchronize).
    node_with_new_blocks.run(replay_from=block_log, wait_for_live=True)
    node_with_new_blocks.wait_number_of_blocks(6)

    node_which_should_not_synchronize.run(replay_from=block_log, **way_to_stop)
    assert not node_which_should_not_synchronize.is_running()

    node_with_new_blocks.close()  # Now node_which_should_not_synchronize will surely not synchronize.

    node_which_should_not_synchronize.run(wait_for_live=False)
    assert node_which_should_not_synchronize.get_last_block_number() == block_log_length  # No new blocks.


@pytest.mark.parametrize(
    "way_to_stop",
    [
        {"arguments": ["--exit-after-replay"]},
        {"exit_before_synchronization": True},
    ],
)
def test_stop_after_replay_in_load_from_snapshot(way_to_stop: dict[str, Any], block_log: Path) -> None:
    node = tt.ApiNode()
    node.run(replay_from=block_log, **way_to_stop)
    snap = node.dump_snapshot(close=True)
    Path(node.directory / "blockchain" / "shared_memory.bin").unlink()
    node.run(load_snapshot_from=snap, **way_to_stop)
    assert not node.is_running()


def test_stop_replay_at_given_block(block_log: Path, block_log_length: int) -> None:
    final_block: Final[int] = block_log_length // 2

    node = tt.ApiNode()
    node.run(replay_from=block_log, stop_at_block=final_block, wait_for_live=False)

    assert node.get_last_block_number() == final_block


def test_exit_replay_at_given_block(block_log: Path, block_log_length: int) -> None:
    final_block: Final[int] = block_log_length // 2

    node = tt.ApiNode()
    node.run(replay_from=block_log, exit_at_block=final_block)
    assert not node.is_running()
    with open(node.directory / "stderr.txt") as file:
        stderr = file.read()
        warning = f"Stopped blockchain replaying on user request. Last applied block number: {final_block}."
        assert warning in stderr


def test_stop_replay_at_given_block_with_enabled_witness_plugin(block_log: Path, block_log_length: int) -> None:
    # In the past there was a problem with witness node stopped at given block. This issue was caused by communication
    # of witness plugin with p2p plugin. When node is stopped at given block, p2p plugin is not enabled, so witness
    # plugin was unable to get information from it and program used to crash.
    #
    # This test closes issue: https://gitlab.syncad.com/hive/hive/-/issues/249.

    final_block: Final[int] = block_log_length // 2

    node = tt.WitnessNode(witnesses=["alice"])
    node.run(replay_from=block_log, stop_at_block=final_block, wait_for_live=False)

    assert node.get_last_block_number() == final_block

    # Wait some time to increase a chance of p2p and witness plugins communication.
    time.sleep(10)

    assert node.get_last_block_number() == final_block  # Node should not produce any block since stop.
    assert node.is_running()  # Make sure, that node didn't crash.


def connect_nodes(first_node: tt.AnyNode, second_node: tt.AnyNode) -> None:
    """
    This place have to be removed after solving issue https://gitlab.syncad.com/hive/test-tools/-/issues/10
    """
    second_node.config.p2p_seed_node = first_node.p2p_endpoint.as_string()


def test_stop_sync_mode_at_given_block() -> None:
    network = tt.Network()
    init_node = tt.InitNode(network=network)
    api_node = tt.ApiNode(network=network)
    init_node.run()
    init_node.wait_number_of_blocks(10)
    connect_nodes(init_node, api_node)
    api_node.run(stop_at_block=5)

    assert api_node.get_last_block_number() == 5


def test_exit_sync_mode_at_given_block() -> None:
    network = tt.Network()
    init_node = tt.InitNode(network=network)
    api_node = tt.ApiNode(network=network)
    init_node.run()
    init_node.wait_number_of_blocks(10)
    connect_nodes(init_node, api_node)
    api_node.run(exit_at_block=5)
    assert not api_node.is_running()
    with open(api_node.directory / "stderr.txt") as file:
        stderr = file.read()
        warning = "Stopped syncing on user request. Last applied block number: 5."
        assert warning in stderr


def test_stop_live_mode_at_given_block() -> None:
    network = tt.Network()
    init_node = tt.InitNode(network=network)
    api_node = tt.ApiNode(network=network)
    init_node.run()
    connect_nodes(init_node, api_node)
    api_node.run(stop_at_block=15)
    init_node.wait_number_of_blocks(30)

    assert api_node.get_last_block_number() == 15


def test_exit_live_mode_at_given_block() -> None:
    network = tt.Network()
    init_node = tt.InitNode(network=network)
    api_node = tt.ApiNode(network=network)
    init_node.run()
    connect_nodes(init_node, api_node)
    api_node.run(exit_at_block=15)
    init_node.wait_number_of_blocks(30)
    assert not api_node.is_running()
    with open(api_node.directory / "stderr.txt") as file:
        stderr = file.read()
        warning = "Stopped live mode on user request. Last applied block number: 15."
        assert warning in stderr


def test_hived_get_version() -> None:
    node = tt.RawNode()
    version_json = node.get_version()

    assert "version" in version_json

    expected_keys = {"blockchain_version", "hive_revision", "fc_revision", "node_type"}
    assert version_json["version"].keys() == expected_keys

    assert version_json["version"]["node_type"] == "testnet"
