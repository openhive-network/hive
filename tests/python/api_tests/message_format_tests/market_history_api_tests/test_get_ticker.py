from __future__ import annotations

import test_tools as tt
from hive_local_tools import run_for


@run_for("testnet", "mainnet_5m", "live_mainnet", enable_plugins=["market_history_api"])
def test_get_ticker(node: tt.InitNode | tt.RemoteNode, should_prepare: bool) -> None:
    if should_prepare:
        wallet = tt.Wallet(attach_to=node)
        wallet.create_account("alice", hives=tt.Asset.Test(600), vests=tt.Asset.Test(100))

        wallet.api.create_order("alice", 0, tt.Asset.Test(100), tt.Asset.Tbd(10), False, 3600)
        wallet.api.create_order("initminer", 0, tt.Asset.Tbd(40), tt.Asset.Test(200), False, 3600)
    node.api.market_history.get_ticker()
