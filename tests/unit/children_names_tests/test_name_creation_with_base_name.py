import pytest

from test_library.children_names import ChildrenNames


@pytest.fixture
def names():
    return ChildrenNames('Name')


def test_next_names_have_increasing_number(names):
    for i in range(5):
        assert names.create_name() == f'Name{i}'
