import pytest

from test_library.node_config import NodeConfig


@pytest.fixture
def empty_config():
    return NodeConfig()


def test_getting_unset_but_correct_entry(empty_config):
    assert empty_config.plugin is None


def test_getting_incorrect_entry(empty_config):
    with pytest.raises(AttributeError):
        assert empty_config.incorrect_entry


def test_setting_correct_entry(empty_config):
    empty_config.plugin = 'witness'


def test_setting_incorrect_entry(empty_config):
    with pytest.raises(AttributeError):
        empty_config.incorrect_entry = 'value'
