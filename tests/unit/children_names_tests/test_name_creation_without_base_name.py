import pytest

from test_library.children_names import NameBaseNotSet, ChildrenNames


@pytest.fixture
def names():
    return ChildrenNames()


def test_creation_attempt(names):
    with pytest.raises(NameBaseNotSet):
        names.create_name()


def test_creation_of_next_names_with_same_base(names):
    for i in range(5):
        assert names.create_name('Base') == f'Base{i}'
