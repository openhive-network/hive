from __future__ import annotations

import test_tools as tt
from hive_local_tools import run_for

from .utilities import check_ask, check_sell_price


@run_for("testnet", enable_plugins=["market_history_api"])
def test_order(wallet: tt.Wallet) -> None:
    wallet.api.create_account("initminer", "alice", "{}")

    wallet.api.transfer("initminer", "alice", tt.Asset.Test(77), "lime")

    wallet.api.transfer_to_vesting("initminer", "alice", tt.Asset.Test(500))

    response = wallet.api.create_order("alice", 666, tt.Asset.Test(7), tt.Asset.Tbd(1), False, 3600)

    _ops = response["operations"]
    assert _ops[0][1]["amount_to_sell"] == tt.Asset.Test(7)
    assert _ops[0][1]["min_to_receive"] == tt.Asset.Tbd(1)

    wallet.api.create_order("alice", 667, tt.Asset.Test(8), tt.Asset.Tbd(2), False, 3600)

    response = wallet.api.get_order_book(5)

    _asks = response["asks"]
    assert len(_asks) == 2
    check_ask(_asks[0], tt.Asset.Test(7), tt.Asset.Tbd(1))
    check_ask(_asks[1], tt.Asset.Test(8), tt.Asset.Tbd(2))

    _result = wallet.api.get_open_orders("alice")
    assert len(_result) == 2
    check_sell_price(_result[0], tt.Asset.Test(7), tt.Asset.Tbd(1))
    check_sell_price(_result[1], tt.Asset.Test(8), tt.Asset.Tbd(2))

    response = wallet.api.cancel_order("alice", 667)

    assert len(response["operations"]) == 1

    response = wallet.api.get_order_book(5)

    _asks = response["asks"]
    assert len(_asks) == 1

    _result = wallet.api.get_open_orders("alice")
    assert len(_result) == 1
    check_sell_price(_result[0], tt.Asset.Test(7), tt.Asset.Tbd(1))
