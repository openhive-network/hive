import pytest

from test_tools import Wallet


@pytest.fixture
def node(world):
    init_node = world.create_init_node()
    init_node.run()
    return init_node


@pytest.fixture
def wallet(node):
    return Wallet(attach_to=node)
