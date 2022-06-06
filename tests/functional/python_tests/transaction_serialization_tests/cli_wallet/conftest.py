import pytest

from test_tools import Wallet, World


@pytest.fixture
def node(world: World):
    node = world.create_init_node()
    node.run()
    return node
