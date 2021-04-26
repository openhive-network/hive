from test_library import World


def test_if_single_node_is_closed():
    with World() as world:
        node = world.create_node()
        node.config.enable_stale_production = True
        node.config.required_participation = 0
        node.set_witness('initminer')

        node.run()
        node.wait_for_synchronization()

    assert not node.is_running()
