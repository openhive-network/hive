from test_library import World


def test_network_startup():
    with World() as world:
        network = world.create_network()
        network.create_init_node()
        network.create_node()
        network.create_node()

        network.run()
