import pytest

from test_tools.exceptions import CommunicationError


@pytest.fixture
def node(world):
    init_node = world.create_init_node()
    init_node.run()

    return init_node


def test_if_raise_when_parameters_are_bad(node):
    with pytest.raises(CommunicationError):
        node.api.database.list_accounts(limit='wrong-type', order='wrong-value')
