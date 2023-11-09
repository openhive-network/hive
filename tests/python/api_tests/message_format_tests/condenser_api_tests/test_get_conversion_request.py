from __future__ import annotations

import test_tools as tt
from hive_local_tools import run_for


@run_for("testnet", "mainnet_5m", "live_mainnet")
def test_get_conversion_request(node: tt.InitNode | tt.RemoteNode, should_prepare: bool) -> None:
    if should_prepare:
        wallet = tt.Wallet(attach_to=node)
        wallet.create_account("seanmclellan", hbds=tt.Asset.Tbd(100), vests=tt.Asset.Test(100))
        wallet.api.convert_hbd("seanmclellan", tt.Asset.Tbd(10))
    node.api.condenser.get_conversion_requests("seanmclellan")
