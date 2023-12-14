from __future__ import annotations

import pytest

import test_tools as tt


@pytest.fixture()
def node() -> tt.InitNode:
    init_node = tt.InitNode()
    init_node.config.plugin.append("market_history_api")
    init_node.run()
    return init_node
