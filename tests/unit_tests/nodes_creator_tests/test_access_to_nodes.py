import pytest

from test_tools import World
from test_tools.network import Network


@pytest.fixture
def world():
    return World()


@pytest.fixture
def creators(world):
    return [world, Network('Alpha')]


def test_node_direct_access(creators):
    for creator in creators:
        init_node = creator.create_init_node('init-node')
        witness_node_0 = creator.create_witness_node('witness-node-0', witnesses=[])
        witness_node_1 = creator.create_witness_node('witness-node-1', witnesses=[])

        assert init_node is creator.node('init-node')
        assert witness_node_0 is creator.node('witness-node-0')
        assert witness_node_1 is creator.node('witness-node-1')


def test_network_node_access_from_world(world):
    alpha_network = world.create_network('Alpha')
    init_node = alpha_network.create_init_node('init-node')

    assert alpha_network is world.network('Alpha')
    assert init_node is alpha_network.node('init-node')
