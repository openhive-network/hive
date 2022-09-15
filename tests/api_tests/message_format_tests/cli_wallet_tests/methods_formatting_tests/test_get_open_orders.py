import re
from typing import Dict

import test_tools as tt

from .local_tools import are_close, calculate_price, create_buy_order, create_sell_order, verify_json_patterns,\
    verify_text_patterns

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
        calculate_price(order_initial_value['min_to_receive'].amount, order_initial_value['amount_to_sell'].amount)


def test_get_open_orders_json_format(node, wallet_with_json_formatter):
    for order_initial_value in ORDER_INITIAL_VALUES:
        if order_initial_value['type'] == 'BUY':
            create_buy_order(wallet_with_json_formatter, 'initminer', order_initial_value['min_to_receive'],
                             order_initial_value['amount_to_sell'], order_initial_value['id'])
        else:
            create_sell_order(wallet_with_json_formatter, 'initminer', order_initial_value['amount_to_sell'],
                              order_initial_value['min_to_receive'], order_initial_value['id'])

    open_orders = wallet_with_json_formatter.api.get_open_orders('initminer')

    for order, order_initial_value in zip(open_orders['orders'], ORDER_INITIAL_VALUES):
        assert order['id'] == order_initial_value['id']
        assert order['type'] == order_initial_value['type']
        assert are_close(float(order['price']), order_initial_value['price'])
        assert order['quantity'] == (order_initial_value['amount_to_sell']).as_nai()


def test_get_open_orders_text_format(node, wallet_with_text_formatter):
    for order_initial_value in ORDER_INITIAL_VALUES:
        if order_initial_value['type'] == 'BUY':
            create_buy_order(wallet_with_text_formatter, 'initminer', order_initial_value['min_to_receive'],
                             order_initial_value['amount_to_sell'], order_initial_value['id'])
        else:
            create_sell_order(wallet_with_text_formatter, 'initminer', order_initial_value['amount_to_sell'],
                              order_initial_value['min_to_receive'], order_initial_value['id'])

    open_orders = parse_text_response(wallet_with_text_formatter.api.get_open_orders('initminer'))

    for order, order_initial_value in zip(open_orders, ORDER_INITIAL_VALUES):
        assert int(order['id']) == order_initial_value['id']
        assert order['type'] == order_initial_value['type']
        assert are_close(float(order['price']), float(order_initial_value['price']))
        assert order['quantity'] == order_initial_value['amount_to_sell']


def test_json_format_pattern(node, wallet_with_json_formatter):
    for order_initial_value in ORDER_INITIAL_VALUES:
        if order_initial_value['type'] == 'BUY':
            create_buy_order(wallet_with_json_formatter, 'initminer', order_initial_value['min_to_receive'],
                             order_initial_value['amount_to_sell'], order_initial_value['id'])
        else:
            create_sell_order(wallet_with_json_formatter, 'initminer', order_initial_value['amount_to_sell'],
                              order_initial_value['min_to_receive'], order_initial_value['id'])

    open_orders = wallet_with_json_formatter.api.get_open_orders('initminer')

    verify_json_patterns('get_open_orders', open_orders)


def test_text_format_pattern(node, wallet_with_text_formatter):
    for order_initial_value in ORDER_INITIAL_VALUES:
        if order_initial_value['type'] == 'BUY':
            create_buy_order(wallet_with_text_formatter, 'initminer', order_initial_value['min_to_receive'],
                             order_initial_value['amount_to_sell'], order_initial_value['id'])
        else:
            create_sell_order(wallet_with_text_formatter, 'initminer', order_initial_value['amount_to_sell'],
                              order_initial_value['min_to_receive'], order_initial_value['id'])

    open_orders = wallet_with_text_formatter.api.get_open_orders('initminer')

    verify_text_patterns('get_open_orders', open_orders)


def parse_text_response(text):
    def parse_single_line_with_order_values(line_to_parse: str) -> Dict:
        splitted_values = re.split(r'\s{2,}', line_to_parse.strip())
        return {
            'id': splitted_values[0],
            'price': splitted_values[1],
            'quantity': splitted_values[2],
            'type': splitted_values[3],
        }

    lines = text.splitlines()
    return [parse_single_line_with_order_values(line_to_parse) for line_to_parse in lines[2:5]]
