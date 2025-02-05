from __future__ import annotations

from typing import TYPE_CHECKING

import pytest
from helpy.exceptions import ErrorInResponseError

from hive_local_tools import run_for
from hive_local_tools.constants import TIME_REQUIRED_TO_DECLINE_VOTING_RIGHTS
from hive_local_tools.functional.python.hf28 import create_proposal
from hive_local_tools.functional.python.operation import Account, get_transaction_timestamp, get_virtual_operations
from schemas.operations.virtual import DeclinedVotingRightsOperation

if TYPE_CHECKING:
    import test_tools as tt


@run_for("testnet")
def test_if_proposal_votes_were_removed_after_declining_voting_rights(
    prepare_environment: tuple[tt.InitNode, tt.Wallet], voter: Account
) -> None:
    node, wallet = prepare_environment
    create_proposal(wallet)

    # vote for proposal -> decline voting rights -> wait 30 days (60s in testnet) for approval decline voting rights
    transaction = wallet.api.update_proposal_votes(voter.name, [0], True)
    voter.rc_manabar.assert_rc_current_mana_is_reduced(
        transaction["rc_cost"], get_transaction_timestamp(node, transaction)
    )
    voter.rc_manabar.update()
    transaction = wallet.api.decline_voting_rights(voter.name, True)
    voter.rc_manabar.assert_rc_current_mana_is_reduced(
        transaction["rc_cost"], get_transaction_timestamp(node, transaction)
    )

    assert len(node.api.condenser.list_proposal_votes([""], 1000, "by_voter_proposal", "ascending", "all")) == 1
    node.wait_number_of_blocks(TIME_REQUIRED_TO_DECLINE_VOTING_RIGHTS)

    assert node.api.database.find_accounts(accounts=[voter.name]).accounts[0].can_vote is False
    assert len(get_virtual_operations(node, DeclinedVotingRightsOperation)) == 1
    assert len(node.api.condenser.list_proposal_votes([""], 1000, "by_voter_proposal", "ascending", "all")) == 0


@run_for("testnet")
def test_vote_for_proposal_from_account_that_has_declined_its_voting_rights(
    prepare_environment: tuple[tt.InitNode, tt.Wallet], voter: Account
) -> None:
    node, wallet = prepare_environment
    create_proposal(wallet)

    # decline voting rights -> wait 30 days (60s in testnet) for approval decline voting rights -> vote for proposal
    transaction = wallet.api.decline_voting_rights(voter.name, True)
    voter.rc_manabar.assert_rc_current_mana_is_reduced(
        transaction["rc_cost"], get_transaction_timestamp(node, transaction)
    )
    assert len(node.api.database.find_decline_voting_rights_requests(accounts=[voter.name])["requests"]) == 1

    node.wait_number_of_blocks(TIME_REQUIRED_TO_DECLINE_VOTING_RIGHTS)

    assert node.api.database.find_accounts(accounts=[voter.name]).accounts[0].can_vote is False
    assert len(node.api.database.find_decline_voting_rights_requests(accounts=[voter.name])["requests"]) == 0
    assert len(get_virtual_operations(node, DeclinedVotingRightsOperation)) == 1
    with pytest.raises(ErrorInResponseError) as error:
        wallet.api.update_proposal_votes(voter.name, [0], True)
    assert (
        "Voter declined voting rights, therefore casting votes is forbidden." in error.value.error
    ), "Error message other than expected."


@run_for("testnet")
def test_vote_for_proposal_when_decline_voting_rights_request_is_being_executed(
    prepare_environment: tuple[tt.InitNode, tt.Wallet], voter: Account
) -> None:
    node, wallet = prepare_environment
    create_proposal(wallet)

    # decline voting rights -> vote for proposal (before approving the waiver of voting rights) -> wait remaining time
    transaction = wallet.api.decline_voting_rights(voter.name, True)
    voter.rc_manabar.assert_rc_current_mana_is_reduced(
        transaction["rc_cost"], get_transaction_timestamp(node, transaction)
    )
    voter.rc_manabar.update()
    assert len(node.api.database.find_decline_voting_rights_requests(accounts=[voter.name])["requests"]) == 1

    node.wait_for_block_with_number(transaction["block_num"] + (TIME_REQUIRED_TO_DECLINE_VOTING_RIGHTS // 2))
    transaction = wallet.api.update_proposal_votes(voter.name, [0], True)
    voter.rc_manabar.assert_rc_current_mana_is_reduced(
        transaction["rc_cost"], get_transaction_timestamp(node, transaction)
    )
    node.wait_for_block_with_number(transaction["block_num"] + TIME_REQUIRED_TO_DECLINE_VOTING_RIGHTS)

    assert len(node.api.condenser.list_proposal_votes([""], 1000, "by_voter_proposal", "ascending", "all")) == 0

    assert node.api.database.find_accounts(accounts=[voter.name]).accounts[0].can_vote is False
    assert len(node.api.database.find_decline_voting_rights_requests(accounts=[voter.name])["requests"]) == 0
    assert len(get_virtual_operations(node, DeclinedVotingRightsOperation)) == 1
