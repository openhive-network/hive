from __future__ import annotations

import re
from pathlib import Path
from typing import Any, Literal

import test_tools as tt
from hive_local_tools.api.message_format.cli_wallet import verify_json_patterns, verify_text_patterns
from hive_local_tools.api.message_format.cli_wallet.output_formater import (
    are_close,
    create_buy_order,
    create_sell_order,
)

__PATTERNS_DIRECTORY = Path(__file__).with_name("response_patterns")


def test_get_order_book_json_format(node: tt.InitNode | tt.FullApiNode, wallet_with_json_formatter: tt.Wallet):
    initial_orders = prepare_accounts_and_orders(wallet_with_json_formatter)

    order_book = wallet_with_json_formatter.api.get_order_book(len(initial_orders))

    assert_that_bids_are_equal(order_book["bids"], initial_orders[0:2], "hf26")
    assert_that_asks_are_equal(order_book["asks"], initial_orders[2:4], "hf26")

    assert tt.Asset.is_same(
        order_book["bid_total"], sum([order["amount_to_sell"] for order in initial_orders[0:2]], tt.Asset.Tbd(0))
    )
    assert tt.Asset.is_same(
        order_book["ask_total"], sum([order["min_to_receive"] for order in initial_orders[2:4]], tt.Asset.Tbd(0))
    )


def test_get_order_book_text_format(node: tt.InitNode | tt.FullApiNode, wallet_with_text_formatter: tt.Wallet):
    initial_orders = prepare_accounts_and_orders(wallet_with_text_formatter)

    order_book = parse_text_response(wallet_with_text_formatter.api.get_order_book(len(initial_orders)))

    assert_that_bids_are_equal(order_book["bids"], initial_orders[0:2], "legacy")
    assert_that_asks_are_equal(order_book["asks"], initial_orders[2:4], "legacy")

    assert tt.Asset.is_same(
        order_book["bid_total"], sum([order["amount_to_sell"] for order in initial_orders[0:2]], tt.Asset.Tbd(0))
    )
    assert tt.Asset.is_same(
        order_book["ask_total"], sum([order["min_to_receive"] for order in initial_orders[2:4]], tt.Asset.Tbd(0))
    )


def test_json_format_pattern(node: tt.InitNode | tt.FullApiNode, wallet_with_json_formatter: tt.Wallet):
    initial_orders = prepare_accounts_and_orders(wallet_with_json_formatter)

    order_book = wallet_with_json_formatter.api.get_order_book(len(initial_orders))

    verify_json_patterns(__PATTERNS_DIRECTORY, "get_order_book", order_book)


def test_text_format_pattern(node: tt.InitNode | tt.FullApiNode, wallet_with_text_formatter: tt.Wallet):
    initial_orders = prepare_accounts_and_orders(wallet_with_text_formatter)

    order_book = wallet_with_text_formatter.api.get_order_book(len(initial_orders))

    verify_text_patterns(__PATTERNS_DIRECTORY, "get_order_book", order_book)


def parse_text_response(text: str) -> dict[str, Any]:
    def parse_lines_with_bid_and_ask(lines_to_parse: list) -> dict:
        bids = []
        asks = []
        for line_to_parse in lines_to_parse:
            splitted_values = re.split(r"\s*\|\s*|\s{2,}", line_to_parse.strip())

            bids.append(
                {
                    "hive": splitted_values[2],
                    "hbd": splitted_values[1],
                    "sum_hbd": splitted_values[0],
                    "price": splitted_values[3],
                }
            )

            asks.append(
                {
                    "hive": splitted_values[5],
                    "hbd": splitted_values[6],
                    "sum_hbd": splitted_values[7],
                    "price": splitted_values[4],
                }
            )

        return {
            "bids": bids,
            "asks": asks,
        }

    def parse_total_bids_and_asks(line_with_bids_summary: str, line_with_asks_summary: str) -> dict:
        return {
            "bid_total": line_with_bids_summary.split(":")[1].strip(),
            "ask_total": line_with_asks_summary.split(":")[1].strip(),
        }

    lines = text.splitlines()
    return {
        **parse_lines_with_bid_and_ask(lines[3:5]),
        **parse_total_bids_and_asks(lines[-2], lines[-1]),
    }


def prepare_accounts_and_orders(wallet: tt.Wallet):
    for account in ["alice", "bob", "carol", "dan"]:
        wallet.create_account(
            account, hives=tt.Asset.Test(1000000), hbds=tt.Asset.Tbd(1000000), vests=tt.Asset.Test(1000000)
        )

    with wallet.in_single_transaction():
        buy_order0 = create_buy_order(wallet, "alice", tt.Asset.Test(100), tt.Asset.Tbd(20), 0)
        buy_order1 = create_buy_order(wallet, "bob", tt.Asset.Test(300), tt.Asset.Tbd(50), 1)
        sell_order0 = create_sell_order(wallet, "carol", tt.Asset.Test(100), tt.Asset.Tbd(40), 2)
        sell_order1 = create_sell_order(wallet, "dan", tt.Asset.Test(100), tt.Asset.Tbd(50), 3)

    return [buy_order0, buy_order1, sell_order0, sell_order1]


def assert_that_bids_are_equal(
    orders_bids: dict, reference_orders_bids: list, asset_format: Literal["hf26", "legacy"]
) -> None:
    sum_hbd_from_bids = tt.Asset.Tbd(0)
    for order, reference_order in zip(orders_bids, reference_orders_bids):
        sum_hbd_from_bids += reference_order["amount_to_sell"]

        assert tt.Asset.is_same(order["hive"], __serialize_asset(reference_order["min_to_receive"], asset_format))
        assert tt.Asset.is_same(order["hbd"], __serialize_asset(reference_order["amount_to_sell"], asset_format))
        assert tt.Asset.is_same(order["sum_hbd"], __serialize_asset(sum_hbd_from_bids, asset_format))
        assert are_close(float(order["price"]), reference_order["price"])


def assert_that_asks_are_equal(
    orders_asks: dict, reference_orders_asks: list, asset_format: Literal["hf26", "legacy"]
) -> None:
    sum_hbd_from_asks = tt.Asset.Tbd(0)
    for order, reference_order in zip(orders_asks, reference_orders_asks):
        sum_hbd_from_asks += reference_order["min_to_receive"]
        assert tt.Asset.is_same(order["hive"], __serialize_asset(reference_order["amount_to_sell"], asset_format))
        assert tt.Asset.is_same(order["hbd"], __serialize_asset(reference_order["min_to_receive"], asset_format))
        assert tt.Asset.is_same(order["sum_hbd"], __serialize_asset(sum_hbd_from_asks, asset_format))
        assert are_close(float(order["price"]), reference_order["price"])


def __serialize_asset(asset: tt.Asset.AnyT, asset_format: Literal["hf26", "legacy"]) -> str | dict:
    return asset.as_nai() if asset_format == "hf26" else asset.as_legacy()
