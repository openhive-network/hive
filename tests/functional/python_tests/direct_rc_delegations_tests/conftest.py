import pytest

from test_tools import Wallet


@pytest.fixture
def node(world):
    init_node = world.create_init_node()
    # The actual HF time does not matter as long as it's in the past
    init_node.run(environment_variables={"HIVE_HF26_TIME": "1598558400"})
    return init_node


@pytest.fixture
def wallet(node):
    return Wallet(attach_to=node)
