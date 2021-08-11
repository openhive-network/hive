# These tests checks if access to nonexistent keys in config are reported as errors.
#
# For example user should write:
#   config.witness = 'my-new-witness'
# but, might by accident write (note double 'n' in 'witness'):
#   config.witnness = 'my-new-witness'
#
# Then exception will be raised and error will be immediately noticed.

import pytest


def test_getting_unset_but_correct_entry(config):
    assert config.required_participation is None


def test_getting_incorrect_entry(config):
    with pytest.raises(KeyError):
        assert config.incorrect_entry


def test_setting_correct_entry(config):
    config.required_participation = 33


def test_setting_incorrect_entry(config):
    with pytest.raises(KeyError):
        config.incorrect_entry = 'value'
