from __future__ import annotations

import pytest

import test_tools as tt


@pytest.fixture()
def wallet(node) -> tt.Wallet:
    return tt.Wallet(attach_to=node)


@pytest.fixture()
def node() -> tt.InitNode:
    init_node = tt.InitNode()
    init_node.config.plugin.append("market_history_api")
    init_node.config.plugin.append("account_history_api")
    init_node.config.plugin.append("condenser_api")
    init_node.run()
    return init_node
