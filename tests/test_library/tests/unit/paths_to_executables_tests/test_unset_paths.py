import pytest

from test_library.paths_to_executables_implementation import *

@pytest.fixture
def paths():
    return PathsToExecutables()


@pytest.fixture
def executable_names():
    return ['hived', 'cli_wallet', 'get_dev_key']


def test_missing_paths(paths, executable_names):
    for executable in executable_names:
        with pytest.raises(Exception):
            paths.get_path_of(executable)
