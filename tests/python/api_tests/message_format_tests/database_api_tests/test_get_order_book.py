from __future__ import annotations

import test_tools as tt
from hive_local_tools import run_for


@run_for("testnet", "mainnet_5m", "live_mainnet")
def test_get_order_book(node, should_prepare):
    if should_prepare:
        wallet = tt.Wallet(attach_to=node)
        wallet.create_account("alice", hives=tt.Asset.Test(100), vests=tt.Asset.Test(100))
        wallet.create_account("bob", hives=tt.Asset.Test(100), vests=tt.Asset.Test(100), hbds=tt.Asset.Tbd(100))

        wallet.api.create_order(
            "alice", 0, tt.Asset.Test(100), tt.Asset.Tbd(100), False, 3600
        )  # Sell 100 HIVE for 100 HBD
        wallet.api.create_order("bob", 0, tt.Asset.Tbd(50), tt.Asset.Test(100), False, 3600)  # Buy 100 HIVE for 50 HBD

    response = node.api.database.get_order_book(limit=100)
    assert len(response["asks"]) != 0
    assert len(response["bids"]) != 0
