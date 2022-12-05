from pathlib import Path
import time
from typing import Final

import pytest

import test_tools as tt


def test_dump_config():
    node = tt.InitNode()
    old_config = dict()
    for key, value in node.config.__dict__.items():
        old_config[key] = value
    node.run()
    node.wait_number_of_blocks(2)
    node.close()
    node.dump_config()
    assert node.config.__dict__ == old_config


def test_no_warning_about_deprecated_flag_exit_after_replay_when_it_is_not_used():
    node = tt.InitNode()
    node.run()

    with open(node.directory / 'stderr.txt') as file:
        stderr = file.read()

    warning = "flag `--exit-after-replay` is deprecated, please consider usage of `--exit-before-sync`"
    assert warning not in stderr


def test_warning_about_deprecated_flag_exit_after_replay(block_log):
    node = tt.ApiNode()

    node.run(replay_from=block_log, arguments=['--exit-after-replay'])

    with open(node.directory / 'stderr.txt') as file:
        stderr = file.read()

    warning = "flag `--exit-after-replay` is deprecated, please consider usage of `--exit-before-sync`"
    assert warning in stderr


@pytest.mark.parametrize(
    'way_to_stop', [
        {'arguments': ['--exit-after-replay']},
        {'exit_before_synchronization': True},
    ]
)
def test_stop_after_replay(way_to_stop, block_log: Path, block_log_length: int):
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
    'way_to_stop', [
        {'arguments': ['--exit-after-replay']},
        {'exit_before_synchronization': True},
    ]
)
def test_stop_after_replay_in_load_from_snapshot(way_to_stop, block_log: Path):
    node = tt.ApiNode()
    node.run(replay_from=block_log, **way_to_stop)
    snap = node.dump_snapshot(close=True)
    Path(node.directory / 'blockchain' / 'shared_memory.bin').unlink()
    node.run(load_snapshot_from=snap, **way_to_stop)
    assert not node.is_running()


def test_stop_replay_at_given_block(block_log: Path, block_log_length: int):
    final_block: Final[int] = block_log_length // 2

    node = tt.ApiNode()
    node.run(replay_from=block_log, stop_at_block=final_block, wait_for_live=False)

    assert node.get_last_block_number() == final_block


def test_stop_replay_at_given_block_with_enabled_witness_plugin(block_log: Path, block_log_length: int):
    # In the past there was a problem with witness node stopped at given block. This issue was caused by communication
    # of witness plugin with p2p plugin. When node is stopped at given block, p2p plugin is not enabled, so witness
    # plugin was unable to get information from it and program used to crash.
    #
    # This test closes issue: https://gitlab.syncad.com/hive/hive/-/issues/249.

    final_block: Final[int] = block_log_length // 2

    node = tt.WitnessNode(witnesses=['alice'])
    node.run(replay_from=block_log, stop_at_block=final_block, wait_for_live=False)

    assert node.get_last_block_number() == final_block

    # Wait some time to increase a chance of p2p and witness plugins communication.
    time.sleep(10)

    assert node.get_last_block_number() == final_block  # Node should not produce any block since stop.
    assert node.is_running()  # Make sure, that node didn't crash.

def test_hived_get_version():
    node = tt.RawNode()
    version_json = node.get_version()

    assert "version" in version_json

    expected_keys = {"blockchain_version", "hive_revision", "fc_revision", "node_type"}
    assert version_json["version"].keys() == expected_keys

    assert version_json["version"]["node_type"] == "testnet"

def test_dump_options_does_not_contain_unknown_types():
    node = tt.InitNode()
    config_options = node.get_supported_config_options()
    cli_options = node.get_supported_cli_options()

    for option in config_options + cli_options:
        if option.value is not None:
            assert option.value.value_type != 'unknown'
