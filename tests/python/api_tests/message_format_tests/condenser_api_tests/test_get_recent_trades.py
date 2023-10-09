import test_tools as tt
from hive_local_tools import run_for


@run_for("testnet", "mainnet_5m", "live_mainnet")
def test_get_recent_trades(node, should_prepare):
    preparation_for_testnet_node(node, should_prepare)

    trades = node.api.condenser.get_recent_trades(100)
    assert len(trades) != 0


@run_for("testnet", "mainnet_5m", "live_mainnet")
def test_get_recent_trades_with_default_argument(node, should_prepare):
    preparation_for_testnet_node(node, should_prepare)

    trades = node.api.condenser.get_recent_trades()
    assert len(trades) != 0


def preparation_for_testnet_node(node, should_prepare):
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
