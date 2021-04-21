import pytest

from fixtures import paths


def test_unsupported_executable(paths):
    with pytest.raises(Exception):
        paths.get_path_of('unsupported_executable')
