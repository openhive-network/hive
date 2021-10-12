import pytest

from test_tools import constants, Wallet, World
from test_tools.private.scope import context
from test_tools.private.scope.scope_fixtures import *  # pylint: disable=wildcard-import, unused-wildcard-import


@pytest.fixture
def world():
    with World(directory=context.get_current_directory()) as world:
        world.set_clean_up_policy(constants.WorldCleanUpPolicy.REMOVE_ONLY_UNNEEDED_FILES)
        yield world


@pytest.fixture
def node(world):
    init_node = world.create_init_node()
    init_node.run()
    return init_node


@pytest.fixture
def wallet(node):
    return Wallet(attach_to=node)