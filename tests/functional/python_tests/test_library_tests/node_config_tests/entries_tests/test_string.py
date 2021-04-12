import pytest

from test_library.node_config_entry_types.string import String


@pytest.fixture
def entry():
    return String()


@pytest.fixture
def values():
    return [
        ('"example"', 'example'),
    ]


def test_parsing(entry, values):
    for input_text, expected in values:
        entry.parse_from_text(input_text)
        assert entry.value == expected


def test_serializing(entry, values):
    for expected, input_value in values:
        entry.value = input_value
        assert entry.serialize_to_text() == expected
