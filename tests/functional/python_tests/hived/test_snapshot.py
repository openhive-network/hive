#!/usr/bin/python3

from pathlib import Path
from pytest import fixture
from test_tools import *

BLOCK_COUNT = 30
SNAPSHOT_NAME = 'snap_1'

# @fixture(scope='module')
@fixture()
def block_log(world : World) -> Path:
  from time import sleep
  logger.info(f'preparing block log with {BLOCK_COUNT} blocks')
  node = world.create_init_node()
  node.run(wait_for_live=True)

  wallet = node.attach_wallet()
  initminer = node.config.witness[0]

  current_block = 0
  while current_block <= BLOCK_COUNT:
    for _ in range(4):
      # put here some dummy trx so block log does not contain just empty blocks
      wallet.api.transfer_to_vesting(from_=initminer, to=initminer, amount=Asset.Test(0.001))

      sleep(0.2)
    current_block = node.get_last_block_number()

  node.close()
  return node.get_block_log().get_path()



def test_snapshots(world : World, block_log : Path):
  '''
  3 nodes:
  0: produces 20 blocks and stops (just to create block log)
  1: replays and create snapshot
  2: load from prev. snap.
  2: dump new snapshot
  - check are both snapshots same (recursively compare file checksums)
  '''

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


def test_snapshots_dir(world : World, block_log : Path):
  '''
  2 nodes:
  0: produces 20 blocks and stops (just to create block log)
  1: replay to 20
  1: dump snapshot to folder, that does not exists
  1: load snapshot, and check is running propoerly
  1: dump new snapshot to same folder, expect fail
  1: load same snapshot to check is old one not corrupted
  1: run for X blocks to check is everythink propoerly loaded
  '''
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
