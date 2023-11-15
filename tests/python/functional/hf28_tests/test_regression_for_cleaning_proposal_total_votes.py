from __future__ import annotations

import test_tools as tt
from hive_local_tools.constants import (
    HIVE_DELAYED_VOTING_TOTAL_INTERVAL_SECONDS,
    HIVE_GOVERNANCE_VOTE_EXPIRATION_PERIOD,
)
from hive_local_tools.functional.python.operation import get_virtual_operations, jump_to_date


def test_regression_for_cleaning_proposal_total_votes():
    node = tt.InitNode()
    node.config.plugin.append("account_history_api")
    node.run(time_offset="+0 x10")
    wallet = tt.Wallet(attach_to=node)

    # Create accounts
    wallet.create_account("alice", vests=tt.Asset.Test(20))
    wallet.create_account("bob", vests=tt.Asset.Test(10))

    # Waiting for expiration delayed_votes
    node.wait_for_irreversible_block()
    node.restart(
        time_offset=tt.Time.serialize(
            node.get_head_block_time() + tt.Time.seconds(HIVE_DELAYED_VOTING_TOTAL_INTERVAL_SECONDS),
            format_=tt.Time.TIME_OFFSET_FORMAT,
        )
    )
    assert len(node.api.database.find_accounts(accounts=["bob"])["accounts"][0]["delayed_votes"]) == 0

    # Create proposal
    wallet.api.post_comment("initminer", "permlink", "", "parent-permlink", "permlink-title", "permlink-body", "{}")
    wallet.api.create_proposal(
        "initminer",
        "initminer",
        tt.Time.now(),
        tt.Time.from_now(days=5),
        tt.Asset.Tbd(5),
        "permlink",
        "permlink",
    )

    # Account voting for proposal and witness
    wallet.api.vote_for_witness("alice", "initminer", True)
    wallet.api.update_proposal_votes("alice", [0], True)

    # bob set proxy to alice
    wallet.api.set_voting_proxy("bob", "alice")

    # Wait for `next_maintenance_time`
    node.wait_for_irreversible_block()
    node.restart(
        time_offset=tt.Time.serialize(
            node.get_head_block_time() + tt.Time.seconds(3600),
            format_=tt.Time.TIME_OFFSET_FORMAT,
        )
    )

    # Here we have assertions for voting power has been conferred
    assert node.api.database.list_witnesses(start="", limit=10, order="by_name")["witnesses"][0]["votes"] > 0
    assert (
        node.api.database.list_proposals(
            start=[""], limit=1, order="by_creator", order_direction="ascending", status="all", last_id=0
        )["proposals"][0]["total_votes"]
        > 0
    )

    # Move by HIVE_GOVERNANCE_VOTE_EXPIRATION_PERIOD - 5 day in testnet
    jump_to_date(node, time_offset=node.get_head_block_time() + tt.Time.seconds(HIVE_GOVERNANCE_VOTE_EXPIRATION_PERIOD))

    assert len(get_virtual_operations(node, "proxy_cleared_operation")) == 1
    assert len(get_virtual_operations(node, "expired_account_notification_operation")) == 2
    assert node.api.database.list_witnesses(start="", limit=10, order="by_name")["witnesses"][0]["votes"] == 0

    # TODO: Here test fail, `total_votes` should be 0
    assert (
        node.api.database.list_proposals(
            start=[""], limit=1, order="by_creator", order_direction="ascending", status="all", last_id=0
        )["proposals"][0]["total_votes"]
        == 0
    )
