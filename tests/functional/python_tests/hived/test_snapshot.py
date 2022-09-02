#!/usr/bin/python3

from pathlib import Path

import test_tools as tt


def test_snapshots_content_binary(block_log: Path):
  node = [None]
  snap = [None]

  node.append(tt.ApiNode())
  node[1].config.plugin.append('state_snapshot')
  node[1].run(exit_before_synchronization=True, replay_from=block_log)

  snap.append(node[1].dump_snapshot(close=True))

  node.append(tt.ApiNode())
  node[2].run(load_snapshot_from=snap[1], exit_before_synchronization=True)

  snap.append(node[2].dump_snapshot(close=True))

  # recursive file (md5) checksum check
  assert snap[1] == snap[2]


def test_snapshots_existing_dir(block_log: Path, block_log_length: int):
  def clear_state(node):
    from shutil import rmtree
    from os.path import join as join_paths
    rmtree( join_paths( str(node.directory), 'blockchain', 'shared_memory.bin' ), ignore_errors=True )

  ERROR_MESSAGE = 'is not empty. Creating snapshot rejected.'

  node = tt.InitNode()
  node.config.plugin.append('state_snapshot')
  node.run(exit_before_synchronization=True, replay_from=block_log)
  node.close()

  snap_0 = node.dump_snapshot(close=True)

  clear_state(node)
  node.run(load_snapshot_from=snap_0)
  node.wait_for_block_with_number(block_log_length + 5)
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

  node.wait_for_block_with_number(block_log_length + 7)
  node.close()
