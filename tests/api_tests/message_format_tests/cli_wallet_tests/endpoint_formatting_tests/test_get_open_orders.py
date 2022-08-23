from math import isclose
from typing import List

import test_tools as tt

from .local_tools import verify_json_patterns, verify_text_patterns, split_text_to_separated_words

ORDER_INITIAL_VALUES = [
    {
        'id': 0,
        'amount_to_sell': tt.Asset.Test(11.534),
        'min_to_receive': tt.Asset.Tbd(6.591),
        'type': 'SELL',
    },
    {
        'id': 1,
        'amount_to_sell': tt.Asset.Test(12.175),
        'min_to_receive': tt.Asset.Tbd(7.010),
        'type': 'SELL',
    },
    {
        'id': 2,
        'amount_to_sell': tt.Asset.Tbd(0.907),
        'min_to_receive': tt.Asset.Test(1.597),
        'type': 'BUY',
    },
]
for order_initial_value in ORDER_INITIAL_VALUES:
    order_initial_value['price'] = \
        order_initial_value['min_to_receive'].amount / order_initial_value['amount_to_sell'].amount


def test_get_open_orders_json_format(node, wallet_with_json_formatter):
    for order_initial_value in ORDER_INITIAL_VALUES:
        wallet_with_json_formatter.api.create_order('initminer', order_initial_value['id'],
                                                    order_initial_value['amount_to_sell'],
                                                    order_initial_value['min_to_receive'], False, 3600)

    json = wallet_with_json_formatter.api.get_open_orders('initminer')

    for order_json, value in zip(json['orders'], ORDER_INITIAL_VALUES):
        assert order_json['id'] == value['id']
        assert order_json['type'] == value['type']
        assert isclose(float(order_json['price']), value['price'])
        assert order_json['quantity'] == (value['amount_to_sell']).as_nai()


def test_get_open_orders_text_format(node, wallet_with_text_formatter):
    for order_initial_value in ORDER_INITIAL_VALUES:
        wallet_with_text_formatter.api.create_order('initminer', order_initial_value['id'],
                                                    order_initial_value['amount_to_sell'],
                                                    order_initial_value['min_to_receive'], False, 3600)

    text_parsed_to_list = parse_text_response(wallet_with_text_formatter.api.get_open_orders('initminer'))

    for order_text, value in zip(text_parsed_to_list, ORDER_INITIAL_VALUES):
        assert int(order_text['id']) == value['id']
        assert order_text['type'] == value['type']
        assert isclose(float(order_text['price']), float(value['price']))
        assert order_text['quantity'] == value['amount_to_sell']


def test_json_format_pattern(node, wallet_with_json_formatter):
    for order_initial_value in ORDER_INITIAL_VALUES:
        wallet_with_json_formatter.api.create_order('initminer', order_initial_value['id'],
                                                    order_initial_value['amount_to_sell'],
                                                    order_initial_value['min_to_receive'], False, 3600)

    json = wallet_with_json_formatter.api.get_open_orders('initminer')

    verify_json_patterns('get_open_orders', json)


def test_text_format_pattern(node, wallet_with_text_formatter):
    for order_initial_value in ORDER_INITIAL_VALUES:
        wallet_with_text_formatter.api.create_order('initminer', order_initial_value['id'],
                                                    order_initial_value['amount_to_sell'],
                                                    order_initial_value['min_to_receive'], False, 3600)

    text = wallet_with_text_formatter.api.get_open_orders('initminer')

    verify_text_patterns('get_open_orders', text)


def parse_text_response(text):
    lines: List[List[str]] = split_text_to_separated_words(text)
    open_orders = []
    for words in lines[2:5]:
        open_orders.append({
            'id': words[0],
            'price': words[1],
            'quantity': words[2],
            'type': words[3],
        })

    return open_orders
