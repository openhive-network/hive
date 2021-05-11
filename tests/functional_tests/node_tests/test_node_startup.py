from test_tools import World


def test_init_node_startup():
    with World() as world:
        init_node = world.create_init_node()
        init_node.run()
        init_node.wait_for_block_with_number(1)
