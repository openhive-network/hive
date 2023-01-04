import test_tools as tt
from hive_local_tools import run_for


@run_for("testnet", "mainnet_5m", "live_mainnet")
def test_find_proposals(node, should_prepare):
    if should_prepare:
        wallet = tt.Wallet(attach_to=node)
        wallet.api.create_account("initminer", "alice", "{}")

        wallet.api.post_comment(
            "initminer", "test-permlink", "", "test-parent-permlink", "test-title", "test-body", "{}"
        )
        wallet.api.create_proposal(
            "initminer",
            "initminer",
            tt.Time.from_now(weeks=2),
            tt.Time.from_now(weeks=50),
            tt.Asset.Tbd(5),
            "test subject",
            "test-permlink",
        )
    node.api.condenser.find_proposals([0])
