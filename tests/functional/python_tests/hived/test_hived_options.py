from pathlib import Path

import pytest

from test_tools import World


def test_dump_config(world: World):
    node = world.create_init_node()
    old_config = dict()
    for key, value in node.config.__dict__.items():
        old_config[key] = value
    node.run()
    node.wait_number_of_blocks(2)
    node.close()
    node.dump_config()
    assert node.config.__dict__ == old_config


def test_no_warning_about_deprecated_flag_exit_after_replay_when_it_is_not_used(world: World):
    node = world.create_init_node()
    node.run()

    with open(node.directory / 'stderr.txt') as file:
        stderr = file.read()

    warning = "flag `--exit-after-replay` is deprecated, please consider usage of `--exit-before-sync`"
    assert warning not in stderr


def test_warning_about_deprecated_flag_exit_after_replay(block_log, world: World):
    node = world.create_api_node()

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
def test_stop_after_replay(way_to_stop, world: World, block_log: Path, block_log_length: int):
    network = world.create_network()
    node_which_should_not_synchronize = network.create_api_node()
    node_with_new_blocks = network.create_init_node()

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
def test_stop_after_replay_in_load_from_snapshot(way_to_stop, world: World, block_log: Path):
    node = world.create_api_node()
    node.run(replay_from=block_log, **way_to_stop)
    snap = node.dump_snapshot(close=True)
    Path(node.directory / 'blockchain' / 'shared_memory.bin').unlink()
    node.run(load_snapshot_from=snap, **way_to_stop)
    assert not node.is_running()
