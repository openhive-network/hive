from __future__ import annotations

import pytest

import test_tools as tt
from hive_local_tools import run_for
from hive_local_tools.constants import TIME_REQUIRED_TO_DECLINE_VOTING_RIGHTS
from hive_local_tools.functional.python.hf28.constants import VOTER_ACCOUNT
from hive_local_tools.functional.python.operation import Account, get_transaction_timestamp, get_virtual_operations
from schemas.operations.virtual import DeclinedVotingRightsOperation


@run_for("testnet")
def test_decline_voting_rights_after_voting_for_witness(
    prepare_environment: tuple[tt.InitNode, tt.Wallet], voter: Account
) -> None:
    node, wallet = prepare_environment

    transaction = wallet.api.vote_for_witness(voter.name, "initminer", approve=True)
    assert len(node.api.database.list_witness_votes(start=["", ""], limit=10, order="by_account_witness").votes) == 1
    voter.rc_manabar.assert_rc_current_mana_is_reduced(
        transaction["rc_cost"], get_transaction_timestamp(node, transaction)
    )

    voter.rc_manabar.update()

    transaction = wallet.api.decline_voting_rights(voter.name, True)
    voter.rc_manabar.assert_rc_current_mana_is_reduced(
        transaction["rc_cost"], get_transaction_timestamp(node, transaction)
    )
    node.wait_number_of_blocks(TIME_REQUIRED_TO_DECLINE_VOTING_RIGHTS)

    assert node.api.database.find_accounts(accounts=[voter.name]).accounts[0].can_vote is False
    assert len(node.api.database.find_decline_voting_rights_requests(accounts=[voter.name])["requests"]) == 0
    assert len(get_virtual_operations(node, DeclinedVotingRightsOperation)) == 1

    assert len(node.api.database.list_witness_votes(start=["", ""], limit=10, order="by_account_witness").votes) == 0


@run_for("testnet")
def test_vote_with_declined_voting_rights(prepare_environment: tuple[tt.InitNode, tt.Wallet], voter: Account) -> None:
    node, wallet = prepare_environment

    # decline voting rights -> wait 30 days (60s in testnet) for approval decline voting rights -> vote for witness
    transaction = wallet.api.decline_voting_rights(VOTER_ACCOUNT, True)
    voter.rc_manabar.assert_rc_current_mana_is_reduced(
        transaction["rc_cost"], get_transaction_timestamp(node, transaction)
    )
    assert len(node.api.database.find_decline_voting_rights_requests(accounts=[voter.name])["requests"]) == 1

    node.wait_number_of_blocks(TIME_REQUIRED_TO_DECLINE_VOTING_RIGHTS)

    assert node.api.database.find_accounts(accounts=[voter.name]).accounts[0].can_vote is False
    assert len(node.api.database.find_decline_voting_rights_requests(accounts=[voter.name])["requests"]) == 0
    assert len(get_virtual_operations(node, DeclinedVotingRightsOperation)) == 1

    with pytest.raises(tt.exceptions.CommunicationError) as exception:
        wallet.api.vote_for_witness(VOTER_ACCOUNT, "initminer", approve=True)

    assert "Account has declined its voting rights." in exception.value.error, "Error message other than expected."


@run_for("testnet")
def test_vote_for_witness_when_decline_voting_rights_is_in_progress(
    prepare_environment: tuple[tt.InitNode, tt.Wallet], voter: Account
) -> None:
    node, wallet = prepare_environment

    # decline voting rights -> vote for witness (before approving the waiver of voting rights) -> wait remaining time
    transaction = wallet.api.decline_voting_rights(voter.name, True)
    voter.rc_manabar.assert_rc_current_mana_is_reduced(
        transaction["rc_cost"], get_transaction_timestamp(node, transaction)
    )
    voter.rc_manabar.update()
    assert len(node.api.database.find_decline_voting_rights_requests(accounts=[voter.name])["requests"]) == 1

    node.wait_for_block_with_number(transaction["block_num"] + (TIME_REQUIRED_TO_DECLINE_VOTING_RIGHTS // 2))

    transaction = wallet.api.vote_for_witness(voter.name, "initminer", True)
    voter.rc_manabar.assert_rc_current_mana_is_reduced(
        transaction["rc_cost"], get_transaction_timestamp(node, transaction)
    )
    assert len(node.api.database.list_witness_votes(start=["", ""], limit=10, order="by_account_witness").votes) == 1

    node.wait_for_block_with_number(transaction["block_num"] + TIME_REQUIRED_TO_DECLINE_VOTING_RIGHTS)

    assert len(node.api.database.list_witness_votes(start=["", ""], limit=10, order="by_account_witness").votes) == 0

    assert node.api.database.find_accounts(accounts=[voter.name]).accounts[0].can_vote is False
    assert len(node.api.database.find_decline_voting_rights_requests(accounts=[voter.name])["requests"]) == 0
    assert len(get_virtual_operations(node, DeclinedVotingRightsOperation)) == 1
