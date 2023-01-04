import test_tools as tt

from hive_local_tools import run_for


@run_for("testnet", "mainnet_5m", "live_mainnet")
def test_get_account_history(node, should_prepare):
    if should_prepare:
        wallet = tt.Wallet(attach_to=node)
        wallet.create_account("alice", hbds=tt.Asset.Tbd(100), vests=tt.Asset.Test(100))
        node.wait_for_irreversible_block()
    node.api.condenser.get_account_history("alice", -1, 10)
