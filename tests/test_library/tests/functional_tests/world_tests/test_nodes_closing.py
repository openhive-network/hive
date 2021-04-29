from test_library import World


def test_if_single_node_is_closed():
    with World() as world:
        node = world.create_node()
        node.config.enable_stale_production = True
        node.config.required_participation = 0
        node.set_witness('initminer')

        node.run()

    assert not node.is_running()


def test_if_all_nodes_in_network_are_closed():
    with World() as world:
        network = world.create_network()

        init_node = network.create_node('InitNode')
        init_node.config.enable_stale_production = True
        init_node.config.required_participation = 0
        init_node.set_witness('initminer')

        for _ in range(3):
            network.create_node()

        network.run()

        nodes = world.nodes()

    assert len(nodes) == 4
    assert all([not node.is_running() for node in nodes])
