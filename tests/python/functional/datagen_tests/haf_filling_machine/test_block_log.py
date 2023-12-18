from __future__ import annotations

import random

import pytest

import test_tools as tt
from hive_local_tools.functional.python.operation import get_vesting_price


@pytest.mark.parametrize(
    "replayed_node",
    [
        ("block_log_multi_sign", {"owner": 3, "active": 6, "posting": 10}),
        ("block_log_open_sign", {"owner": 0, "active": 0, "posting": 0}),
        ("block_log_single_sign", {"owner": 1, "active": 1, "posting": 1}),
    ],
    indirect=True,
)
@pytest.mark.parametrize("account", [f"account-{random.randint(0, 99_999)}"])
def test_block_log_with_several_type_of_signatures(replayed_node: tt.InitNode, account: int) -> None:
    node, wallet, signature_type = replayed_node

    assert len(wallet.api.list_witnesses("", 1000)) == 21, "Incorrect number of witnesses"
    assert get_vesting_price(node) > 1_799_990, "Too low price Hive to Vest"
    assert len(node.api.database.find_comments(comments=[[account, f"permlink-{account}"]]).comments) == 1
    assert node.api.database.find_accounts(accounts=[account]).accounts[0].balance > tt.Asset.Test(1195)
    assert node.api.database.find_accounts(accounts=[account]).accounts[0].hbd_balance == tt.Asset.Tbd(295)
    assert node.api.database.find_accounts(accounts=[account]).accounts[0].delayed_votes == []
    assert node.api.database.find_accounts(accounts=[account]).accounts[0].vesting_shares > tt.Asset.Vest(2705396)
    assert node.api.database.find_accounts(accounts=[account]).accounts[0].received_vesting_shares == tt.Asset.Vest(
        800000
    )

    # custom json
    wallet.api.use_automatic_authority()
    wallet.api.use_authority("posting", account)
    custom_json = wallet.api.follow(account, account, ["blog"])
    assert len(custom_json["signatures"]) == signature_type["posting"]

    # comment ( posting )
    wallet.api.use_automatic_authority()
    wallet.api.use_authority("posting", account)
    comment = wallet.api.post_comment(account, "test-permlink", "", "someone0", "test-title", "this is a body", "{}")
    assert len(comment["signatures"]) == signature_type["posting"]

    # vote ( posting )
    wallet.api.use_automatic_authority()
    wallet.api.use_authority("posting", "account-0")
    vote = wallet.api.vote(account, account, "test-permlink", 100)
    assert len(vote["signatures"]) == signature_type["posting"]

    # transfer ( active )
    wallet.api.use_automatic_authority()
    wallet.api.use_authority("active", account)
    transfer = wallet.api.transfer(account, "initminer", tt.Asset.Test(1), "{}", broadcast=False)
    assert len(transfer["signatures"]) == signature_type["active"]

    # recurrent transfer ( active )
    wallet.api.use_automatic_authority()
    wallet.api.use_authority("active", account)
    recurrent_transfer = wallet.api.recurrent_transfer(account, "initminer", tt.Asset.Test(1), "memo", 24, 5)
    assert len(recurrent_transfer["signatures"]) == signature_type["active"]
