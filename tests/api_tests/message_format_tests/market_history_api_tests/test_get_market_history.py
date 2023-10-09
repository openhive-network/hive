import test_tools as tt

from hive_local_tools import run_for


@run_for("testnet", "live_mainnet")
def test_get_market_history(node, should_prepare):
    if should_prepare:
        wallet = tt.Wallet(attach_to=node)
        wallet.create_account("alice", hives=tt.Asset.Test(100), vests=tt.Asset.Test(100))

        wallet.api.create_order("alice", 0, tt.Asset.Test(50), tt.Asset.Tbd(5), False, 3600)
        wallet.api.create_order("initminer", 1, tt.Asset.Tbd(50), tt.Asset.Test(100), False, 3600)
    history = node.api.market_history.get_market_history(
        bucket_seconds=3600, start=tt.Time.from_now(weeks=-100), end=tt.Time.from_now(weeks=1)
    )["buckets"]
    assert len(history) != 0
