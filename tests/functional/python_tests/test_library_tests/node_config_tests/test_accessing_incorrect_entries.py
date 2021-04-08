import pytest

from test_library.node_config import NodeConfig


def test_getting_unset_but_correct_entry():
    config = NodeConfig()
    assert config.plugin is None


def test_getting_incorrect_entry():
    config = NodeConfig()
    with pytest.raises(AttributeError):
        assert config.incorrect_entry


def test_setting_correct_entry():
    config = NodeConfig()
    config.plugin = 'witness'


def test_setting_incorrect_entry():
    config = NodeConfig()
    with pytest.raises(AttributeError):
        config.incorrect_entry = 'value'
