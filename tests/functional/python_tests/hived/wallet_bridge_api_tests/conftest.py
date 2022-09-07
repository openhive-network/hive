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
