import pytest

import test_tools as tt


def pytest_configure(config):
    config.addinivalue_line('markers', 'node_shared_file_size: Shared file size of node from `node` fixture')


@pytest.fixture
def node(request):
    init_node = tt.InitNode()
    # The actual HF time does not matter as long as it's in the past

    shared_file_size = request.node.get_closest_marker('node_shared_file_size')
    if shared_file_size:
        init_node.config.shared_file_size = shared_file_size.args[0]

    init_node.run(environment_variables={"HIVE_HF26_TIME": "1598558400"})
    return init_node


@pytest.fixture
def wallet(node):
    return tt.Wallet(attach_to=node)
