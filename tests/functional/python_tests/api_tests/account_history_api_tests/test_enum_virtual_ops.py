import pytest
from dataclasses import dataclass

import test_tools as tt

from ..local_tools import BaseArgs

@dataclass
class EnumVirtualOpsArgs(BaseArgs):
    block_range_begin: int = 1
    block_range_end: int = 2
    include_reversible: bool = None
    group_by_block: bool = None
    operation_begin: int = None
    limit: int = None
    filter: int = None


@pytest.mark.parametrize('group_by_block, key', (
    (False, 'ops'),
    (True, 'ops_by_block')
))
def test_no_virtual_operations(ah_node: tt.InitNode, group_by_block: bool, key: str):
    ah_node.wait_number_of_blocks(5)
    block_to_start = ah_node.get_last_block_number() - 3
    response = ah_node.api.account_history.enum_virtual_ops(**EnumVirtualOpsArgs(
        block_range_begin=block_to_start,
        block_range_end=block_to_start + 1
    ).as_dict())
    assert len(response[key]) == 0
    assert response['next_block_range_begin'] == 0
    assert response['next_operation_begin'] == 0
