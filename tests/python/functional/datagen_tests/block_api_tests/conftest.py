from __future__ import annotations

import os
from pathlib import Path

import pytest

import test_tools as tt


@pytest.fixture()
def replayed_node(block_log_empty_430_split) -> tt.ApiNode:
    api_node = tt.ApiNode()
    api_node.run(replay_from=block_log_empty_430_split, wait_for_live=False)
    return api_node
