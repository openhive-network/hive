import pytest

from test_library.paths_to_executables_implementation import *


@pytest.fixture
def paths():
    return PathsToExecutables()


@pytest.fixture
def executables():
    class Executable:
        def __init__(self, name, command_line_argument):
            self.name = name
            self.path = f'{self.name}_path'
            self.argument = command_line_argument

    return [
        Executable('hived', '--hived-path'),
        Executable('cli_wallet', '--cli-wallet-path'),
        Executable('get_dev_key', '--get-dev-key-path'),
    ]
