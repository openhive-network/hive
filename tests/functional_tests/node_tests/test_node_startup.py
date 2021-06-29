import pytest


def test_init_node_startup(world):
    init_node = world.create_init_node()
    init_node.run()
    init_node.wait_for_block_with_number(1)


def test_startup_timeout(world):
    node = world.create_witness_node(witnesses=[])
    with pytest.raises(TimeoutError):
        node.run(timeout=2)
