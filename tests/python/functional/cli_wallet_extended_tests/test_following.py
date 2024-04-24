from __future__ import annotations

import json

import pytest

import test_tools as tt

from .utilities import create_accounts


@pytest.mark.skip(reason="Unimplemented functionality in wallet")
def test_following(wallet: tt.Wallet) -> None:
    create_accounts(wallet, "initminer", ["alice", "bob"])

    wallet.api.transfer_to_vesting("initminer", "alice", tt.Asset.Test(100))

    response = wallet.api.follow("alice", "bob", ["blog"])  # hasz1
    operation = response["operations"][0]
    operation_type, operation_json = operation[0], json.loads(operation[1]["json"])

    assert operation_type == "custom_json"
    assert operation_json == {
        "type": "follow_operation",
        "value": {"follower": "alice", "following": "@bob", "what": ["blog"]},
    }
