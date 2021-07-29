from test_tools import World
from pathlib import Path
from .conftest import BLOCK_COUNT

def test_dump_config(world : World):
  node = world.create_init_node('init_0')
  old_config = dict()
  for key, value in node.config.__dict__.items(): old_config[key] = value
  node.run()
  node.wait_number_of_blocks(2)
  node.close()
  node.dump_config()

  assert node.config.__dict__ == old_config

def test_exit_before_sync(world : World, block_log : Path):
  from shutil import rmtree
  from os import remove
  from os.path import join

  init = world.create_api_node(name='node_1')
  half_way = int(BLOCK_COUNT / 2.0)

  init.run(replay_from=block_log, stop_at_block=half_way, exit_before_synchronization=True)
  assert not init.is_running()

  rmtree( join( str(init.directory), 'blockchain' ), ignore_errors=True)
  init.run(replay_from=block_log, exit_before_synchronization=True)
  assert not init.is_running()

  snap = init.dump_snapshot(close=True)
  assert not init.is_running()

  remove( join( str(init.directory), 'blockchain', 'shared_memory.bin' ) )
  init.run(load_snapshot_from=snap, exit_before_synchronization=True)
  assert not init.is_running()