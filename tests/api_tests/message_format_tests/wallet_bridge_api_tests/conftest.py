from pathlib import Path

import pytest

import test_tools as tt


def pytest_configure(config):
    config.addinivalue_line('markers', 'enabled_plugins: Enabled plugins in node from `node` fixture')


@pytest.fixture
def node(request):
    init_node = tt.InitNode()

    enabled_plugins = request.node.get_closest_marker('enabled_plugins')
    if enabled_plugins:
        init_node.config.plugin = list(enabled_plugins.args)

    init_node.run()
    return init_node


@pytest.fixture
def replayed_node():
    api_node = tt.ApiNode()
    api_node.run(replay_from=Path(__file__).parent.joinpath('block_log/block_log'), wait_for_live=False)
    return api_node


@pytest.fixture
def wallet(node):
    return tt.Wallet(attach_to=node)
