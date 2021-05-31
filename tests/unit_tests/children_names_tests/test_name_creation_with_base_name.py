import pytest

from test_tools.private.children_names import ChildrenNames, NameAlreadyInUse


@pytest.fixture
def names():
    return ChildrenNames('Name')


def test_next_names_have_increasing_number(names):
    for i in range(5):
        assert names.create_name() == f'Name{i}'


def test_registered_name_collision_with_already_generated(names):
    name_already_in_use = names.create_name()
    with pytest.raises(NameAlreadyInUse):
        names.register_name(name_already_in_use)


def test_registered_name_collision_with_already_registered(names):
    already_registered = names.register_name('SomeName')
    with pytest.raises(NameAlreadyInUse):
        names.register_name(already_registered)


def test_registered_names_are_skipped_during_creation(names):
    registered = names.register_name('Name0')
    created = names.create_name()
    assert created != registered
