from __future__ import annotations

import test_tools as tt
from hive_local_tools import run_for


@run_for("testnet", "mainnet_5m", "live_mainnet", enable_plugins=["market_history_api"])
def test_get_recent_trades(node: tt.InitNode | tt.RemoteNode, should_prepare: bool) -> None:
    if should_prepare:
        wallet = tt.Wallet(attach_to=node)
        wallet.create_account("alice", hives=tt.Asset.Test(100), vests=tt.Asset.Test(100))
        wallet.create_account("bob", hives=tt.Asset.Test(100), vests=tt.Asset.Test(100), hbds=tt.Asset.Tbd(100))

        wallet.api.create_order(
            "alice", 0, tt.Asset.Test(100), tt.Asset.Tbd(100), False, 3600
        )  # Sell 100 HIVE for 100 HBD
        wallet.api.create_order(
            "bob", 0, tt.Asset.Tbd(100), tt.Asset.Test(100), False, 3600
        )  # Buy 100 HIVE for 100 HBD

    trades = node.api.market_history.get_recent_trades(limit=10).trades
    assert len(trades) != 0
