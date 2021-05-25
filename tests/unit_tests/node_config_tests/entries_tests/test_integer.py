import pytest

from test_tools.node_config_entry_types import Integer


@pytest.fixture
def entry():
    return Integer()


@pytest.fixture
def values():
    return [
        ('0', 0),
        ('123', 123),
        ('-1', -1),
    ]


def test_parsing(entry, values):
    for input_text, expected in values:
        entry.parse_from_text(input_text)
        assert entry.get_value() == expected


def test_serializing(entry, values):
    for expected, input_value in values:
        entry.set_value(input_value)
        assert entry.serialize_to_text() == expected


def test_different_type_assignments(entry):
    for incorrect_value in ['123', True, 2.718]:
        with pytest.raises(ValueError):
            entry.set_value(incorrect_value)


def test_if_returned_value_behaves_like_int(entry):
    entry.set_value(5)
    value = entry.get_value()

    # Perform few normal int operations
    assert value + 2 == 7
    assert value < 10
    assert 2 ** value == 32
    assert bool(value)
    assert str(value) == '5'


def test_if_get_value_returns_copy(entry):
    entry.set_value(5)
    value = entry.get_value()
    value += 5

    assert entry.get_value() == 5
    assert value == 10
