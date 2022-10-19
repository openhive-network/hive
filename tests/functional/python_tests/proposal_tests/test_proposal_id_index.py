from typing import Dict, List

import pytest

import test_tools as tt

from ....local_tools import date_from_now


@pytest.mark.testnet
def test_proposal_id_on_snapshot_node():
    first_node = tt.InitNode()
    first_node.run()
    first_node_wallet = tt.Wallet(attach_to=first_node)

    first_node_wallet.create_account("alice", hives=tt.Asset.Test(100), vests=tt.Asset.Test(100), hbds=tt.Asset.Tbd(100))
    first_node_wallet.api.post_comment("alice", "permlink", "", "parent-permlink", "title", "body", "{}")

    prepare_proposal(first_node_wallet, number_of_proposals_to_create=3)

    first_node_wallet.api.remove_proposal("alice", [2])

    # wait for the blocks with the transactions to become irreversible, and will be saved in block_log
    first_node.wait_number_of_blocks(21)

    # close the wallet before a snapshot. Node during snapshot is restarted - this requires wallet reconnection
    first_node_wallet.close()
    snapshot = first_node.dump_snapshot()
    first_node_wallet.run()

    second_node = tt.ApiNode()

    connect_nodes(first_node, second_node)

    second_node.run(load_snapshot_from=snapshot, wait_for_live=False)
    second_node_wallet = tt.Wallet(attach_to=second_node)
    second_node_wallet.api.import_keys([tt.PrivateKey("alice")])

    prepare_proposal(first_node_wallet, number_of_proposals_to_create=1)
    prepare_proposal(second_node_wallet, number_of_proposals_to_create=1)

    assert get_active_proposals(first_node) == get_active_proposals(second_node)


def connect_nodes(first_node, second_node) -> None:
    """Manually set p2p_seed_node of second_node configuration to p2p_endpoint of first_node"""
    from test_tools.__private.user_handles.get_implementation import get_implementation
    second_node.config.p2p_seed_node = get_implementation(first_node).get_p2p_endpoint()


def get_active_proposals(node) -> List[Dict]:
    return node.api.database.list_proposals(start=["alice"],
                                            limit=100,
                                            order="by_creator",
                                            order_direction="ascending",
                                            status="all")["proposals"]


def prepare_proposal(wallet, number_of_proposals_to_create=1) -> None:
    with wallet.in_single_transaction():
        for proposal_number in range(number_of_proposals_to_create):
            wallet.api.create_proposal("alice",
                                       "alice",
                                       date_from_now(weeks=1 * 10),
                                       date_from_now(weeks=1 * 10 + 5),
                                       tt.Asset.Tbd(1 * 100),
                                       f"subject-alice-{number_of_proposals_to_create}",
                                       "permlink")
