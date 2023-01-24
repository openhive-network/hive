from datetime import datetime
from pathlib import Path
from typing import Optional

import pytest

import test_tools as tt

from hive_local_tools.functional.python.datagen.recurrent_transfer import ReplayedNodeMaker

@pytest.fixture
def replayed_node() -> ReplayedNodeMaker:
    def _replayed_node(
        block_log_directory: Path,
        *,
        absolute_start_time: Optional[datetime] = None,
        time_multiplier: Optional[float] = None,
        timeout: float = tt.InitNode.DEFAULT_WAIT_FOR_LIVE_TIMEOUT,
    ) -> tt.InitNode:
        if absolute_start_time is None:
            with open(block_log_directory / "timestamp", encoding='utf-8') as file:
                absolute_start_time = tt.Time.parse(file.read())

        absolute_start_time -= tt.Time.seconds(5)

        time_offset = tt.Time.serialize(absolute_start_time, format_=tt.Time.TIME_OFFSET_FORMAT)
        if time_multiplier is not None:
            time_offset += f" x{time_multiplier}"

        node = tt.InitNode()
        node.config.shared_file_size = "16G"
        node.run(
            time_offset=time_offset,
            replay_from=block_log_directory / "block_log",
            timeout=timeout
        )

        return node
    return _replayed_node
