from __future__ import annotations

import pytest
import test_tools as tt
from hive_local_tools import run_for


def _is_mainnet_5m(request: pytest.FixtureRequest) -> bool:
    """Check if current test is running against mainnet_5m node."""
    return request.node.get_closest_marker("mainnet_5m") is not None


@run_for("testnet", "mainnet_5m", "live_mainnet", enable_plugins=["market_history_api"])
def test_get_trade_history(
    node: tt.InitNode | tt.RemoteNode, should_prepare: bool, request: pytest.FixtureRequest
) -> None:
    if should_prepare:
        wallet = tt.Wallet(attach_to=node)
        wallet.create_account("alice", hives=tt.Asset.Test(1000), vests=tt.Asset.Test(1000))
        wallet.create_account("bob", hives=tt.Asset.Test(1000), vests=tt.Asset.Test(1000), hbds=tt.Asset.Tbd(1000))

        wallet.api.create_order(
            "alice", 0, tt.Asset.Test(100), tt.Asset.Tbd(100), False, 3600
        )  # Sell 100 HIVE for 100 HBD
        wallet.api.create_order(
            "bob", 0, tt.Asset.Tbd(100), tt.Asset.Test(100), False, 3600
        )  # Buy 100 HIVE for 100 HBD
    history = node.api.market_history.get_trade_history(start="2016-03-24T16:05:00", end=tt.Time.now(), limit=10).trades
    # mainnet_5m may not have trades in the queried time window (depends on block log contents)
    if not _is_mainnet_5m(request):
        assert len(history) != 0
