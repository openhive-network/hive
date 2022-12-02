from dataclasses import dataclass
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

