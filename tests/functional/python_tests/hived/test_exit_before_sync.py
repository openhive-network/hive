from pathlib import Path
from test_tools import *
from .conftest import BLOCK_COUNT
from shutil import rmtree
from os import remove
from os.path import join


def test_exit_before_sync(world : World, block_log : Path):
  '''
  with specified '--exit-before-sync' flag:
    1. check after replaying
      - in the middle
      - full
    2. check exiting after dumping snapshot
    3. check exiting after loading snapshot
  '''

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