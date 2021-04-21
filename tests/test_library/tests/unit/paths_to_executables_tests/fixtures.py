import pytest

from test_library.paths_to_executables_implementation import *


@pytest.fixture
def empty_paths():
    """Returns PathsToExecutables object without any value from current environment.

    Doesn't matter if user has e.g. set environment variable searched by this object or
    script is run with some command line argument. All such information are ignored.
    """

    paths = PathsToExecutables()
    paths.parse_command_line_arguments([])
    paths.set_environment_variables({})
    paths.set_installed_executables({})
    return paths


def __executables():
    class Executable:
        def __init__(self, name, command_line_argument, environment_variable):
            self.name = name
            self.path = f'{self.name}_path'
            self.argument = command_line_argument
            self.environment_variable = environment_variable

    return [
        Executable('hived', '--hived-path', 'HIVED_PATH'),
        Executable('cli_wallet', '--cli-wallet-path', 'CLI_WALLET_PATH'),
        Executable('get_dev_key', '--get-dev-key-path', 'GET_DEV_KEY_PATH'),
    ]


@pytest.fixture
def executables():
    return __executables()


@pytest.fixture
def executable():
    return __executables()[0]
