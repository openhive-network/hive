"""
This file contains tests showing abnormalities on hardfork 27, and behavior after hardfork 28.
Related issue: https://gitlab.syncad.com/hive/hive/-/issues/441
"""
import pytest
import time

import test_tools as tt
from hive_local_tools import run_for
from hive_local_tools.constants import TIME_REQUIRED_TO_DECLINE_VOTING_RIGHTS
from hive_local_tools.functional.python.hf28 import create_proposal
from hive_local_tools.functional.python.hf28.constants import VOTER_ACCOUNT


@run_for("testnet")
def test_decline_voting_rights_more_than_once_on_hf_27(prepare_environment_on_hf_27):
    node, wallet = prepare_environment_on_hf_27

    wallet.api.decline_voting_rights(VOTER_ACCOUNT, True)
    node.wait_number_of_blocks(TIME_REQUIRED_TO_DECLINE_VOTING_RIGHTS)
    assert node.api.database.find_accounts(accounts=[VOTER_ACCOUNT])['accounts'][0]["can_vote"] == False

    error_message = "Voter declined voting rights already, therefore trying to decline voting rights again is forbidden."

    with pytest.raises(tt.exceptions.CommunicationError) as exception_from_hf_27:
        wallet.api.decline_voting_rights(VOTER_ACCOUNT, True)
    response_from_hf_27 = exception_from_hf_27.value.response
    assert error_message in response_from_hf_27["error"]["message"]

    wait_for_hardfork_28_application(node)

    with pytest.raises(tt.exceptions.CommunicationError) as exception_from_hf_28:
        wallet.api.decline_voting_rights(VOTER_ACCOUNT, True)
    response_from_hf_28 = exception_from_hf_28.value.response
    assert error_message in response_from_hf_28["error"]["message"]


@run_for("testnet")
def test_if_proposal_votes_were_removed_after_declining_voting_rights_on_hf_27(prepare_environment_on_hf_27):
    node, wallet = prepare_environment_on_hf_27
    create_proposal(wallet)

    # vote for proposal -> decline voting rights -> wait 30 days (60s in testnet) for approval decline voting rights
    wallet.api.update_proposal_votes(VOTER_ACCOUNT, [0], True)
    wallet.api.decline_voting_rights(VOTER_ACCOUNT, True)
    node.wait_number_of_blocks(TIME_REQUIRED_TO_DECLINE_VOTING_RIGHTS)
    assert len(node.api.database.list_proposal_votes(start=[""], limit=1000, order="by_voter_proposal",
                                                     order_direction="ascending", status="all")['proposal_votes']) == 1

    wait_for_hardfork_28_application(node)

    assert len(node.api.database.list_proposal_votes(start=[""], limit=1000, order="by_voter_proposal",
                                                     order_direction="ascending", status="all")['proposal_votes']) == 0


@run_for("testnet")
def test_vote_for_proposal_from_account_that_has_declined_its_voting_rights_on_hf_27(prepare_environment_on_hf_27):
    node, wallet = prepare_environment_on_hf_27
    create_proposal(wallet)

    # decline voting rights -> wait 30 days (60s in testnet) for approval decline voting rights -> vote for proposal
    wallet.api.decline_voting_rights(VOTER_ACCOUNT, True)
    node.wait_number_of_blocks(TIME_REQUIRED_TO_DECLINE_VOTING_RIGHTS)

    error_message = "Voter declined voting rights, therefore casting votes is forbidden."
    with pytest.raises(tt.exceptions.CommunicationError) as exception_from_hf_27:
        wallet.api.update_proposal_votes(VOTER_ACCOUNT, [0], True)
    response_from_hf_27 = exception_from_hf_27.value.response
    assert error_message in response_from_hf_27["error"]["message"]

    wait_for_hardfork_28_application(node)

    with pytest.raises(tt.exceptions.CommunicationError) as exception_from_hf_28:
        wallet.api.update_proposal_votes(VOTER_ACCOUNT, [0], True)
    response_from_hf_28 = exception_from_hf_28.value.response
    assert error_message in response_from_hf_28["error"]["message"]


@run_for("testnet")
def test_vote_for_proposal_when_decline_voting_rights_request_is_being_executed_on_hf_27(prepare_environment_on_hf_27):
    node, wallet = prepare_environment_on_hf_27
    create_proposal(wallet)

    # decline voting rights -> vote for proposal (before approving the waiver of voting rights) -> wait remaining time
    head_block_number = wallet.api.decline_voting_rights(VOTER_ACCOUNT, True)["block_num"]
    node.wait_for_block_with_number(head_block_number + (TIME_REQUIRED_TO_DECLINE_VOTING_RIGHTS // 2))
    wallet.api.update_proposal_votes(VOTER_ACCOUNT, [0], True)
    node.wait_for_block_with_number(head_block_number + TIME_REQUIRED_TO_DECLINE_VOTING_RIGHTS)

    assert (
            len(node.api.wallet_bridge.list_proposal_votes([""], 1000, "by_voter_proposal", "ascending", "all")
                ['proposal_votes']) == 1
    )

    wait_for_hardfork_28_application(node)

    assert (
            len(node.api.wallet_bridge.list_proposal_votes([""], 1000, "by_voter_proposal", "ascending", "all")
                ['proposal_votes']) == 0
    )


def wait_for_hardfork_28_application(node: tt.InitNode, timeout: int=240) -> None:
    tt.logger.info("Waiting for application hardfork 28")
    timeout = time.time() + timeout
    while True:
        if node.api.wallet_bridge.get_hardfork_version() == "1.28.0":
            node.wait_number_of_blocks(1)
            break
        if time.time() > timeout:
            raise TimeoutError("hardfork 28 was not applied within expected time")
