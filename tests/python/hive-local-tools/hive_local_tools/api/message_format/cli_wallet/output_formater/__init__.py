from __future__ import annotations

from math import isclose
from typing import TYPE_CHECKING, Any

if TYPE_CHECKING:
    import test_tools as tt


def are_close(first: float, second: float) -> bool:
    return isclose(first, second, abs_tol=0.0000005)


def create_buy_order(
    wallet: tt.Wallet, account: str, buy: tt.Asset.TestT, offer: tt.Asset.TbdT, id: int
) -> dict[str, Any]:
    wallet.api.create_order(account, id, offer, buy, False, 3600)
    return {
        "name": account,
        "id": id,
        "amount_to_sell": offer,
        "min_to_receive": buy,
        "type": "BUY",
        "price": calculate_price(buy.amount, offer.amount),
    }


def create_sell_order(
    wallet: tt.Wallet, account: str, sell: tt.Asset.TestT, offer: tt.Asset.TbdT, id: int
) -> dict[str, Any]:
    wallet.api.create_order(account, id, sell, offer, False, 3600)
    return {
        "name": account,
        "id": id,
        "amount_to_sell": sell,
        "min_to_receive": offer,
        "type": "SELL",
        "price": calculate_price(sell.amount, offer.amount),
    }


def calculate_price(amount_1: int, amount_2: int):
    return min(int(amount_1), int(amount_2)) / max(int(amount_1), int(amount_2))
