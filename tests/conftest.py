import pytest

from test_tools import automatic_tests_configuration, constants, World
from test_tools.private.scope.scope_fixtures import *  # pylint: disable=wildcard-import, unused-wildcard-import


@pytest.fixture(autouse=True)
def configure_test_tools_paths(request):
    automatic_tests_configuration.configure_test_tools_paths(request)


@pytest.fixture
def world(request, configure_test_tools_paths):
    _ = configure_test_tools_paths  # This fixture is required to guarantee right order of fixtures creation
    directory = automatic_tests_configuration.get_preferred_directory(request)
    with World(directory=directory) as world:
        world.set_clean_up_policy(constants.WorldCleanUpPolicy.REMOVE_ONLY_UNNEEDED_FILES)
        yield world
