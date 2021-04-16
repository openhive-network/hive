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


def test_paths_set_manually(paths, executables):
    for executable in executables:
        paths.set_path_of(executable.name, executable.path)
        assert paths.get_path_of(executable.name) == executable.path
