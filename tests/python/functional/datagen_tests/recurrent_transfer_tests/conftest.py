from __future__ import annotations

from pathlib import Path
from typing import TYPE_CHECKING

import pytest

import test_tools as tt

if TYPE_CHECKING:
    from datetime import datetime

    from hive_local_tools.functional.python.datagen.recurrent_transfer import ReplayedNodeMaker


@pytest.fixture(scope="session")
def block_log() -> tt.BlockLog:
    """Artifacts must be generated before running tests in parallel to avoid race conditions."""
    block_log_directory = Path(__file__).parent.joinpath("block_log")
    block_log = tt.BlockLog(block_log_directory / "block_log")
    block_log.generate_artifacts()
    return block_log


@pytest.fixture()
def replayed_node(block_log: tt.BlockLog) -> ReplayedNodeMaker:
    def _replayed_node(
        *,
        absolute_start_time: datetime | None = None,
        time_multiplier: float | None = None,
        timeout: float = tt.InitNode.DEFAULT_WAIT_FOR_LIVE_TIMEOUT,
    ) -> tt.InitNode:
        if absolute_start_time is None:
            absolute_start_time = block_log.get_head_block_time()

        absolute_start_time -= tt.Time.seconds(5)

        time_offset = tt.Time.serialize(absolute_start_time, format_=tt.TimeFormats.TIME_OFFSET_FORMAT)
        if time_multiplier is not None:
            time_offset += f" x{time_multiplier}"

        node = tt.InitNode()
        node.config.shared_file_size = "16G"
        node.run(time_offset=time_offset, replay_from=block_log, timeout=timeout)

        return node

    return _replayed_node
