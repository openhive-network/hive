"""This test verifies the correctness of generated block_logs. It is not included in the CI tests."""
from __future__ import annotations

import random

import pytest

import test_tools as tt
from hive_local_tools.functional.python.operation import get_vesting_price
from python.functional.util.universal_block_logs.generate_block_log_with_varied_signature_types import (
    ACCOUNT_NAMES,
    DELEGATION_PER_ACCOUNT,
    TBD_PER_ACCOUNT,
)


@pytest.mark.testnet()
@pytest.mark.parametrize(
    "replayed_node",
    [
        ("block_log_multi_sign", {"owner": 3, "active": 6, "posting": 10}),
        ("block_log_open_sign", {"owner": 0, "active": 0, "posting": 0}),
        ("block_log_single_sign", {"owner": 1, "active": 1, "posting": 1}),
    ],
    ids=["block_log_multi_sign", "block_log_open_sign", "block_log_single_sign"],
    indirect=True,
)
def test_block_log_with_several_type_of_signatures(replayed_node: tt.InitNode) -> None:
    node, wallet, signature_type = replayed_node

    account: str = f"account-{random.randint(0, 99_999)}"

    assert len(wallet.api.list_witnesses("", 1000)) == 21, "Incorrect number of witnesses"
    assert get_vesting_price(node) > 1_799_900, "Too low price Hive to Vest"
    assert len(node.api.database.find_comments(comments=[[account, f"permlink-{account.split('-')[1]}"]]).comments) == 1

    find_account_output = node.api.database.find_accounts(accounts=[account]).accounts[0]
    assert find_account_output.balance >= tt.Asset.Test(110)
    assert find_account_output.hbd_balance == TBD_PER_ACCOUNT
    assert find_account_output.delayed_votes == []
    assert find_account_output.vesting_shares >= tt.Asset.Vest(103000)
    assert find_account_output.received_vesting_shares == DELEGATION_PER_ACCOUNT
    assert node.api.database.find_accounts(accounts=[ACCOUNT_NAMES[-1]]).accounts[0].name == ACCOUNT_NAMES[-1]

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
    wallet.api.use_authority("posting", account)
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
