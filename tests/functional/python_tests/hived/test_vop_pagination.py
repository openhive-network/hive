from test_tools import World, logger
from test_tools.private.remote_node import RemoteNode
from pytest import fixture
from os import getenv

STEPS = 20
BLOCKS_PER_STEP = 100
BLOCK_MAX = 3000000
BLOCK_MIN = BLOCK_MAX - ( BLOCKS_PER_STEP * STEPS )


@fixture
def main_net(world : World) -> RemoteNode:
  REMOTE_NODE_URL = getenv('HIVED_MAINNET_ADDRESS')
  if REMOTE_NODE_URL is None: 
    REMOTE_NODE_URL = 'http://192.168.6.136:8091'
  assert REMOTE_NODE_URL is not None
  return world.create_remote_node(REMOTE_NODE_URL)

@fixture
def block_range():
  return range(BLOCK_MIN, BLOCK_MAX, BLOCKS_PER_STEP)


# class that compressing vop
class compressed_vop:
  def __init__(self, vop):
    from hashlib import sha512
    from json import dumps

    self.id = "{}_{}_{}".format( (~0x8000000000000000) & int(vop["operation_id"]), vop["block"], vop["trx_in_block"])
    self.checksum = sha512( dumps(vop).encode() ).hexdigest()
    # self.content = vop

  def get(self):
    return self.__dict__

# return compressed data from api call
def compress_vops(data : list) -> list:
  ret = []
  for vop in data:
    ret.append(compressed_vop(vop).get())
  return ret

# this function do call to API
def get_vops(range_begin : int, range_end : int, start_from_id : int, limit : int, main_net : RemoteNode) -> dict:
  return main_net.api.account_history.enum_virtual_ops(
    block_range_begin=range_begin,
    block_range_end=range_end,
    operation_begin=start_from_id,
    limit=limit
  )['result']

# checks is there anythink more to get
def paginated(data : dict, range_end : int) -> bool:
  return not ( data['next_operation_begin'] == 0 and ( data['next_block_range_begin'] == range_end or data['next_block_range_begin'] == 0 ) )

# do one huge call
def get_vops_at_once(range_begin : int, range_end : int, main_net : RemoteNode) -> list:
  MAX_AT_ONCE = 1000
  tmp = get_vops(range_begin, range_end, 0, MAX_AT_ONCE, main_net)
  assert not paginated(tmp, range_end)
  return compress_vops(tmp['ops'])

# generator, that get page by page in step given as limit
def get_vops_paginated(range_begin : int, range_end : int, limit : int, main_net : RemoteNode):
  ret = get_vops(range_begin, range_end, 0, limit, main_net)
  yield compress_vops(ret['ops'])
  if not paginated(ret, range_end):
    ret = None
  while ret is not None:
    ret = get_vops(ret['next_block_range_begin'], range_end, ret['next_operation_begin'], limit, main_net)
    yield compress_vops(ret['ops'])
    if not paginated(ret, range_end):
      ret = None
  yield None

# wrapper on generator that agregates paginated output
def get_vops_with_step(range_begin : int, range_end : int, limit : int, main_net : RemoteNode) -> list:
  next_object = get_vops_paginated(range_begin, range_end, limit, main_net)
  ret = []
  value = next(next_object)
  while value is not None:
    ret.extend(value)
    value = next(next_object)
  return ret

# proxy, to get_vops_with_step with limit set as 1
def get_vops_one_by_one(range_begin : int, range_end : int, main_net : RemoteNode) -> list:
  return get_vops_with_step(range_begin, range_end, 1, main_net)

# get same data in given range with diffrent step
def check_range(range_begin : int, step : int, main_net : RemoteNode):
  range_end = range_begin + BLOCKS_PER_STEP + 1
  logger.debug(f"gathering blocks in range [ {range_begin} ; {range_end} )")
  all_at_once = get_vops_at_once(range_begin, range_end, main_net)
  paginated = get_vops_with_step(range_begin, range_end, step, main_net)
  return all_at_once == paginated

def test_vop_pagination_by_1( main_net : RemoteNode, block_range):
  for i in block_range:
    assert check_range(i, 1, main_net)

def test_vop_pagination_by_2( main_net : RemoteNode, block_range):
  for i in block_range:
    assert check_range(i, 2, main_net)

def test_vop_pagination_by_5( main_net : RemoteNode, block_range):
  for i in block_range:
    assert check_range(i, 5, main_net)

def test_vop_pagination_by_10( main_net : RemoteNode, block_range):
  for i in block_range:
    assert check_range(i, 10, main_net)
