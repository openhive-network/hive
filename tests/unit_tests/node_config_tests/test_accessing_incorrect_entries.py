# These tests checks if access to nonexistent keys in config are reported as errors.
#
# For example user should write:
#   config.witness = 'my-new-witness'
# but, might by accident write (note double 'n' in 'witness'):
#   config.witnness = 'my-new-witness'
#
# Then exception will be raised and error will be immediately noticed.

import pytest

from test_tools.node_config import NodeConfig


@pytest.fixture
def empty_config():
    return NodeConfig()


def test_getting_unset_but_correct_entry(empty_config):
    assert empty_config.required_participation is None


def test_getting_incorrect_entry(empty_config):
    with pytest.raises(KeyError):
        assert empty_config.incorrect_entry


def test_setting_correct_entry(empty_config):
    empty_config.required_participation = 33


def test_setting_incorrect_entry(empty_config):
    with pytest.raises(KeyError):
        empty_config.incorrect_entry = 'value'
