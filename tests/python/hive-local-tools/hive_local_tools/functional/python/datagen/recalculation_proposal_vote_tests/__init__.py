from __future__ import annotations

from datetime import datetime

import test_tools as tt


def get_next_maintenance_time(node: tt.InitNode) -> datetime:
    nmt = node.api.database.get_dynamic_global_properties().next_maintenance_time
    return datetime.fromisoformat(nmt) if isinstance(nmt, str) else nmt


def _to_naive(dt: datetime) -> datetime:
    return datetime(dt.year, dt.month, dt.day, dt.hour, dt.minute, dt.second)


def wait_for_maintenance_block(node: tt.InitNode, maintenance_time: datetime) -> None:
    def is_maintenance_time() -> bool:
        hbt = node.get_head_block_time()
        tt.logger.info(f"hbt: {hbt}, maintenance_time: {maintenance_time}")
        return _to_naive(hbt) > _to_naive(maintenance_time)

    tt.logger.info("Wait for maintenance block.")
    tt.Time.wait_for(is_maintenance_time)
    tt.logger.info(f"Maintenance block was generated, at headblock {node.get_last_block_number()}")
