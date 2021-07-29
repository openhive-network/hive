#!/usr/bin/python3

from pathlib import Path
from test_tools import World
from .conftest import BLOCK_COUNT

def test_snapshots_content_binary(world : World, block_log : Path):
  node = [None]
  snap = [None]

  node.append(world.create_api_node(name="node_1"))
  node[1].config.plugin.append('state_snapshot')
  node[1].run(exit_before_synchronization=True, replay_from=block_log)

  snap.append(node[1].dump_snapshot(close=True))

  node.append(world.create_api_node(name="node_2"))
  node[2].run(load_snapshot_from=snap[1], exit_before_synchronization=True)

  snap.append(node[2].dump_snapshot(close=True))

  # recursive file (md5) checksum check
  assert snap[1] == snap[2]


def test_snapshots_existing_dir(world : World, block_log : Path):
  def clear_state(node):
    from shutil import rmtree
    from os.path import join as join_paths
    rmtree( join_paths( str(node.directory), 'blockchain', 'shared_memory.bin' ), ignore_errors=True )

  ERROR_MESSAGE = 'is not empty. Creating snapshot rejected.'

  node = world.create_init_node(name='node_1') #, witnesses=['witnessaaa'])
  node.config.plugin.append('state_snapshot')
  node.run(exit_before_synchronization=True, replay_from=block_log)
  node.close()

  snap_0 = node.dump_snapshot(close=True)

  clear_state(node)
  node.run(load_snapshot_from=snap_0)
  node.wait_for_block_with_number(number=BLOCK_COUNT + 5)
  node.close()

  node.dump_snapshot(close=True)
  node.close()
  with open(node.directory / 'stderr.txt', 'r') as file:
    assert ERROR_MESSAGE in file.read(999999)
    '''
    for line in file:
      if ERROR_MESSAGE in line:
        ERROR_MESSAGE = 'FOUND'
        break
    '''

  # assert ERROR_MESSAGE == 'FOUND'

  clear_state(node)
  node.run(load_snapshot_from=snap_0)

  node.wait_for_block_with_number(number=BLOCK_COUNT + 7)
  node.close()
