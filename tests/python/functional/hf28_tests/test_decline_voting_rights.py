from __future__ import annotations

from typing import TYPE_CHECKING

import pytest
from beekeepy.exceptions import ErrorInResponseError

from hive_local_tools import run_for
from hive_local_tools.constants import OWNER_AUTH_RECOVERY_PERIOD, TIME_REQUIRED_TO_DECLINE_VOTING_RIGHTS
from hive_local_tools.functional.python.operation import Account, get_transaction_timestamp, get_virtual_operations
from schemas.operations.virtual import DeclinedVotingRightsOperation

if TYPE_CHECKING:
    import test_tools as tt


@run_for("testnet")
def test_decline_voting_rights(prepare_environment: tuple[tt.InitNode, tt.Wallet], voter: Account) -> None:
    node, wallet = prepare_environment

    transaction = wallet.api.decline_voting_rights(voter.name, True)
    assert transaction["rc_cost"] > 0, "Rc cost of operation should be greater than 0."
    voter.rc_manabar.assert_rc_current_mana_is_reduced(
        transaction["rc_cost"], get_transaction_timestamp(node, transaction)
    )

    assert len(node.api.database.find_decline_voting_rights_requests(accounts=[voter.name])["requests"]) == 1
    assert node.api.database.find_accounts(accounts=[voter.name]).accounts[0].can_vote is True
    node.wait_number_of_blocks(TIME_REQUIRED_TO_DECLINE_VOTING_RIGHTS)
    assert len(node.api.database.find_decline_voting_rights_requests(accounts=[voter.name])["requests"]) == 0
    assert node.api.database.find_accounts(accounts=[voter.name]).accounts[0].can_vote is False
    assert len(get_virtual_operations(node, DeclinedVotingRightsOperation)) == 1


@run_for("testnet")
def test_decline_voting_rights_more_than_once(
    prepare_environment: tuple[tt.InitNode, tt.Wallet], voter: Account
) -> None:
    node, wallet = prepare_environment

    transaction = wallet.api.decline_voting_rights(voter.name, True)
    voter.rc_manabar.assert_rc_current_mana_is_reduced(
        transaction["rc_cost"], get_transaction_timestamp(node, transaction)
    )
    assert len(node.api.database.find_decline_voting_rights_requests(accounts=[voter.name])["requests"]) == 1
    assert node.api.database.find_accounts(accounts=[voter.name]).accounts[0].can_vote is True

    node.wait_number_of_blocks(TIME_REQUIRED_TO_DECLINE_VOTING_RIGHTS)

    assert len(node.api.database.find_decline_voting_rights_requests(accounts=[voter.name])["requests"]) == 0
    assert node.api.database.find_accounts(accounts=[voter.name]).accounts[0].can_vote is False
    assert len(get_virtual_operations(node, DeclinedVotingRightsOperation)) == 1

    with pytest.raises(ErrorInResponseError) as error:
        wallet.api.decline_voting_rights(voter.name, True)
    assert (
        "Voter declined voting rights already, therefore trying to decline voting rights again is forbidden."
        in error.value.error
    ), "Error message other than expected."


@run_for("testnet")
def test_create_two_decline_voting_rights_requests(
    prepare_environment: tuple[tt.InitNode, tt.Wallet], voter: Account
) -> None:
    node, wallet = prepare_environment

    transaction = wallet.api.decline_voting_rights(voter.name, True)
    voter.rc_manabar.assert_rc_current_mana_is_reduced(
        transaction["rc_cost"], get_transaction_timestamp(node, transaction)
    )
    assert len(node.api.database.find_decline_voting_rights_requests(accounts=[voter.name])["requests"]) == 1

    with pytest.raises(ErrorInResponseError) as exception:
        wallet.api.decline_voting_rights(voter.name, True)

    assert (
        "Cannot create new request because one already exists." in exception.value.error
    ), "Error message other than expected."

    node.wait_number_of_blocks(OWNER_AUTH_RECOVERY_PERIOD)

    assert len(node.api.database.find_decline_voting_rights_requests(accounts=[voter.name]).requests) == 0
    assert node.api.database.find_accounts(accounts=[voter.name]).accounts[0].can_vote is False
    assert len(get_virtual_operations(node, DeclinedVotingRightsOperation)) == 1


@run_for("testnet")
def test_remove_non_existent_decline_voting_rights_request(
    prepare_environment: tuple[tt.InitNode, tt.Wallet], voter: Account
) -> None:
    _, wallet = prepare_environment

    with pytest.raises(ErrorInResponseError) as exception:
        wallet.api.decline_voting_rights(voter.name, False)

    assert (
        "Cannot cancel the request because it does not exist." in exception.value.error
    ), "Error message other than expected."


@run_for("testnet")
def test_remove_decline_voting_rights_request(
    prepare_environment: tuple[tt.InitNode, tt.Wallet], voter: Account
) -> None:
    node, wallet = prepare_environment

    transaction = wallet.api.decline_voting_rights(voter.name, True)
    voter.rc_manabar.assert_rc_current_mana_is_reduced(
        transaction["rc_cost"], get_transaction_timestamp(node, transaction)
    )
    voter.rc_manabar.update()

    node.wait_number_of_blocks(1)
    transaction = wallet.api.decline_voting_rights(voter.name, False)
    voter.rc_manabar.assert_rc_current_mana_is_reduced(
        transaction["rc_cost"], get_transaction_timestamp(node, transaction)
    )
    node.wait_number_of_blocks(1)
    assert len(node.api.database.find_decline_voting_rights_requests(accounts=[voter.name]).requests) == 0
    assert node.api.database.find_accounts(accounts=[voter.name]).accounts[0].can_vote is True

    node.wait_number_of_blocks(OWNER_AUTH_RECOVERY_PERIOD)

    assert len(node.api.database.find_decline_voting_rights_requests(accounts=[voter.name]).requests) == 0
    assert node.api.database.find_accounts(accounts=[voter.name]).accounts[0].can_vote is True
    assert len(get_virtual_operations(node, DeclinedVotingRightsOperation)) == 0
