import pytest

from test_tools import Wallet

def pytest_configure(config):
    config.addinivalue_line('markers', 'enabled_plugins: Enabled plugins in node from `node` fixture')

@pytest.fixture
def node(world, request):
    init_node = world.create_init_node()

    enabled_plugins = request.node.get_closest_marker('enabled_plugins')
    if enabled_plugins:
        init_node.config.plugin = list(enabled_plugins.args)

    init_node.run()
    return init_node
