import pytest

from test_tools.node_config_entry_types import List, Integer, String, Untouched


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


def test_setting_incorrect_value_type():
    items = List(Integer)
    for incorrect_value in [None, True, '1', ['1'], [1, 2, '3'], [None], [1, 2, None]]:
        with pytest.raises(ValueError):
            items.set_value(incorrect_value)
            pytest.fail(f'Exception was not raised for value: {incorrect_value}')


def test_setting_single_integer():
    items = List(Integer)
    items.set_value(5)  # Note: it is not [5], but 5
    assert items.get_value() == [5]


def test_setting_single_string():
    items = List(String)
    items.set_value('text')  # Note: it is not ['text'], but 'text'
    assert items.get_value() == ['text']
