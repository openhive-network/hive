def test_network_startup(world):
    network = world.create_network()
    network.create_init_node()
    network.create_witness_node(witnesses=[])
    network.create_api_node()

    network.run()


def test_two_connected_networks_startup(world):
    first = world.create_network()
    first.create_init_node()
    for _ in range(3):
        first.create_api_node()

    second = world.create_network()
    for _ in range(3):
        second.create_api_node()

    first.connect_with(second)

    first.run()
    second.run()
