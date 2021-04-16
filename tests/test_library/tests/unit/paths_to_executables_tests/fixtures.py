import pytest

from test_library.paths_to_executables_implementation import *


@pytest.fixture
def paths():
    return PathsToExecutables()


@pytest.fixture
def executables():
    class Executable:
        def __init__(self, name):
            self.name = name
            self.path = f'{self.name}_path'

    return [
        Executable('hived'),
        Executable('cli_wallet'),
        Executable('get_dev_key'),
    ]
