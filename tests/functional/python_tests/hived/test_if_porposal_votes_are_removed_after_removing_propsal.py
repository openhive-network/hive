from typing import Optional

import pytest

import test_tools as tt


@pytest.mark.parametrize("api", ["database", "condenser", "wallet_bridge"])
def test_if_proposal_votes_are_removed_after_removing_proposal(node_with_20k_proposal_votes, api):
    wallet = tt.Wallet(attach_to=node_with_20k_proposal_votes)
    wallet.api.import_keys([tt.Account('alice').keys.private])

    # Check the number of votes before removing the proposal
    assert get_all_proposal_votes(node_with_20k_proposal_votes, api) == 20000

    # Delete proposal
    wallet.api.remove_proposal("alice", [0])

    # Check the number of votes after removing the proposal
    assert get_all_proposal_votes(node_with_20k_proposal_votes, api) == 0


def get_all_proposal_votes(node: tt.InitNode, api: str) -> int:
    proposal_votes = []
    start_account = ""

    while True:
        template = {
            "start": [start_account],
            "limit": 1000,
            "order": "by_voter_proposal",
            "order_direction": "ascending",
            "status": "all",
        }

        pack_of_proposals: Optional[list] = None
        if api == "database":
            pack_of_proposals = getattr(node.api, api).list_proposal_votes(**template)["proposal_votes"]
        if api == "condenser":
            pack_of_proposals = getattr(node.api, api).list_proposal_votes(*template.values())
        if api == "wallet_bridge":
            pack_of_proposals = getattr(node.api, api).list_proposal_votes(*template.values())["proposal_votes"]

        if not pack_of_proposals or len(pack_of_proposals) == 1:
            break

        # skip the last account
        if proposal_votes and pack_of_proposals[0] == proposal_votes[-1]:
            pack_of_proposals.pop(0)

        proposal_votes.extend(pack_of_proposals)

        start_account = pack_of_proposals[-1]['voter']

    assert not any(proposal_votes.count(vote) > 1 for vote in proposal_votes)
    return len(proposal_votes)
