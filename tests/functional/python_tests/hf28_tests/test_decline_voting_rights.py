import pytest

import test_tools as tt
from hive_local_tools import run_for
from hive_local_tools.constants import OWNER_AUTH_RECOVERY_PERIOD, TIME_REQUIRED_TO_DECLINE_VOTING_RIGHTS
from hive_local_tools.functional.python.hf28.constants import VOTER_ACCOUNT
from hive_local_tools.functional.python.operation import get_virtual_operations, get_rc_current_mana


@run_for("testnet")
def test_decline_voting_rights(prepare_environment):
    node, wallet = prepare_environment

    mana_before_decline = get_rc_current_mana(node, VOTER_ACCOUNT)

    transaction = wallet.api.decline_voting_rights(VOTER_ACCOUNT, True)
    assert get_rc_current_mana(node, VOTER_ACCOUNT) < mana_before_decline
    assert transaction["rc_cost"] > 0

    assert len(node.api.database.find_decline_voting_rights_requests(accounts=[VOTER_ACCOUNT])["requests"]) == 1
    assert node.api.database.find_accounts(accounts=[VOTER_ACCOUNT])["accounts"][0]["can_vote"] is True
    node.wait_number_of_blocks(TIME_REQUIRED_TO_DECLINE_VOTING_RIGHTS)
    assert len(node.api.database.find_decline_voting_rights_requests(accounts=[VOTER_ACCOUNT])["requests"]) == 0
    assert node.api.database.find_accounts(accounts=[VOTER_ACCOUNT])["accounts"][0]["can_vote"] is False
    assert len(get_virtual_operations(node, "declined_voting_rights_operation")) == 1


@run_for("testnet")
def test_decline_voting_rights_more_than_once(prepare_environment):
    node, wallet = prepare_environment

    wallet.api.decline_voting_rights(VOTER_ACCOUNT, True)
    node.wait_number_of_blocks(TIME_REQUIRED_TO_DECLINE_VOTING_RIGHTS)
    assert node.api.database.find_accounts(accounts=[VOTER_ACCOUNT])["accounts"][0]["can_vote"] is False

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

    node.wait_number_of_blocks(OWNER_AUTH_RECOVERY_PERIOD)
    assert len(node.api.database.find_decline_voting_rights_requests(accounts=[VOTER_ACCOUNT])["requests"]) == 0
    assert node.api.database.find_accounts(accounts=[VOTER_ACCOUNT])["accounts"][0]["can_vote"] is False
    assert len(get_virtual_operations(node, "declined_voting_rights_operation")) == 1


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
    assert node.api.database.find_accounts(accounts=[VOTER_ACCOUNT])["accounts"][0]["can_vote"] is True

    node.wait_number_of_blocks(OWNER_AUTH_RECOVERY_PERIOD)
    assert len(node.api.database.find_decline_voting_rights_requests(accounts=[VOTER_ACCOUNT])["requests"]) == 0
    assert node.api.database.find_accounts(accounts=[VOTER_ACCOUNT])["accounts"][0]["can_vote"] is True
    assert len(get_virtual_operations(node, "declined_voting_rights_operation")) == 0
