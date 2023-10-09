import pytest

import test_tools as tt
from hive_local_tools import run_for
from hive_local_tools.constants import TIME_REQUIRED_TO_DECLINE_VOTING_RIGHTS
from hive_local_tools.functional.python.hf28.constants import PROXY_ACCOUNT, VOTER_ACCOUNT
from hive_local_tools.functional.python.operation import get_virtual_operations


@run_for("testnet")
def test_proxy_before_waiving_voting_rights(prepare_environment):
    node, wallet = prepare_environment

    wallet.api.set_voting_proxy(VOTER_ACCOUNT, PROXY_ACCOUNT)

    wallet.api.decline_voting_rights(VOTER_ACCOUNT, True)
    assert node.api.wallet_bridge.get_account(VOTER_ACCOUNT)["proxy"] == PROXY_ACCOUNT
    node.wait_number_of_blocks(TIME_REQUIRED_TO_DECLINE_VOTING_RIGHTS)

    assert len(get_virtual_operations(node, "proxy_cleared_operation")) == 1
    assert node.api.wallet_bridge.get_account(VOTER_ACCOUNT)["proxy"] == ""

    node.restart(time_offset="+25h")
    assert not any(node.api.wallet_bridge.get_account(PROXY_ACCOUNT)["proxied_vsf_votes"])


@run_for("testnet")
def test_set_proxy_after_waiving_voting_rights(prepare_environment):
    node, wallet = prepare_environment

    wallet.api.decline_voting_rights(VOTER_ACCOUNT, True)
    node.wait_number_of_blocks(TIME_REQUIRED_TO_DECLINE_VOTING_RIGHTS)

    with pytest.raises(tt.exceptions.CommunicationError) as exception:
        wallet.api.set_voting_proxy(VOTER_ACCOUNT, PROXY_ACCOUNT)

    response = exception.value.response["error"]["message"]
    assert "Account has declined the ability to vote and cannot proxy votes." in response


@run_for("testnet")
def test_set_proxy_when_decline_voting_rights_is_in_progress(prepare_environment):
    node, wallet = prepare_environment

    head_block_number = wallet.api.decline_voting_rights(VOTER_ACCOUNT, True)["block_num"]
    node.wait_for_block_with_number(head_block_number + (TIME_REQUIRED_TO_DECLINE_VOTING_RIGHTS // 2))
    wallet.api.set_voting_proxy(VOTER_ACCOUNT, PROXY_ACCOUNT)
    node.wait_for_block_with_number(head_block_number + TIME_REQUIRED_TO_DECLINE_VOTING_RIGHTS)

    assert len(get_virtual_operations(node, "proxy_cleared_operation")) == 1
    node.restart(time_offset="+25h")
    assert node.api.wallet_bridge.get_account(VOTER_ACCOUNT)["proxy"] == ""
    assert not any(node.api.wallet_bridge.get_account(PROXY_ACCOUNT)["proxied_vsf_votes"])


@run_for("testnet")
def test_proxied_vsf_votes_when_principal_account_declined_its_voting_rights(prepare_environment):
    node, wallet = prepare_environment

    wallet.api.set_voting_proxy(VOTER_ACCOUNT, PROXY_ACCOUNT)

    node.wait_for_irreversible_block()
    node.restart(time_offset="+25h x5")

    assert int(node.api.wallet_bridge.get_account(PROXY_ACCOUNT)["proxied_vsf_votes"][0]) > 0

    wallet.api.decline_voting_rights(VOTER_ACCOUNT, True)
    node.wait_number_of_blocks(TIME_REQUIRED_TO_DECLINE_VOTING_RIGHTS)

    assert node.api.wallet_bridge.get_account(VOTER_ACCOUNT)["proxy"] == ""
    assert len(get_virtual_operations(node, "proxy_cleared_operation")) == 1
    assert not any(node.api.wallet_bridge.get_account(PROXY_ACCOUNT)["proxied_vsf_votes"])
