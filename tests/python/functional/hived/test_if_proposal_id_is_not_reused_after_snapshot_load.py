from __future__ import annotations

import pytest

import test_tools as tt
from hive_local_tools.functional import connect_nodes


@pytest.mark.testnet()
def test_if_proposal_id_is_not_reused_after_snapshot_load() -> None:
    """
    When you create a proposal and delete it, proposal id should not be used again. For example when you create
    proposal with id 0, delete it and create another one, it should have another unique id, i.e. 1. This test
    checks that the snapshot correctly recreates the state of the next available proposal id.

    Earlier in hived there was such a problem that when it took an id and then released it, node on which it was done
    correctly remembered not to use that already used id. When starting the next node loading the snapshot from the
    first node, he guessed (and sometimes was wrong) that the next free proposal id is one greater than the last one used.

    For example (it used to be like this):
    On the first node I create a proposal with id 0 (note that node knows the next free proposal id is 1). I remove this
    proposal (the next free proposal id is still 1, nothing has changed). When I load a snapshot from this node then the
    value of the next free proposal id was not restored. Node saw that all id's were free (because id 0 remained
    released at the moment of deleting the proposal) so hived mistakenly assumed that the next free one was 0.
    """

    init_node = tt.InitNode()
    init_node.config.plugin.append("market_history_api")
    init_node.run()
    wallet = tt.Wallet(attach_to=init_node)

    # This wait is added to avoid situation, when gdpo is called on non existing block 0
    init_node.wait_number_of_blocks(blocks_to_wait=2)

    wallet.create_account("alice", hives=tt.Asset.Test(100), vests=tt.Asset.Test(100), hbds=tt.Asset.Tbd(100))
    wallet.api.post_comment("alice", "permlink", "", "parent-permlink", "title", "body", "{}")

    create_proposal(wallet)
    wallet.api.remove_proposal("alice", [0])

    init_node.wait_for_irreversible_block()

    snapshot = init_node.dump_snapshot()

    api_node = tt.ApiNode()

    # To activate ApiNode in the 'live' mode, plugins must match those of InitNode.
    api_node.config.plugin.remove("transaction_status_api")
    api_node.config.plugin.remove("reputation_api")

    connect_nodes(init_node, api_node)
    api_node.run(load_snapshot_from=snapshot, wait_for_live=True)

    create_proposal(wallet)  # to allocate next proposal ids on each node

    assert get_last_proposal_id(init_node) == get_last_proposal_id(api_node) == 1


def get_last_proposal_id(node: tt.AnyNode) -> int:
    return node.api.database.list_proposals(
        start=["alice"], limit=100, order="by_creator", order_direction="ascending", status="all"
    ).proposals[-1]["id"]


def create_proposal(wallet: tt.Wallet) -> None:
    wallet.api.create_proposal(
        "alice",
        "alice",
        tt.Time.from_now(weeks=10),
        tt.Time.from_now(weeks=15),
        tt.Asset.Tbd(1 * 100),
        "subject",
        "permlink",
    )
