import pytest

from test_tools import automatic_tests_configuration


@pytest.fixture(autouse=True)
def configure_test_tools_paths(request):
    automatic_tests_configuration.configure_test_tools_paths(request)
