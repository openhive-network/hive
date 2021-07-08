def test_if_snapshot_is_dumped_and_node_continues_to_run(world):
    init_node = world.create_init_node()
    init_node.run()

    init_node.dump_snapshot()
    assert init_node.is_running()


def test_if_snapshot_is_dumped_and_node_is_closed(world):
    init_node = world.create_init_node()
    init_node.run()

    init_node.dump_snapshot(close=True)
    assert not init_node.is_running()
