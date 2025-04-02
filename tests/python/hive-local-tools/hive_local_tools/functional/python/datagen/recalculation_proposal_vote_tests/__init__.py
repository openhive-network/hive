from __future__ import annotations

from typing import TYPE_CHECKING

import test_tools as tt

if TYPE_CHECKING:
    from datetime import datetime


def get_next_maintenance_time(node: tt.InitNode) -> datetime:
    return node.api.database.get_dynamic_global_properties().next_maintenance_time


def wait_for_maintenance_block(node: tt.InitNode, maintenance_time: datetime) -> None:
    def is_maintenance_time() -> bool:
        tt.logger.info(f"hbt: {node.get_head_block_time()}, maintenance_time: {maintenance_time}")
        return node.get_head_block_time() > maintenance_time

    tt.logger.info("Wait for maintenance block.")
    tt.Time.wait_for(is_maintenance_time)
    tt.logger.info(f"Maintenance block was generated, at headblock {node.get_last_block_number()}")
