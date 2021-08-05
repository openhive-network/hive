import pytest

from test_tools import automatic_tests_configuration, constants, World


@pytest.fixture(autouse=True)
def configure_test_tools_paths(request):
    automatic_tests_configuration.configure_test_tools_paths(request)


@pytest.fixture
def world(request, configure_test_tools_paths):
    directory = automatic_tests_configuration.get_preferred_directory(request)
    with World(directory=directory) as world:
        world.set_clean_up_policy(constants.WorldCleanUpPolicy.REMOVE_ONLY_UNNEEDED_FILES)
        yield world
