from __future__ import annotations

import pytest

import test_tools as tt

from ....local_tools import date_from_now


@pytest.mark.testnet
def test_if_proposal_id_is_not_reused_after_snapshot_load():
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

    first_node = tt.InitNode()
    first_node.run()
    wallet = tt.Wallet(attach_to=first_node)

    wallet.create_account("alice", hives=tt.Asset.Test(100), vests=tt.Asset.Test(100), hbds=tt.Asset.Tbd(100))
    wallet.api.post_comment("alice", "permlink", "", "parent-permlink", "title", "body", "{}")

    create_proposal(wallet)
    wallet.api.remove_proposal("alice", [0])

    # wait for the blocks with the transactions to become irreversible, and will be saved in block_log
    first_node.wait_number_of_blocks(21)

    # Node during snapshot is restarted - this requires wallet reconnection. This is a bug described in issue
    # https://gitlab.syncad.com/hive/test-tools/-/issues/9. This place have to be rewritten after solving this issue.
    wallet.close()
    snapshot = first_node.dump_snapshot()
    wallet.run()

    second_node = tt.ApiNode()
    connect_nodes(first_node, second_node)
    second_node.run(load_snapshot_from=snapshot, wait_for_live=False)

    create_proposal(wallet)  # to allocate next proposal ids on each node

    assert get_last_proposal_id(first_node) == get_last_proposal_id(second_node) == 1


def connect_nodes(first_node: tt.AnyNode, second_node: tt.AnyNode) -> None:
    """
    This place have to be removed after solving issue https://gitlab.syncad.com/hive/test-tools/-/issues/10
    """
    from test_tools.__private.user_handles.get_implementation import get_implementation
    second_node.config.p2p_seed_node = get_implementation(first_node).get_p2p_endpoint()


def get_last_proposal_id(node) -> int:
    return node.api.database.list_proposals(
        start=["alice"],
        limit=100,
        order="by_creator",
        order_direction="ascending",
        status="all"
    )["proposals"][-1]["id"]


def create_proposal(wallet) -> None:
    wallet.api.create_proposal(
        "alice",
        "alice",
        date_from_now(weeks=1 * 10),
        date_from_now(weeks=1 * 10 + 5),
        tt.Asset.Tbd(1 * 100),
        "subject",
        "permlink",
    )
