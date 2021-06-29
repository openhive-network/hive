import pytest

from test_tools import automatic_tests_configuration, World


@pytest.fixture(autouse=True)
def configure_test_tools_paths(request):
    automatic_tests_configuration.configure_test_tools_paths(request)


@pytest.fixture
def world(request, configure_test_tools_paths):
    directory = automatic_tests_configuration.get_preferred_directory(request)
    with World(directory=directory) as world:
        yield world
