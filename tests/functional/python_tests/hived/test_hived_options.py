from os import remove
from os.path import join
from pathlib import Path
from shutil import rmtree

from test_tools import World

from .conftest import BLOCK_COUNT


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


def test_no_appearance_of_deprecated_flag_exception_in_run_without_flag_exit_after_replay(world: World):
    node = world.create_init_node()
    node.run()

    with open(node.directory / 'stderr.txt') as file:
        stderr = file.read()

    warning = "flag `--exit-after-replay` is deprecated, please consider usage of `--exit-before-sync`"
    assert warning not in stderr


def test_appearance_of_deprecated_flag_exception_in_run_with_flag_exit_after_replay(world: World, block_log: Path):
    node = world.create_api_node()
    half_way = int(BLOCK_COUNT / 2.0)

    node.run(replay_from=block_log, stop_at_block=half_way, with_arguments=['--exit-after-replay'])

    with open(node.directory / 'stderr.txt') as file:
        stderr = file.read()

    warning = "flag `--exit-after-replay` is deprecated, please consider usage of `--exit-before-sync`"
    assert warning in stderr


def test_stop_after_replay_with_flag_exit_after_replay(world: World, block_log: Path):
    node = world.create_api_node()
    half_way = int(BLOCK_COUNT / 2.0)
    node.run(replay_from=block_log, stop_at_block=half_way, with_arguments=['--exit-after-replay'])
    assert not node.is_running()


def test_exit_after_replay_not_run_from_half_way(world: World, block_log: Path):
    node = world.create_api_node()
    half_way = int(BLOCK_COUNT / 2.0)

    node.run(replay_from=block_log, stop_at_block=half_way, with_arguments=['--exit-after-replay'])

    background_node = world.create_init_node()
    background_node.run(replay_from=block_log, wait_for_live=True)
    background_node.wait_number_of_blocks(2)

    rmtree(join(str(node.directory), 'blockchain'), ignore_errors=True)
    node.run(replay_from=block_log, with_arguments=['--exit-after-replay'])
    assert not node.is_running()


def test_stop_after_replay_in_load_from_snapshot_with_flag_exit_after_replay(world: World, block_log: Path):
    node = world.create_api_node()
    node.run(replay_from=block_log, with_arguments=['--exit-after-replay'])
    snap = node.dump_snapshot(close=True)
    remove(join(str(node.directory), 'blockchain', 'shared_memory.bin'))
    node.run(load_snapshot_from=snap, with_arguments=['--exit-after-replay'])
    assert not node.is_running()


def test_stop_after_replay_with_flag_exit_before_sync(world: World, block_log: Path):
    node = world.create_api_node()
    half_way = int(BLOCK_COUNT / 2.0)
    node.run(replay_from=block_log, stop_at_block=half_way, exit_before_synchronization=True)
    assert not node.is_running()


def test_exit_before_sync_not_run_from_half_way(world: World, block_log: Path):
    node = world.create_api_node()
    half_way = int(BLOCK_COUNT / 2.0)

    node.run(replay_from=block_log, stop_at_block=half_way, exit_before_synchronization=True)

    background_node = world.create_init_node()
    background_node.run(replay_from=block_log, wait_for_live=True)
    background_node.wait_number_of_blocks(2)

    rmtree(join(str(node.directory), 'blockchain'), ignore_errors=True)
    node.run(replay_from=block_log, exit_before_synchronization=True)
    assert not node.is_running()


def test_stop_after_replay_in_load_from_snapshot_with_flag_exit_before_sync(world: World, block_log: Path):
    node = world.create_api_node()
    node.run(replay_from=block_log, exit_before_synchronization=True)
    snap = node.dump_snapshot(close=True)
    remove(join(str(node.directory), 'blockchain', 'shared_memory.bin'))
    node.run(load_snapshot_from=snap, exit_before_synchronization=True)
    assert not node.is_running()
