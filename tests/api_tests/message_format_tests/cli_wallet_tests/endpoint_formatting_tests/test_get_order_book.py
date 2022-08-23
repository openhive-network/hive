from __future__ import annotations

import re
from typing import Dict, List, Literal, Union

import test_tools as tt

from .local_tools import are_close, create_buy_order, create_sell_order, verify_json_patterns, verify_text_patterns
from .....local_tools import create_account_and_fund_it


def test_get_order_book_json_format(node, wallet_with_json_formatter):
    initial_orders = prepare_accounts_and_orders(wallet_with_json_formatter)

    order_book = wallet_with_json_formatter.api.get_order_book(len(initial_orders))

    assert_that_bids_are_equal(order_book['bids'], initial_orders[0:2], 'hf26')
    assert_that_asks_are_equal(order_book['asks'], initial_orders[2:4], 'hf26')

    assert order_book['bid_total'] == sum([order['amount_to_sell'] for order in initial_orders[0:2]], tt.Asset.Tbd(0))
    assert order_book['ask_total'] == sum([order['min_to_receive'] for order in initial_orders[2:4]], tt.Asset.Tbd(0))


def test_get_order_book_text_format(node, wallet_with_text_formatter):
    initial_orders = prepare_accounts_and_orders(wallet_with_text_formatter)

    order_book = parse_text_response(wallet_with_text_formatter.api.get_order_book(len(initial_orders)))

    assert_that_bids_are_equal(order_book['bids'], initial_orders[0:2], 'legacy')
    assert_that_asks_are_equal(order_book['asks'], initial_orders[2:4], 'legacy')

    assert order_book['bid_total'] == sum([order['amount_to_sell'] for order in initial_orders[0:2]], tt.Asset.Tbd(0))
    assert order_book['ask_total'] == sum([order['min_to_receive'] for order in initial_orders[2:4]], tt.Asset.Tbd(0))


def test_json_format_pattern(node, wallet_with_json_formatter):
    initial_orders = prepare_accounts_and_orders(wallet_with_json_formatter)

    order_book = wallet_with_json_formatter.api.get_order_book(len(initial_orders))

    verify_json_patterns('get_order_book', order_book)


def test_text_format_pattern(node, wallet_with_text_formatter):
    initial_orders = prepare_accounts_and_orders(wallet_with_text_formatter)

    order_book = wallet_with_text_formatter.api.get_order_book(len(initial_orders))

    verify_text_patterns('get_order_book', order_book)


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
    for account in ['alice', 'bob', 'carol', 'dan']:
        create_account_and_fund_it(wallet, account, tests=tt.Asset.Test(1000000),
                                   tbds=tt.Asset.Tbd(1000000), vests=tt.Asset.Test(1000000))

    with wallet.in_single_transaction():
        buy_order0 = create_buy_order(wallet, 'alice', tt.Asset.Test(100), tt.Asset.Tbd(20), 0)
        buy_order1 = create_buy_order(wallet, 'bob', tt.Asset.Test(300), tt.Asset.Tbd(50), 1)
        sell_order0 = create_sell_order(wallet, 'carol', tt.Asset.Test(100), tt.Asset.Tbd(40), 2)
        sell_order1 = create_sell_order(wallet, 'dan', tt.Asset.Test(100), tt.Asset.Tbd(50), 3)

    return [buy_order0, buy_order1, sell_order0, sell_order1]


def assert_that_bids_are_equal(orders_bids: Dict, reference_orders_bids: List,
                               asset_format: Literal['hf26', 'legacy']) -> None:
    sum_hbd_from_bids = tt.Asset.Tbd(0)
    for order, reference_order in zip(orders_bids, reference_orders_bids):
        sum_hbd_from_bids += reference_order['amount_to_sell']

        assert order['hive'] == __serialize_asset(reference_order['min_to_receive'], asset_format)
        assert order['hbd'] == __serialize_asset(reference_order['amount_to_sell'], asset_format)
        assert order['sum_hbd'] == __serialize_asset(sum_hbd_from_bids, asset_format)
        assert are_close(float(order['price']), reference_order['price'])


def assert_that_asks_are_equal(orders_asks: Dict, reference_orders_asks: List,
                               asset_format: Literal['hf26', 'legacy']) -> None:
    sum_hbd_from_asks = tt.Asset.Tbd(0)
    for order, reference_order in zip(orders_asks, reference_orders_asks):
        sum_hbd_from_asks += reference_order['min_to_receive']
        assert order['hive'] == __serialize_asset(reference_order['amount_to_sell'], asset_format)
        assert order['hbd'] == __serialize_asset(reference_order['min_to_receive'], asset_format)
        assert order['sum_hbd'] == __serialize_asset(sum_hbd_from_asks, asset_format)
        assert are_close(float(order['price']), reference_order['price'])


def __serialize_asset(asset: tt.AnyAsset, asset_format: Literal['hf26', 'legacy']) -> Union[str, Dict]:
    return asset.as_nai() if asset_format == 'hf26' else str(asset)
