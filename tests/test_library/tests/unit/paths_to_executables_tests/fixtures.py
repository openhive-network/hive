import pytest

from test_library.paths_to_executables_implementation import *


@pytest.fixture
def paths():
    return PathsToExecutables()


@pytest.fixture
def executables():
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
