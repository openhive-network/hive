from __future__ import annotations

import pytest

import test_tools as tt


def pytest_configure(config):
    config.addinivalue_line("markers", "enabled_plugins: Enabled plugins in node from `node` fixture")


@pytest.fixture()
def node(request: pytest.FixtureRequest) -> tt.InitNode:
    init_node = tt.InitNode()

    enabled_plugins = request.node.get_closest_marker("enabled_plugins")
    if enabled_plugins:
        init_node.config.plugin = list(enabled_plugins.args)

    init_node.config.plugin.append("market_history_api")
    init_node.config.plugin.append("account_by_key_api")
    init_node.run()
    return init_node
