import pytest

from test_library.children_names import NameBaseNotSet, ChildrenNames


@pytest.fixture
def names():
    return ChildrenNames()


def test_creation_attempt(names):
    with pytest.raises(NameBaseNotSet):
        names.create_name()
