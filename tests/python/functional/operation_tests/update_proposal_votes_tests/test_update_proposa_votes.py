"""https://gitlab.syncad.com/hive/hive/-/issues/496
These tests based on the block-log with existing proposals."""
from __future__ import annotations

from typing import TYPE_CHECKING

import pytest

from hive_local_tools.constants import HIVE_GOVERNANCE_VOTE_EXPIRATION_PERIOD
from hive_local_tools.functional.python.operation import Account, list_votes_for_all_proposals
from hive_local_tools.functional.python.operation.update_proposal_votes import UpdateProposalVotes

if TYPE_CHECKING:
    import test_tools as tt


@pytest.mark.testnet()
def test_update_proposal_votes(node: tt.InitNode, wallet: tt.Wallet, voter: Account):
    """User votes for the proposal X and proposal Y."""
    for proposal_id in range(2):
        vote_for_proposal = UpdateProposalVotes(node, wallet, voter.name, proposal_id=proposal_id)

        voter.rc_manabar.assert_rc_current_mana_is_reduced(vote_for_proposal.rc_cost, vote_for_proposal.timestamp)
        assert len(list_votes_for_all_proposals(node)) == proposal_id + 1
        voter.update_account_info()


@pytest.mark.testnet()
def test_cancellation_of_update_proposal_vote(node: tt.InitNode, wallet: tt.Wallet, voter: Account):
    """User removes the vote."""
    for proposal_id in range(2):
        vote_for_proposal = UpdateProposalVotes(node, wallet, voter.name, proposal_id=proposal_id)

        voter.rc_manabar.assert_rc_current_mana_is_reduced(vote_for_proposal.rc_cost, vote_for_proposal.timestamp)
        assert len(list_votes_for_all_proposals(node)) == proposal_id + 1
        voter.update_account_info()

    vote_for_proposal.delete()
    voter.rc_manabar.assert_rc_current_mana_is_reduced(vote_for_proposal.rc_cost, vote_for_proposal.timestamp)
    assert len(list_votes_for_all_proposals(node)) == 1


@pytest.mark.testnet()
def test_expiration_of_update_proposal_vote(node: tt.InitNode, wallet: tt.Wallet, voter: Account):
    """User hasn't voted for a HIVE_GOVERNANCE_VOTE_EXPIRATION_PERIOD."""
    for proposal_id in range(2):
        vote_for_proposal = UpdateProposalVotes(node, wallet, voter.name, proposal_id=proposal_id)

        voter.rc_manabar.assert_rc_current_mana_is_reduced(vote_for_proposal.rc_cost, vote_for_proposal.timestamp)
        assert len(list_votes_for_all_proposals(node)) == proposal_id + 1
        voter.update_account_info()

    node.restart(time_offset=f"+{HIVE_GOVERNANCE_VOTE_EXPIRATION_PERIOD}s")
    assert len(list_votes_for_all_proposals(node)) == 0
