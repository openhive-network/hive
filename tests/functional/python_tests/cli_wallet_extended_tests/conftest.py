import pytest


@pytest.fixture
def wallet(world):
    init_node = world.create_init_node()
    init_node.run()

    return init_node.attach_wallet()
