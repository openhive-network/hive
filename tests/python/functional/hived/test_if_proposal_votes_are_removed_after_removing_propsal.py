from __future__ import annotations

import pytest

import test_tools as tt


@pytest.mark.parametrize("api", ["database", "condenser", "wallet_bridge"])
def test_if_proposal_votes_are_removed_after_removing_proposal(
    node_with_20k_proposal_votes: tt.InitNode, api: str
) -> None:
    wallet = tt.Wallet(attach_to=node_with_20k_proposal_votes)
    wallet.api.import_keys([tt.Account("alice").private_key])

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

        pack_of_proposals: list | None = None
        if api == "database":
            pack_of_proposals = getattr(node.api, api).list_proposal_votes(**template).proposal_votes
        if api == "condenser":
            pack_of_proposals = getattr(node.api, api).list_proposal_votes(*template.values())
        if api == "wallet_bridge":
            pack_of_proposals = getattr(node.api, api).list_proposal_votes(*template.values()).proposal_votes

        if not pack_of_proposals or len(pack_of_proposals) == 1:
            break

        # skip the last account
        if proposal_votes and pack_of_proposals[0] == proposal_votes[-1]:
            pack_of_proposals.pop(0)

        proposal_votes.extend(pack_of_proposals)

        start_account = pack_of_proposals[-1].voter

    sorted_proposal_votes = sort_votes_by_username(proposal_votes)
    return len(sorted_proposal_votes)


def sort_votes_by_username(list_of_votes: list) -> dict:
    ordered_proposal_votes = {}
    for proposal_vote in list_of_votes:
        if proposal_vote.voter not in ordered_proposal_votes:
            ordered_proposal_votes[proposal_vote.voter] = proposal_vote
        else:
            raise Exception("Duplicate proposal vote")  # noqa: TRY002

    return ordered_proposal_votes
