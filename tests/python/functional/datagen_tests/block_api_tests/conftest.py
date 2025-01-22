from __future__ import annotations

import os
from pathlib import Path

import pytest

import test_tools as tt


@pytest.fixture()
def replayed_node() -> tt.ApiNode:
    destination_variable = os.environ.get("TESTING_BLOCK_LOGS_DESTINATION")
    assert destination_variable is not None, "Path TESTING_BLOCK_LOGS_DESTINATION must be set!"
    block_logs_location: Path = Path(destination_variable) / "empty_430"

    api_node = tt.ApiNode()
    api_node.run(replay_from=block_logs_location, wait_for_live=False)
    return api_node
