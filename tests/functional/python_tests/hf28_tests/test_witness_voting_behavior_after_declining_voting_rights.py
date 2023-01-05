import pytest

import test_tools as tt
from hive_local_tools import run_for
from hive_local_tools.functional.python.hf28.constants import VOTER_ACCOUNT, TIME_REQUIRED_TO_DECLINE_VOTING_RIGHTS


@run_for("testnet")
def test_decline_voting_rights_after_voting_for_witness(prepare_environment):
    node, wallet = prepare_environment

    wallet.api.vote_for_witness(VOTER_ACCOUNT, "initminer", approve=True)

    wallet.api.decline_voting_rights(VOTER_ACCOUNT, True)
    node.wait_number_of_blocks(TIME_REQUIRED_TO_DECLINE_VOTING_RIGHTS)

    assert len(node.api.database.list_witness_votes(start=["", ""], limit=10, order="by_account_witness")["votes"]) == 0


@run_for("testnet")
def test_if_voting_with_declined_voting_rights_is_rejected(prepare_environment):
    node, wallet = prepare_environment

    # decline voting rights -> wait 30 days (60s in testnet) for approval decline voting rights -> vote for witness
    wallet.api.decline_voting_rights(VOTER_ACCOUNT, True)
    node.wait_number_of_blocks(TIME_REQUIRED_TO_DECLINE_VOTING_RIGHTS)

    with pytest.raises(tt.exceptions.CommunicationError) as exception:
        wallet.api.vote_for_witness(VOTER_ACCOUNT, "initminer", approve=True)

    error_response = exception.value.response["error"]["message"]
    assert "Account has declined its voting rights." in error_response


@run_for("testnet")
def test_vote_for_witness_when_decline_voting_rights_is_in_progress(prepare_environment):
    node, wallet = prepare_environment

    # decline voting rights -> vote for witness (before approving the waiver of voting rights) -> wait remaining time
    head_block_number = wallet.api.decline_voting_rights(VOTER_ACCOUNT, True)["block_num"]
    node.wait_for_block_with_number(head_block_number + (TIME_REQUIRED_TO_DECLINE_VOTING_RIGHTS // 2))
    wallet.api.vote_for_witness(VOTER_ACCOUNT, "initminer", True)
    node.wait_for_block_with_number(head_block_number + TIME_REQUIRED_TO_DECLINE_VOTING_RIGHTS)

    assert len(node.api.database.list_witness_votes(start=["", ""], limit=10, order="by_account_witness")["votes"]) == 0
