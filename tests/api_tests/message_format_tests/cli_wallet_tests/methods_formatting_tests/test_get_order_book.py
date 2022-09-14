import re
from typing import Dict, List

import test_tools as tt

from .local_tools import are_close, verify_json_patterns, verify_text_patterns
from .....local_tools import create_account_and_fund_it


def calculate_price(amount_1, amount_2):
    return min(amount_1, amount_2) / max(amount_1, amount_2)


ORDER_INITIAL_VALUES = [
    {
        'name': 'alice',
        'id': 0,
        'amount_to_sell': tt.Asset.Tbd(20),
        'min_to_receive': tt.Asset.Test(100),
        'type': 'SELL',
    },
    {
        'name': 'bob',
        'id': 1,
        'amount_to_sell': tt.Asset.Tbd(50),
        'min_to_receive': tt.Asset.Test(300),
        'type': 'SELL',
    },
    {
        'name': 'carol',
        'id': 2,
        'amount_to_sell': tt.Asset.Test(100),
        'min_to_receive': tt.Asset.Tbd(40),
        'type': 'SELL',
    },
    {
        'name': 'dan',
        'id': 3,
        'amount_to_sell': tt.Asset.Test(100),
        'min_to_receive': tt.Asset.Tbd(50),
        'type': 'SELL',
    },
]

for order_initial_value in ORDER_INITIAL_VALUES:
    order_initial_value['price'] = \
        calculate_price(order_initial_value['min_to_receive'].amount, order_initial_value['amount_to_sell'].amount)


def test_get_order_book_json_format(node, wallet_with_json_formatter):
    prepare_accounts_and_orders(wallet_with_json_formatter)

    json = wallet_with_json_formatter.api.get_order_book(len(ORDER_INITIAL_VALUES))
    sum_hbd_from_bids = tt.Asset.Tbd(0)
    for order_value, order_initial_value in zip(json['bids'], ORDER_INITIAL_VALUES[0:2]):
        sum_hbd_from_bids = sum_hbd_from_bids + order_initial_value['amount_to_sell']
        assert order_value['hive'] == order_initial_value['min_to_receive'].as_nai()
        assert order_value['hbd'] == order_initial_value['amount_to_sell'].as_nai()
        assert order_value['sum_hbd'] == sum_hbd_from_bids.as_nai()
        assert are_close(float(order_value['price']), order_initial_value['price'])

    sum_hbd_from_asks = tt.Asset.Tbd(0)
    for order_value, order_initial_value in zip(json['asks'], ORDER_INITIAL_VALUES[2:4]):
        sum_hbd_from_asks = sum_hbd_from_asks + order_initial_value['min_to_receive']
        assert order_value['hive'] == order_initial_value['amount_to_sell'].as_nai()
        assert order_value['hbd'] == order_initial_value['min_to_receive'].as_nai()
        assert order_value['sum_hbd'] == sum_hbd_from_asks.as_nai()
        assert are_close(float(order_value['price']), order_initial_value['price'])

    assert json['bid_total'] == sum_hbd_from_bids.as_nai()
    assert json['ask_total'] == sum_hbd_from_asks.as_nai()


def test_get_order_book_text_format(node, wallet_with_text_formatter):
    prepare_accounts_and_orders(wallet_with_text_formatter)

    text_parsed_to_dict = parse_text_response(wallet_with_text_formatter.api.get_order_book(len(ORDER_INITIAL_VALUES)))

    sum_hbd_from_bids = tt.Asset.Tbd(0)
    for order_value, order_initial_value in zip(text_parsed_to_dict['bids'], ORDER_INITIAL_VALUES[0:2]):
        sum_hbd_from_bids = sum_hbd_from_bids + order_initial_value['amount_to_sell']
        assert order_value['hive'] == order_initial_value['min_to_receive']
        assert order_value['hbd'] == order_initial_value['amount_to_sell']
        assert order_value['sum_hbd'] == sum_hbd_from_bids
        assert are_close(float(order_value['price']), order_initial_value['price'])

    sum_hbd_from_asks = tt.Asset.Tbd(0)
    for order_value, order_initial_value in zip(text_parsed_to_dict['asks'], ORDER_INITIAL_VALUES[2:4]):
        sum_hbd_from_asks = sum_hbd_from_asks + order_initial_value['min_to_receive']
        assert order_value['hive'] == order_initial_value['amount_to_sell']
        assert order_value['hbd'] == order_initial_value['min_to_receive']
        assert order_value['sum_hbd'] == sum_hbd_from_asks
        assert are_close(float(order_value['price']), order_initial_value['price'])

    assert text_parsed_to_dict['bid_total'] == sum_hbd_from_bids
    assert text_parsed_to_dict['ask_total'] == sum_hbd_from_asks


def test_json_format_pattern(node, wallet_with_json_formatter):
    prepare_accounts_and_orders(wallet_with_json_formatter)

    json = wallet_with_json_formatter.api.get_order_book(len(ORDER_INITIAL_VALUES))

    verify_json_patterns('get_order_book', json)


def test_text_format_pattern(node, wallet_with_text_formatter):
    prepare_accounts_and_orders(wallet_with_text_formatter)

    text = wallet_with_text_formatter.api.get_order_book(len(ORDER_INITIAL_VALUES))

    verify_text_patterns('get_order_book', text)


def parse_text_response(text):
    def parse_lines_with_bid_and_ask(lines_to_parse: List) -> Dict:
        bids = []
        asks = []
        for line_to_parse in lines_to_parse:
            splitted_values = re.split(r'\s*\|\s*|\s{2,}', line_to_parse.strip())

            bids.append({
                'hive': splitted_values[2],
                'hbd': splitted_values[1],
                'sum_hbd': splitted_values[0],
                'price': splitted_values[3],
            })

            asks.append({
                'hive': splitted_values[5],
                'hbd': splitted_values[6],
                'sum_hbd': splitted_values[7],
                'price': splitted_values[4],
            })

        return {
            'bids': bids,
            'asks': asks,
        }

    def parse_total_bids_and_asks(line_with_bids_summary: str, line_with_asks_summary: str) -> Dict:
        return {
            'bid_total': line_with_bids_summary.split(':')[1].strip(),
            'ask_total': line_with_asks_summary.split(':')[1].strip(),
        }

    lines = text.splitlines()
    return {
        **parse_lines_with_bid_and_ask(lines[3:5]),
        **parse_total_bids_and_asks(lines[-2], lines[-1]),
    }


def prepare_accounts_and_orders(wallet):
    for order_initial_value in ORDER_INITIAL_VALUES:
        create_account_and_fund_it(wallet, order_initial_value['name'],
                                   tests=tt.Asset.Test(1000000),
                                   tbds=tt.Asset.Tbd(1000000), vests=tt.Asset.Test(1000000))
        wallet.api.create_order(order_initial_value['name'], order_initial_value['id'],
                                order_initial_value['amount_to_sell'],
                                order_initial_value['min_to_receive'], False, 3600)
