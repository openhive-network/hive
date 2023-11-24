from __future__ import annotations

import pytest

import test_tools as tt
from hive_local_tools import run_for
from hive_local_tools.constants import TIME_REQUIRED_TO_DECLINE_VOTING_RIGHTS
from hive_local_tools.functional.python.hf28 import create_proposal
from hive_local_tools.functional.python.hf28.constants import VOTER_ACCOUNT
from hive_local_tools.functional.python.operation import get_virtual_operations
from schemas.operations.virtual import DeclinedVotingRightsOperation


@run_for("testnet")
def test_if_proposal_votes_were_removed_after_declining_voting_rights(
    prepare_environment: tuple[tt.InitNode, tt.Wallet]
):
    node, wallet = prepare_environment
    create_proposal(wallet)

    # vote for proposal -> decline voting rights -> wait 30 days (60s in testnet) for approval decline voting rights
    wallet.api.update_proposal_votes(VOTER_ACCOUNT, [0], True)
    wallet.api.decline_voting_rights(VOTER_ACCOUNT, True)
    node.wait_number_of_blocks(TIME_REQUIRED_TO_DECLINE_VOTING_RIGHTS)

    assert node.api.database.find_accounts(accounts=[VOTER_ACCOUNT]).accounts[0].can_vote is False
    assert len(get_virtual_operations(node, DeclinedVotingRightsOperation)) == 1
    assert len(node.api.condenser.list_proposal_votes([""], 1000, "by_voter_proposal", "ascending", "all")) == 0


@run_for("testnet")
def test_vote_for_proposal_from_account_that_has_declined_its_voting_rights(
    prepare_environment: tuple[tt.InitNode, tt.Wallet]
):
    node, wallet = prepare_environment
    create_proposal(wallet)

    # decline voting rights -> wait 30 days (60s in testnet) for approval decline voting rights -> vote for proposal
    wallet.api.decline_voting_rights(VOTER_ACCOUNT, True)
    node.wait_number_of_blocks(TIME_REQUIRED_TO_DECLINE_VOTING_RIGHTS)

    with pytest.raises(tt.exceptions.CommunicationError):
        wallet.api.update_proposal_votes(VOTER_ACCOUNT, [0], True)


@run_for("testnet")
def test_vote_for_proposal_when_decline_voting_rights_request_is_being_executed(
    prepare_environment: tuple[tt.InitNode, tt.Wallet]
):
    node, wallet = prepare_environment
    create_proposal(wallet)

    # decline voting rights -> vote for proposal (before approving the waiver of voting rights) -> wait remaining time
    head_block_number = wallet.api.decline_voting_rights(VOTER_ACCOUNT, True)["block_num"]
    node.wait_for_block_with_number(head_block_number + (TIME_REQUIRED_TO_DECLINE_VOTING_RIGHTS // 2))
    wallet.api.update_proposal_votes(VOTER_ACCOUNT, [0], True)
    node.wait_for_block_with_number(head_block_number + TIME_REQUIRED_TO_DECLINE_VOTING_RIGHTS)

    assert len(node.api.condenser.list_proposal_votes([""], 1000, "by_voter_proposal", "ascending", "all")) == 0
