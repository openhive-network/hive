from __future__ import annotations

from typing import TYPE_CHECKING

import test_tools as tt
from hive_local_tools.functional.python.operation import to_naive

if TYPE_CHECKING:
    from datetime import datetime


def get_next_maintenance_time(node: tt.InitNode) -> datetime:
    nmt = node.api.database.get_dynamic_global_properties().next_maintenance_time
    return tt.Time.parse(nmt, time_zone=None) if isinstance(nmt, str) else nmt


def wait_for_maintenance_block(node: tt.InitNode, maintenance_time: datetime) -> None:
    def is_maintenance_time() -> bool:
        hbt = node.get_head_block_time()
        tt.logger.info(f"hbt: {hbt}, maintenance_time: {maintenance_time}")
        return to_naive(hbt) > to_naive(maintenance_time)

    tt.logger.info("Wait for maintenance block.")
    tt.Time.wait_for(is_maintenance_time)
    tt.logger.info(f"Maintenance block was generated, at headblock {node.get_last_block_number()}")
