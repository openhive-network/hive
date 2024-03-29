from __future__ import annotations

import test_tools as tt
from hive_local_tools import run_for


@run_for("testnet", "mainnet_5m", "live_mainnet")
def test_get_active_votes(node: tt.InitNode | tt.RemoteNode, should_prepare: bool) -> None:
    if should_prepare:
        wallet = tt.Wallet(attach_to=node)
        wallet.create_account("acidyo", hives=tt.Asset.Test(100), vests=tt.Asset.Test(100))
        wallet.api.post_comment(
            "acidyo",
            "drew-an-avatar-signature-for-my-posts",
            "",
            "test-parent-permlink",
            "test-title",
            "test-body",
            "{}",
        )
        wallet.api.vote("initminer", "acidyo", "drew-an-avatar-signature-for-my-posts", 100)
    node.api.condenser.get_active_votes("acidyo", "drew-an-avatar-signature-for-my-posts")
