import logging
import pytest

from test_tools import constants, World
from test_tools.private.scope import context
from test_tools.private.scope.scope_fixtures import *  # pylint: disable=wildcard-import, unused-wildcard-import

def pytest_addoption(parser):
    parser.addoption( "--http_endpoint", action="store", type=str, help='specifies http_endpoint of reference node')
    parser.addoption( "--ws_endpoint", action="store", type=str, help='specifies ws_endpoint of reference node')

@pytest.fixture
def world():
    with World(directory=context.get_current_directory()) as world:
        world.set_clean_up_policy(constants.WorldCleanUpPolicy.REMOVE_ONLY_UNNEEDED_FILES)
        yield world


def pytest_sessionstart() -> None:
    # Turn off unnecessary logs
    logging.getLogger('urllib3.connectionpool').propagate = False
