import pytest

from test_tools import RemoteNode, Wallet


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


@pytest.fixture
def wallet(node):
    return Wallet(attach_to=node)


@pytest.fixture
def node5m():
    return RemoteNode(http_endpoint='http://hive-3.pl.syncad.com:18092')


@pytest.fixture
def node64m():
    return RemoteNode(http_endpoint='https://api.hive.blog:443')
    # FIXME uncomment after repair 64m consensus node
    # return RemoteNode(http_endpoint='http://hive-staging.pl.syncad.com:8090')

