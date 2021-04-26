from test_library import World


def test_no_nodes():
    with World() as world:
        assert world.nodes() == []


def test_single_node():
    with World() as world:
        node = world.create_node()

        assert len(world.nodes()) == 1
        assert node in world.nodes()
