import pytest

from test_library.node_config_entry_types import Boolean, Integer, List, String, Untouched


@pytest.fixture
def entries_with_example_values():
    return [
        (Boolean(), True),
        (Integer(), 42),
        (List(Untouched), ['127.0.0.1:65535']),
        (String(), 'initminer'),
        (Untouched(), '127.0.0.1:65535'),
    ]


def test_is_set(entries_with_example_values):
    for entry, value in entries_with_example_values:
        assert not entry.get_value().is_set()

        entry.set_value(value)
        assert entry.get_value().is_set()

        entry.clear()
        assert not entry.get_value().is_set()
