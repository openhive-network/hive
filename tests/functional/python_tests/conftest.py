import pytest

from test_tools import logger, constants, World
from test_tools.private.scope import context
from test_tools.private.scope.scope_fixtures import *  # pylint: disable=wildcard-import, unused-wildcard-import


@pytest.fixture(scope="function")
def world():
    with World(directory=context.get_current_directory()) as world:
        world.set_clean_up_policy(constants.WorldCleanUpPolicy.REMOVE_ONLY_UNNEEDED_FILES)
        yield world


def pytest_exception_interact(report):
    logger.error(f'Test exception:\n{report.longreprtext}')
