import pytest

from test_tools import automatic_tests_configuration, constants, World
from test_tools.private.scope import context
from test_tools.private.scope.scope_fixtures import *  # pylint: disable=wildcard-import, unused-wildcard-import


@pytest.fixture(autouse=True)
def configure_test_tools_paths(request):
    automatic_tests_configuration.configure_test_tools_paths(request)


@pytest.fixture
def world(configure_test_tools_paths):
    _ = configure_test_tools_paths  # This fixture is required to guarantee right order of fixtures creation
    with World(directory=context.get_current_directory()) as world:
        world.set_clean_up_policy(constants.WorldCleanUpPolicy.REMOVE_ONLY_UNNEEDED_FILES)
        yield world
