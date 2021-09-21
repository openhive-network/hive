import pytest
from typing import List, Iterable

import test_tools as tt


@pytest.fixture
def steps() -> int:
  return 20

@pytest.fixture
def blocks_per_step() -> int:
  return 100

@pytest.fixture
def block_max() -> int:
  return 3000000

@pytest.fixture
def block_min(block_max: int, blocks_per_step: int, steps: int) -> int:
  return block_max - (blocks_per_step * steps)

@pytest.fixture
def block_range_for_pagination(block_min: int, block_max: int, blocks_per_step: int):
  return range(block_min, block_max, blocks_per_step)

def get_all_virtual_operations(test_node: tt.RemoteNode, block_min: int, block_max: int, limit: int) -> List[dict]:
  non_paginated_virtual_operations = get_virtual_ops(test_node, block_min, block_max, 0, limit)
  assert is_pagination_complete(non_paginated_virtual_operations, block_max)
  return non_paginated_virtual_operations['ops']

def get_virtual_ops(test_node: tt.RemoteNode, range_begin: int, range_end: int, start_from_id: int, limit: int) -> dict:
  tt.logger.debug(f"calling for: {range_begin=} {range_end=} {start_from_id=} {limit=}")
  response: dict = test_node.api.account_history.enum_virtual_ops(
    block_range_begin=range_begin,
    block_range_end=range_end,
    operation_begin=start_from_id,
    limit=limit
  )
  tt.logger.debug(f"pagination status: {response['next_block_range_begin']=}, {response['next_operation_begin']=}")
  return response

def is_pagination_complete(data: dict, range_end: int) -> bool:
  return (data["next_block_range_begin"] == range_end and data["next_operation_begin"] == 0)

def paged_virtual_operations(test_node: tt.RemoteNode, range_begin: int, range_end: int, limit: int) -> Iterable[List[dict]]:
  response = dict(next_block_range_begin=range_begin, next_operation_begin=0)
  while not is_pagination_complete(response, range_end):
    response = get_virtual_ops(test_node, response['next_block_range_begin'], range_end, response['next_operation_begin'], limit)
    yield response['ops']

def get_virtual_operations_with_step(test_node: tt.RemoteNode, range_begin: int, range_end: int, step: int) -> list:
  result = []
  for page in paged_virtual_operations(test_node, range_begin, range_end, step):
    result.extend(page)
  return result

def check_range(test_node: tt.RemoteNode, range_begin : int, step: int, blocks_per_step: int, max_limit: int):
  range_end = range_begin + blocks_per_step + 1
  all_at_once = get_all_virtual_operations(test_node, range_begin, range_end, max_limit)
  paginated = get_virtual_operations_with_step(test_node, range_begin, range_end, step)
  assert len(all_at_once) == len(paginated)
  assert all_at_once == paginated

@pytest.mark.parametrize('page_width', [10, 5, 2, 1])
def test_vop_pagination_by(test_node: tt.RemoteNode, block_range_for_pagination: Iterable[int], page_width: int, blocks_per_step: int, request_limit: int):
  for block_number in block_range_for_pagination:
    tt.logger.debug(f"gathering blocks in range <{block_number} : {block_number + blocks_per_step}>")
    check_range(test_node, block_number, page_width, blocks_per_step, request_limit)
