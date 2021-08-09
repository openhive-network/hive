import pytest

from test_tools.private.node_config_entry_types import Boolean


@pytest.fixture
def data_sets():
    return [
        {'entry': Boolean(),                       True: '1',   False: '0'},
        {'entry': Boolean(true='yes', false='no'), True: 'yes', False: 'no'},
    ]


def test_parsing(data_sets):
    for data_set in data_sets:
        for expected in (True, False):
            entry = data_set['entry']
            value_as_text = data_set[expected]

            entry.parse_from_text(value_as_text)
            assert entry.get_value() == expected


def test_serializing(data_sets):
    for data_set in data_sets:
        for input_value in (True, False):
            entry = data_set['entry']
            expected = data_set[input_value]

            entry.set_value(input_value)
            assert entry.serialize_to_text() == expected


def test_different_type_assignments():
    for incorrect_value in ['123', 0, 1, 2.718]:
        with pytest.raises(ValueError):
            Boolean().set_value(incorrect_value)
