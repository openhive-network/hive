import pytest

import test_tools as tt
from hive_local_tools import run_for
from hive_local_tools.functional.python.hf28.constants import VOTER_ACCOUNT, TIME_REQUIRED_TO_DECLINE_VOTING_RIGHTS, PROXY_ACCOUNT


@run_for("testnet")
def test_proxy_before_waiving_voting_rights(prepare_environment):
    node, wallet = prepare_environment

    wallet.api.set_voting_proxy(VOTER_ACCOUNT, PROXY_ACCOUNT)

    wallet.api.decline_voting_rights(VOTER_ACCOUNT, True)
    assert node.api.wallet_bridge.get_account(VOTER_ACCOUNT)["proxy"] == PROXY_ACCOUNT
    node.wait_number_of_blocks(TIME_REQUIRED_TO_DECLINE_VOTING_RIGHTS)

    node.restart(time_offset="+25h")
    assert node.api.wallet_bridge.get_account(VOTER_ACCOUNT)["proxy"] == ""
    assert not any(node.api.wallet_bridge.get_account(PROXY_ACCOUNT)["proxied_vsf_votes"])


@run_for("testnet")
def test_set_proxy_after_waiving_voting_rights(prepare_environment):
    node, wallet = prepare_environment

    wallet.api.decline_voting_rights(VOTER_ACCOUNT, True)
    node.wait_number_of_blocks(TIME_REQUIRED_TO_DECLINE_VOTING_RIGHTS)

    with pytest.raises(tt.exceptions.CommunicationError) as exception:
        wallet.api.set_voting_proxy(VOTER_ACCOUNT, PROXY_ACCOUNT)

    response = exception.value.response['error']['message']
    assert "Account has declined the ability to vote and cannot proxy votes." in response


@run_for("testnet")
def test_set_proxy_when_decline_voting_rights_is_in_progress(prepare_environment):
    node, wallet = prepare_environment

    head_block_number = wallet.api.decline_voting_rights(VOTER_ACCOUNT, True)["block_num"]
    node.wait_for_block_with_number(head_block_number + (TIME_REQUIRED_TO_DECLINE_VOTING_RIGHTS // 2))
    wallet.api.set_voting_proxy(VOTER_ACCOUNT, PROXY_ACCOUNT)
    node.wait_for_block_with_number(head_block_number + TIME_REQUIRED_TO_DECLINE_VOTING_RIGHTS)

    with pytest.raises(tt.exceptions.CommunicationError) as exception:
        wallet.api.set_voting_proxy(VOTER_ACCOUNT, PROXY_ACCOUNT)

    response = exception.value.response['error']['message']
    assert "Account has declined the ability to vote and cannot proxy votes." in response


@run_for("testnet")
def test_vesting_shares_balance_on_proxy_account_after_decline_voting_rights_of_the_principal_acc(prepare_environment):
    node, wallet = prepare_environment

    wallet.api.set_voting_proxy(VOTER_ACCOUNT, PROXY_ACCOUNT)

    node.wait_for_irreversible_block()
    node.restart(time_offset="+25h x5")

    assert int(node.api.wallet_bridge.get_account(PROXY_ACCOUNT)["proxied_vsf_votes"][0]) > 0

    wallet.api.decline_voting_rights(VOTER_ACCOUNT, True)
    node.wait_number_of_blocks(TIME_REQUIRED_TO_DECLINE_VOTING_RIGHTS)

    assert not any(node.api.wallet_bridge.get_account(PROXY_ACCOUNT)["proxied_vsf_votes"])
