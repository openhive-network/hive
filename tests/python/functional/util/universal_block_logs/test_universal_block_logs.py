"""This test verifies the correctness of generated block_logs. It is not included in the CI tests."""
from __future__ import annotations

import random

import pytest

import test_tools as tt
from hive_local_tools.functional.python.operation import get_vesting_price
from python.functional.util.universal_block_logs.generate_universal_block_logs import (
    ACCOUNT_NAMES,
    DELEGATION_PER_ACCOUNT,
    NUMBER_OF_COMMENTS,
    TBD_PER_ACCOUNT,
)


@pytest.mark.testnet()
@pytest.mark.parametrize(
    "replayed_node",
    [
        ("block_log_multi_sign", {"owner": 5, "active": 5, "posting": 5}),
        ("block_log_open_sign", {"owner": 0, "active": 0, "posting": 0}),
        ("block_log_single_sign", {"owner": 1, "active": 1, "posting": 1}),
        ("block_log_maximum_sign", {"owner": 1600, "active": 1600, "posting": 1600}),
    ],
    ids=["block_log_multi_sign", "block_log_open_sign", "block_log_single_sign", "block_log_maximum_sign"],
    indirect=True,
)
def test_block_log_with_several_type_of_signatures(replayed_node: tt.InitNode, test_id: str) -> None:
    node, wallet, signature_type = replayed_node
    account_names = ACCOUNT_NAMES if test_id != "block_log_maximum_sign" else ACCOUNT_NAMES[:NUMBER_OF_COMMENTS]

    account: str = account_names[random.randint(0, len(account_names))]
    post_authors = ACCOUNT_NAMES[:NUMBER_OF_COMMENTS]
    random_author = post_authors[random.randint(0, NUMBER_OF_COMMENTS)]

    assert len(wallet.api.list_witnesses("", 1000)) == 21, "Incorrect number of witnesses"
    assert get_vesting_price(node) > 1_799_900, "Too low price Hive to Vest"
    assert (
        len(
            node.api.database.get_comment_pending_payouts(
                comments=[[random_author, f"permlink-{random_author.split('-')[1]}"]]
            ).cashout_infos
        )
        == 1
    )

    find_account_output = node.api.database.find_accounts(accounts=[account]).accounts[0]
    assert find_account_output.balance >= tt.Asset.Test(110)
    assert find_account_output.hbd_balance == TBD_PER_ACCOUNT
    assert find_account_output.delayed_votes == []
    assert find_account_output.vesting_shares >= tt.Asset.Vest(103000)
    assert find_account_output.received_vesting_shares == DELEGATION_PER_ACCOUNT
    assert node.api.database.find_accounts(accounts=[account_names[-1]]).accounts[0].name == account_names[-1]

    # custom json
    wallet.api.use_automatic_authority()
    custom_json = wallet.api.follow(account, account, ["blog"])
    assert len(custom_json["signatures"]) == signature_type["posting"]

    # comment ( posting )
    wallet.api.use_automatic_authority()
    comment = wallet.api.post_comment(account, "test-permlink", "", "someone0", "test-title", "this is a body", "{}")
    assert len(comment["signatures"]) == signature_type["posting"]

    # vote ( posting )
    wallet.api.use_automatic_authority()
    vote = wallet.api.vote(account, account, "test-permlink", 100)
    assert len(vote["signatures"]) == signature_type["posting"]

    # transfer ( active )
    wallet.api.use_automatic_authority()
    transfer = wallet.api.transfer(account, "initminer", tt.Asset.Test(1), "{}", broadcast=False)
    assert len(transfer["signatures"]) == signature_type["active"]

    # recurrent transfer ( active )
    wallet.api.use_automatic_authority()
    recurrent_transfer = wallet.api.recurrent_transfer(account, "initminer", tt.Asset.Test(1), "memo", 24, 5)
    assert len(recurrent_transfer["signatures"]) == signature_type["active"]
