from test_library import World


def test_if_wallet_attached_to_single_node_is_closed():
    with World() as world:
        init_node = world.create_init_node()
        init_node.run()

        wallet = init_node.attach_wallet()

    assert not wallet.is_running()


def test_if_wallet_attached_to_node_in_network_is_closed():
    with World() as world:
        network = world.create_network()
        init_node = network.create_init_node()
        network.run()

        wallet = init_node.attach_wallet()

    assert not wallet.is_running()
