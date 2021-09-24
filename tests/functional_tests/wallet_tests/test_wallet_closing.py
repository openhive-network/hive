from test_tools.wallet import Wallet


def test_closing(world):
    init_node = world.create_init_node()
    init_node.run()

    wallet = Wallet(attach_to=init_node)
    wallet.close()

    assert not wallet.is_running()
