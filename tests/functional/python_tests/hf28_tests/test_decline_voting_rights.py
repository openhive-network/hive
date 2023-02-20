import pytest

import test_tools as tt
from hive_local_tools import run_for
from hive_local_tools.constants import TIME_REQUIRED_TO_DECLINE_VOTING_RIGHTS
from hive_local_tools.functional.python.hf28.constants import VOTER_ACCOUNT


@run_for("testnet")
def test_decline_voting_rights(prepare_environment):
    node, wallet = prepare_environment

    wallet.api.decline_voting_rights(VOTER_ACCOUNT, True)
    assert len(node.api.database.find_decline_voting_rights_requests(accounts=[VOTER_ACCOUNT])["requests"]) == 1
    assert node.api.database.find_accounts(accounts=[VOTER_ACCOUNT])['accounts'][0]["can_vote"] == True
    node.wait_number_of_blocks(TIME_REQUIRED_TO_DECLINE_VOTING_RIGHTS)
    assert len(node.api.database.find_decline_voting_rights_requests(accounts=[VOTER_ACCOUNT])["requests"]) == 0
    assert node.api.database.find_accounts(accounts=[VOTER_ACCOUNT])['accounts'][0]["can_vote"] == False


@run_for("testnet")
def test_decline_voting_rights_more_than_once(prepare_environment):
    node, wallet = prepare_environment

    wallet.api.decline_voting_rights(VOTER_ACCOUNT, True)
    node.wait_number_of_blocks(TIME_REQUIRED_TO_DECLINE_VOTING_RIGHTS)
    assert node.api.database.find_accounts(accounts=[VOTER_ACCOUNT])['accounts'][0]["can_vote"] == False

    with pytest.raises(tt.exceptions.CommunicationError):
        wallet.api.decline_voting_rights(VOTER_ACCOUNT, True)


@run_for("testnet")
def test_create_two_decline_voting_rights_requests(prepare_environment):
    node, wallet = prepare_environment

    wallet.api.decline_voting_rights(VOTER_ACCOUNT, True)

    with pytest.raises(tt.exceptions.CommunicationError) as exception:
        wallet.api.decline_voting_rights(VOTER_ACCOUNT, True)

    response = exception.value.response
    assert "Cannot create new request because one already exists." in response["error"]["message"]


@run_for("testnet")
def test_remove_non_existent_decline_voting_rights_request(prepare_environment):
    node, wallet = prepare_environment

    with pytest.raises(tt.exceptions.CommunicationError) as exception:
        wallet.api.decline_voting_rights(VOTER_ACCOUNT, False)

    response = exception.value.response
    assert "Cannot cancel the request because it does not exist." in response["error"]["message"]


@run_for("testnet")
def test_remove_decline_voting_rights_request(prepare_environment):
    node, wallet = prepare_environment

    wallet.api.decline_voting_rights(VOTER_ACCOUNT, True)
    node.wait_number_of_blocks(1)
    wallet.api.decline_voting_rights(VOTER_ACCOUNT, False)
    node.wait_number_of_blocks(1)
    assert len(node.api.database.find_decline_voting_rights_requests(accounts=[VOTER_ACCOUNT])["requests"]) == 0
    assert node.api.database.find_accounts(accounts=[VOTER_ACCOUNT])['accounts'][0]["can_vote"] == True
