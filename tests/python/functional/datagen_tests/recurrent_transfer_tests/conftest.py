from __future__ import annotations

from typing import TYPE_CHECKING

import pytest

import test_tools as tt

if TYPE_CHECKING:
    from datetime import datetime
    from pathlib import Path

    from hive_local_tools.functional.python.datagen.recurrent_transfer import ReplayedNodeMaker


@pytest.fixture()
def replayed_node() -> ReplayedNodeMaker:
    def _replayed_node(
        block_log_directory: Path,
        *,
        alternate_chain_specs: tt.AlternateChainSpecs | None = None,
        absolute_start_time: datetime | None = None,
        time_multiplier: float | None = None,
        timeout: float = tt.InitNode.DEFAULT_WAIT_FOR_LIVE_TIMEOUT,
    ) -> tt.InitNode:
        block_log = tt.BlockLog(block_log_directory, "auto")

        if absolute_start_time is None:
            time_offset = tt.StartTimeControl(start_time="head_block_time", speed_up_rate=time_multiplier)

        else:
            time_offset = tt.StartTimeControl(start_time=absolute_start_time, speed_up_rate=time_multiplier)

        node = tt.InitNode()
        node.config.shared_file_size = "16G"
        node.run(time_control=time_offset, replay_from=block_log, timeout=timeout, alternate_chain_specs=alternate_chain_specs)

        return node

    return _replayed_node
