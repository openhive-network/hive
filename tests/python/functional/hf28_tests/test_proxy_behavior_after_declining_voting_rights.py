from __future__ import annotations

import pytest

import test_tools as tt
from hive_local_tools import run_for
from hive_local_tools.constants import TIME_REQUIRED_TO_DECLINE_VOTING_RIGHTS
from hive_local_tools.functional.python.operation import Account, get_transaction_timestamp, get_virtual_operations
from schemas.operations.virtual import DeclinedVotingRightsOperation, ProxyClearedOperation


@run_for("testnet")
def test_proxy_before_waiving_voting_rights(
    prepare_environment: tuple[tt.InitNode, tt.Wallet], voter: Account, proxy: Account
) -> None:
    node, wallet = prepare_environment
    transaction = wallet.api.set_voting_proxy(voter.name, proxy.name)
    voter.rc_manabar.assert_rc_current_mana_is_reduced(
        transaction["rc_cost"], get_transaction_timestamp(node, transaction)
    )
    voter.rc_manabar.update()

    transaction = wallet.api.decline_voting_rights(voter.name, True)
    voter.rc_manabar.assert_rc_current_mana_is_reduced(
        transaction["rc_cost"], get_transaction_timestamp(node, transaction)
    )
    assert (voter := node.api.wallet_bridge.get_account(voter.name)) is not None
    assert voter.proxy == proxy.name

    assert len(node.api.database.find_decline_voting_rights_requests(accounts=[voter.name])["requests"]) == 1

    node.wait_number_of_blocks(TIME_REQUIRED_TO_DECLINE_VOTING_RIGHTS)

    assert len(get_virtual_operations(node, ProxyClearedOperation)) == 1
    assert (voter := node.api.wallet_bridge.get_account(voter.name)) is not None
    assert voter.proxy == ""

    assert node.api.database.find_accounts(accounts=[voter.name]).accounts[0].can_vote is False
    assert len(node.api.database.find_decline_voting_rights_requests(accounts=[voter.name])["requests"]) == 0
    assert len(get_virtual_operations(node, DeclinedVotingRightsOperation)) == 1

    node.restart(time_control=tt.OffsetTimeControl(offset="+25h"))
    assert (proxy := node.api.wallet_bridge.get_account(proxy.name)) is not None
    assert not any(proxy.proxied_vsf_votes)


@run_for("testnet")
def test_set_proxy_after_waiving_voting_rights(
    prepare_environment: tuple[tt.InitNode, tt.Wallet], voter: Account, proxy: Account
) -> None:
    node, wallet = prepare_environment

    transaction = wallet.api.decline_voting_rights(voter.name, True)
    voter.rc_manabar.assert_rc_current_mana_is_reduced(
        transaction["rc_cost"], get_transaction_timestamp(node, transaction)
    )
    assert len(node.api.database.find_decline_voting_rights_requests(accounts=[voter.name])["requests"]) == 1

    node.wait_number_of_blocks(TIME_REQUIRED_TO_DECLINE_VOTING_RIGHTS)

    assert node.api.database.find_accounts(accounts=[voter.name]).accounts[0].can_vote is False
    assert len(node.api.database.find_decline_voting_rights_requests(accounts=[voter.name])["requests"]) == 0
    assert len(get_virtual_operations(node, DeclinedVotingRightsOperation)) == 1

    with pytest.raises(tt.exceptions.CommunicationError) as exception:
        wallet.api.set_voting_proxy(voter.name, proxy.name)

    assert (
        "Account has declined the ability to vote and cannot proxy votes." in exception.value.error
    ), "Error message other than expected."


@run_for("testnet")
def test_set_proxy_when_decline_voting_rights_is_in_progress(
    prepare_environment: tuple[tt.InitNode, tt.Wallet], voter: Account, proxy: Account
) -> None:
    node, wallet = prepare_environment

    transaction = wallet.api.decline_voting_rights(voter.name, True)
    voter.rc_manabar.assert_rc_current_mana_is_reduced(
        transaction["rc_cost"], get_transaction_timestamp(node, transaction)
    )
    voter.rc_manabar.update()
    assert len(node.api.database.find_decline_voting_rights_requests(accounts=[voter.name])["requests"]) == 1

    node.wait_for_block_with_number(transaction["block_num"] + (TIME_REQUIRED_TO_DECLINE_VOTING_RIGHTS // 2))

    transaction = wallet.api.set_voting_proxy(voter.name, proxy.name)
    voter.rc_manabar.assert_rc_current_mana_is_reduced(
        transaction["rc_cost"], get_transaction_timestamp(node, transaction)
    )
    node.wait_for_block_with_number(transaction["block_num"] + TIME_REQUIRED_TO_DECLINE_VOTING_RIGHTS)

    assert node.api.database.find_accounts(accounts=[voter.name]).accounts[0].can_vote is False
    assert len(node.api.database.find_decline_voting_rights_requests(accounts=[voter.name])["requests"]) == 0
    assert len(get_virtual_operations(node, DeclinedVotingRightsOperation)) == 1

    assert len(get_virtual_operations(node, ProxyClearedOperation)) == 1
    node.restart(time_control=tt.OffsetTimeControl(offset="+25h"))

    assert (voter := node.api.wallet_bridge.get_account(voter.name)) is not None
    assert voter.proxy == ""

    assert (proxy := node.api.wallet_bridge.get_account(proxy.name)) is not None
    assert not any(proxy.proxied_vsf_votes)


@run_for("testnet")
def test_proxied_vsf_votes_when_principal_account_declined_its_voting_rights(
    prepare_environment: tuple[tt.InitNode, tt.Wallet], voter: Account, proxy: Account
) -> None:
    node, wallet = prepare_environment

    transaction = wallet.api.set_voting_proxy(voter.name, proxy.name)
    voter.rc_manabar.assert_rc_current_mana_is_reduced(
        transaction["rc_cost"], get_transaction_timestamp(node, transaction)
    )
    voter.rc_manabar.update()
    node.wait_for_irreversible_block()
    node.restart(time_control=tt.OffsetTimeControl(offset="+25h", speed_up_rate=5))

    assert (proxy := node.api.wallet_bridge.get_account(proxy.name)) is not None
    assert int(proxy.proxied_vsf_votes[0]) > 0

    transaction = wallet.api.decline_voting_rights(voter.name, True)
    voter.rc_manabar.assert_rc_current_mana_is_reduced(
        transaction["rc_cost"], get_transaction_timestamp(node, transaction)
    )

    assert len(node.api.database.find_decline_voting_rights_requests(accounts=[voter.name])["requests"]) == 1

    node.wait_number_of_blocks(TIME_REQUIRED_TO_DECLINE_VOTING_RIGHTS)

    assert (voter := node.api.wallet_bridge.get_account(voter.name)) is not None
    assert voter.proxy == ""
    assert len(get_virtual_operations(node, ProxyClearedOperation)) == 1

    assert (proxy := node.api.wallet_bridge.get_account(proxy.name)) is not None
    assert not any(proxy.proxied_vsf_votes)

    assert node.api.database.find_accounts(accounts=[voter.name]).accounts[0].can_vote is False
    assert len(node.api.database.find_decline_voting_rights_requests(accounts=[voter.name])["requests"]) == 0
    assert len(get_virtual_operations(node, DeclinedVotingRightsOperation)) == 1
