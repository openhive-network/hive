import pytest

from test_tools.exceptions import NotSupported


def test_addition_operator_is_removed(config):
    with pytest.raises(NotSupported):
        config.witness += ['initminer']

    with pytest.raises(NotSupported):
        config.witness += 'initminer'


def test_adding_item_to_empty_list(config):
    config.witness.append('A')
    assert config.witness == ['A']


def test_adding_multiple_items_in_single_line(config):
    config.private_key.extend(['1', '2', '3'])
    assert config.private_key == ['1', '2', '3']


def test_adding_multiple_items_in_few_lines(config):
    config.private_key.append('1')
    config.private_key.append('2')
    config.private_key.append('3')
    assert config.private_key == ['1', '2', '3']


def test_setting_string_instead_of_list(config):
    config.private_key = '5KFirstQmyBxGKqXCv5qRhip'
    assert config.private_key == ['5KFirstQmyBxGKqXCv5qRhip']


def test_adding_first_string_instead_of_list(config):
    config.private_key.append('5KFirstQmyBxGKqXCv5qRhip')
    assert config.private_key == ['5KFirstQmyBxGKqXCv5qRhip']


def test_adding_second_string_instead_of_list(config):
    config.private_key = ['5KFirstQmyBxGKqXCv5qRhip']
    config.private_key.append('5KSecondXPdzYqB8d6S66bup')
    assert config.private_key == ['5KFirstQmyBxGKqXCv5qRhip', '5KSecondXPdzYqB8d6S66bup']


def test_remove_item_from_list(config):
    config.plugin = ['witness', 'account_by_key', 'account_by_key_api', 'condenser_api']
    config.plugin.remove('witness')
    assert config.plugin == ['account_by_key', 'account_by_key_api', 'condenser_api']
