import pytest

from test_tools.node_config_entry_types import Boolean


@pytest.fixture
def entry():
    return Boolean()


@pytest.fixture
def values():
    return [
        ('1', True),
        ('0', False),
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
    for incorrect_value in ['123', 0, 1, 2.718]:
        with pytest.raises(ValueError):
            entry.set_value(incorrect_value)
