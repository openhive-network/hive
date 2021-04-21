import pytest

from fixtures import empty_paths as paths
from test_library.paths_to_executables import NotSupported


def test_unsupported_executable(paths):
    with pytest.raises(NotSupported):
        paths.get_path_of('unsupported_executable')
