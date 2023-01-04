import test_tools as tt

from hive_local_tools import run_for


@run_for("testnet", "mainnet_5m", "live_mainnet")
def test_get_collateralized_conversion_requests(node, should_prepare):
    if should_prepare:
        wallet = tt.Wallet(attach_to=node)
        wallet.api.convert_hive_with_collateral("initminer", tt.Asset.Test(10))
    # method introduced after HF25
    node.api.condenser.get_collateralized_conversion_requests("initminer")
