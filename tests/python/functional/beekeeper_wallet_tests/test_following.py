from __future__ import annotations

import test_tools as tt

from .utilities import create_accounts


def test_following(wallet: tt.Wallet) -> None:
    create_accounts(wallet, "initminer", ["alice", "bob"])

    wallet.api.transfer_to_vesting("initminer", "alice", tt.Asset.Test(100))

    response = wallet.api.follow("alice", "bob", ["blog"])
    operation = response["operations"][0]

    assert operation.type_ == "custom_json_operation"
    assert operation.value.id_ == "follow"
    assert operation.value.json_.value == {
        "type": "follow_operation",
        "value": {"follower": "alice", "following": "bob", "what": ["blog"]},
    }
