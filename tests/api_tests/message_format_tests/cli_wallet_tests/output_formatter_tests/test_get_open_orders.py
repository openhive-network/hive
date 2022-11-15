from pathlib import Path
import re
from typing import Dict

import test_tools as tt

from .local_tools import are_close, create_buy_order, create_sell_order
from ..local_tools import verify_json_patterns, verify_text_patterns

__PATTERNS_DIRECTORY = Path(__file__).with_name('response_patterns')


def test_get_open_orders_json_format(node, wallet_with_json_formatter):
    initial_orders = prepare_orders(wallet_with_json_formatter)

    open_orders = wallet_with_json_formatter.api.get_open_orders('initminer')

    for order, initial_order in zip(open_orders['orders'], initial_orders):
        assert order['id'] == initial_order['id']
        assert order['type'] == initial_order['type']
        assert are_close(float(order['price']), initial_order['price'])
        assert order['quantity'] == (initial_order['amount_to_sell']).as_nai()


def test_get_open_orders_text_format(node, wallet_with_text_formatter):
    initial_orders = prepare_orders(wallet_with_text_formatter)

    open_orders = parse_text_response(wallet_with_text_formatter.api.get_open_orders('initminer'))

    for order, initial_order in zip(open_orders, initial_orders):
        assert order['id'] == initial_order['id']
        assert order['type'] == initial_order['type']
        assert are_close(order['price'], initial_order['price'])
        assert order['quantity'] == initial_order['amount_to_sell']


def test_json_format_pattern(node, wallet_with_json_formatter):
    prepare_orders(wallet_with_json_formatter)

    open_orders = wallet_with_json_formatter.api.get_open_orders('initminer')

    verify_json_patterns(__PATTERNS_DIRECTORY, 'get_open_orders', open_orders)


def test_text_format_pattern(node, wallet_with_text_formatter):
    prepare_orders(wallet_with_text_formatter)

    open_orders = wallet_with_text_formatter.api.get_open_orders('initminer')

    verify_text_patterns(__PATTERNS_DIRECTORY, 'get_open_orders', open_orders)


def prepare_orders(wallet):
    with wallet.in_single_transaction():
        sell_order0 = create_sell_order(wallet, 'initminer', tt.Asset.Test(11.534), tt.Asset.Tbd(6.591), 0)
        sell_order1 = create_sell_order(wallet, 'initminer', tt.Asset.Test(12.175), tt.Asset.Tbd(7.010), 1)
        buy_order0 = create_buy_order(wallet, 'initminer', tt.Asset.Test(1.597), tt.Asset.Tbd(0.907), 2)

    return [sell_order0, sell_order1, buy_order0]


def parse_text_response(text):
    def parse_single_line_with_order_values(line_to_parse: str) -> Dict:
        splitted_values = re.split(r'\s{2,}', line_to_parse.strip())
        return {
            'id': int(splitted_values[0]),
            'price': float(splitted_values[1]),
            'quantity': splitted_values[2],
            'type': splitted_values[3],
        }

    lines = text.splitlines()
    return [parse_single_line_with_order_values(line_to_parse) for line_to_parse in lines[2:]]
