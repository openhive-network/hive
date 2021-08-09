def test_if_single_node_is_closed(world):
    node = world.create_init_node()
    node.run()

    world.close()
    assert not node.is_running()


def test_if_all_nodes_in_network_are_closed(world):
    network = world.create_network()

    network.create_init_node()
    for _ in range(3):
        network.create_api_node()

    network.run()

    world.close()

    nodes = world.nodes()

    assert len(nodes) == 4
    assert all(not node.is_running() for node in nodes)
