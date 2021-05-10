import pytest

from test_library.node_config_entry_types import *


def test_parsing_single_line_of_untouched():
    expected_items = ['network_node_api', 'account_by_key', 'witness', 'network_broadcast_api', 'condenser_api']

    items = List(Untouched)
    items.parse_from_text(' '.join(expected_items))

    assert items.get_value() == expected_items


def test_serializing_single_line_of_untouched():
    input_items = ['network_node_api', 'account_by_key', 'witness', 'network_broadcast_api', 'condenser_api']

    items = List(Untouched)
    items.set_value(input_items)

    serialized = items.serialize_to_text()

    assert serialized == ' '.join(input_items)


def test_parsing_single_line_of_integers():
    input_text = '[15,60,300,3600,86400]'
    expected_items = [15, 60, 300, 3600, 86400]

    items = List(Integer, begin='[', separator=',', end=']')
    items.parse_from_text(input_text)

    assert items.get_value() == expected_items


def test_serializing_single_line_of_integers():
    input_items = [15, 60, 300, 3600, 86400]
    expected = '[15,60,300,3600,86400]'

    items = List(Integer, begin='[', separator=',', end=']')
    items.set_value(input_items)

    assert items.serialize_to_text() == expected


def test_serializing_multiple_lines_of_strings():
    input_items = ['initminer', 'blocktrades', 'gtg', 'good-karma']
    expected = [f'"{item}"' for item in input_items]

    items = List(String, single_line=False)
    items.set_value(input_items)

    assert items.serialize_to_text() == expected


def test_if_value_is_set_after_append():
    items = List(Integer)
    items.get_value().append(0)
    assert items.get_value().is_set()


def test_if_value_is_set_after_extend():
    items = List(Integer)
    items.get_value().extend([0, 1, 2])
    assert items.get_value().is_set()


def test_if_value_is_not_set_after_remove():
    items = List(Integer)
    items.get_value().append(0)
    items.get_value().remove(0)
    assert not items.get_value().is_set()


def test_clear():
    items = List(Untouched)
    items.get_value().append('x')
    items.get_value().clear()
    assert not items.get_value().is_set()
