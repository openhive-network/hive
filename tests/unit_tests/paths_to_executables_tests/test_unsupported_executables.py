import pytest

from test_tools.exceptions import NotSupported


def test_get_unsupported_executable(paths):
    with pytest.raises(NotSupported):
        paths.get_path_of('unsupported_executable')


def test_set_unsupported_executable(paths):
    with pytest.raises(NotSupported):
        paths.set_path_of('unsupported_executable', 'path')
