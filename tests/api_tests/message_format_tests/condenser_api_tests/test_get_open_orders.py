import test_tools as tt

from hive_local_tools import run_for


@run_for("testnet", "mainnet_5m", "live_mainnet")
def test_get_open_orders(node, should_prepare):
    if should_prepare:
        wallet = tt.Wallet(attach_to=node)
        wallet.create_account("abit", hives=tt.Asset.Test(100), vests=tt.Asset.Test(100))
        wallet.api.create_order("abit", 0, tt.Asset.Test(50), tt.Asset.Tbd(5), False, 3600)
    node.api.condenser.get_open_orders("abit")
