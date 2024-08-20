from __future__ import annotations

from pathlib import Path

import pytest

import test_tools as tt


@pytest.fixture()
def replayed_node() -> tt.ApiNode:
    api_node = tt.ApiNode()
    api_node.config.block_log_split = -1
    api_node.run(replay_from=Path(__file__).parent.joinpath("block_log"), wait_for_live=False)
    return api_node
