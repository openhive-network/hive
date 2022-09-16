import re
from typing import Dict

import test_tools as tt

from .local_tools import are_close, calculate_price, create_buy_order, create_sell_order, verify_json_patterns,\
    verify_text_patterns

INITIAL_ORDERS = [
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
for initial_order in INITIAL_ORDERS:
    initial_order['price'] = \
        calculate_price(initial_order['min_to_receive'].amount, initial_order['amount_to_sell'].amount)


def test_get_open_orders_json_format(node, wallet_with_json_formatter):
    for initial_order in INITIAL_ORDERS:
        if initial_order['type'] == 'BUY':
            create_buy_order(wallet_with_json_formatter, 'initminer', initial_order['min_to_receive'],
                             initial_order['amount_to_sell'], initial_order['id'])
        else:
            create_sell_order(wallet_with_json_formatter, 'initminer', initial_order['amount_to_sell'],
                              initial_order['min_to_receive'], initial_order['id'])

    open_orders = wallet_with_json_formatter.api.get_open_orders('initminer')

    for order, initial_order in zip(open_orders['orders'], INITIAL_ORDERS):
        assert order['id'] == initial_order['id']
        assert order['type'] == initial_order['type']
        assert are_close(float(order['price']), initial_order['price'])
        assert order['quantity'] == (initial_order['amount_to_sell']).as_nai()


def test_get_open_orders_text_format(node, wallet_with_text_formatter):
    for initial_order in INITIAL_ORDERS:
        if initial_order['type'] == 'BUY':
            create_buy_order(wallet_with_text_formatter, 'initminer', initial_order['min_to_receive'],
                             initial_order['amount_to_sell'], initial_order['id'])
        else:
            create_sell_order(wallet_with_text_formatter, 'initminer', initial_order['amount_to_sell'],
                              initial_order['min_to_receive'], initial_order['id'])

    open_orders = parse_text_response(wallet_with_text_formatter.api.get_open_orders('initminer'))

    for order, initial_order in zip(open_orders, INITIAL_ORDERS):
        assert int(order['id']) == initial_order['id']
        assert order['type'] == initial_order['type']
        assert are_close(float(order['price']), float(initial_order['price']))
        assert order['quantity'] == initial_order['amount_to_sell']


def test_json_format_pattern(node, wallet_with_json_formatter):
    for initial_order in INITIAL_ORDERS:
        if initial_order['type'] == 'BUY':
            create_buy_order(wallet_with_json_formatter, 'initminer', initial_order['min_to_receive'],
                             initial_order['amount_to_sell'], initial_order['id'])
        else:
            create_sell_order(wallet_with_json_formatter, 'initminer', initial_order['amount_to_sell'],
                              initial_order['min_to_receive'], initial_order['id'])

    open_orders = wallet_with_json_formatter.api.get_open_orders('initminer')

    verify_json_patterns('get_open_orders', open_orders)


def test_text_format_pattern(node, wallet_with_text_formatter):
    for initial_order in INITIAL_ORDERS:
        if initial_order['type'] == 'BUY':
            create_buy_order(wallet_with_text_formatter, 'initminer', initial_order['min_to_receive'],
                             initial_order['amount_to_sell'], initial_order['id'])
        else:
            create_sell_order(wallet_with_text_formatter, 'initminer', initial_order['amount_to_sell'],
                              initial_order['min_to_receive'], initial_order['id'])

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
