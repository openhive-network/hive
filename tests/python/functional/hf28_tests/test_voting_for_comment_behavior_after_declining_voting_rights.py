from __future__ import annotations

import pytest

import test_tools as tt
from hive_local_tools import run_for
from hive_local_tools.constants import TIME_REQUIRED_TO_DECLINE_VOTING_RIGHTS
from hive_local_tools.functional.python.hf28 import post_comment
from hive_local_tools.functional.python.operation import get_virtual_operations


@run_for("testnet")
def test_vote_for_comment_from_account_that_has_declined_its_voting_rights(node: tt.InitNode | tt.RemoteNode) -> None:
    wallet = tt.Wallet(attach_to=node)

    wallet.create_account("alice", vests=100)
    post_comment(wallet, number_of_comments=2)

    wallet.api.decline_voting_rights("alice", True)
    node.wait_number_of_blocks(TIME_REQUIRED_TO_DECLINE_VOTING_RIGHTS)

    with pytest.raises(tt.exceptions.CommunicationError) as exception:
        wallet.api.vote("alice", "creator-0", "comment-of-creator-0", 100)

    error_message = exception.value.response["error"]["message"]
    assert "Voter has declined their voting rights." in error_message


@run_for("testnet")
def test_if_vote_for_comment_made_before_declining_voting_rights_has_remained_active(
    prepare_environment: tuple[tt.InitNode, tt.Wallet]
) -> None:
    node, wallet = prepare_environment

    wallet.create_account("alice", vests=100)
    post_comment(wallet, number_of_comments=2)

    wallet.api.vote("alice", "creator-0", "comment-of-creator-0", 100)

    wallet.api.decline_voting_rights("alice", True)
    node.wait_number_of_blocks(TIME_REQUIRED_TO_DECLINE_VOTING_RIGHTS)

    # The account voted for the comment before declining vote rights the rights. This is correct behavior.
    assert len(node.api.condenser.get_active_votes("creator-0", "comment-of-creator-0")) == 1

    node.wait_for_irreversible_block()
    node.restart(time_offset="+62m")

    assert len(get_virtual_operations(node, "curation_reward_operation")) == 1
    assert node.api.wallet_bridge.get_accounts(["alice"])[0].reward_vesting_balance > tt.Asset.Vest(0)
    assert node.api.wallet_bridge.get_accounts(["alice"])[0].reward_vesting_hive > tt.Asset.Test(0)


@run_for("testnet")
def test_vote_for_comment_when_decline_voting_rights_is_being_executed(
    prepare_environment: tuple[tt.InitNode, tt.Wallet]
) -> None:
    node, wallet = prepare_environment

    wallet.create_account("alice", vests=100)

    post_comment(wallet, number_of_comments=2)

    # decline voting rights -> vote for comment (before approving the decline of voting rights) -> wait remaining time
    head_block_number = wallet.api.decline_voting_rights("alice", True)["block_num"]
    node.wait_for_block_with_number(head_block_number + (TIME_REQUIRED_TO_DECLINE_VOTING_RIGHTS // 2))
    wallet.api.vote("alice", "creator-0", "comment-of-creator-0", 100)
    node.wait_for_block_with_number(head_block_number + TIME_REQUIRED_TO_DECLINE_VOTING_RIGHTS)

    assert len(node.api.condenser.get_active_votes("creator-0", "comment-of-creator-0")) == 1

    node.wait_for_irreversible_block()
    node.restart(time_offset="+62m")

    assert len(get_virtual_operations(node, "curation_reward_operation")) == 1
    assert node.api.wallet_bridge.get_accounts(["alice"])[0].reward_vesting_balance > tt.Asset.Vest(0)
    assert node.api.wallet_bridge.get_accounts(["alice"])[0].reward_vesting_hive > tt.Asset.Test(0)


@run_for("testnet")
def test_edit_comment_vote_without_voting_rights_before_comment_reward_pay_out(node: tt.InitNode | tt.RemoteNode):
    wallet = tt.Wallet(attach_to=node)
    wallet.create_account("alice", vests=100)

    post_comment(wallet, number_of_comments=2)
    wallet.api.vote("alice", "creator-0", "comment-of-creator-0", 100)

    wallet.api.decline_voting_rights("alice", True)
    node.wait_number_of_blocks(TIME_REQUIRED_TO_DECLINE_VOTING_RIGHTS)

    with pytest.raises(tt.exceptions.CommunicationError):
        wallet.api.vote("alice", "creator-0", "comment-of-creator-0", 50)


@run_for("testnet", enable_plugins=["account_history_api"])
def test_payout_rewards_for_comment_vote_without_voting_rights(node: tt.InitNode | tt.RemoteNode) -> None:
    wallet = tt.Wallet(attach_to=node)
    wallet.create_account("alice", vests=100_000_000)

    accounts = wallet.create_accounts(number_of_accounts=3, name_base="creator")
    with wallet.in_single_transaction():
        for account in accounts:
            wallet.api.transfer_to_vesting("initminer", account.name, tt.Asset.Test(1))

    # Create first comment - +0s
    wallet.api.post_comment("creator-0", "comment-of-creator-0", "", "someone0", "test-title", "this is a body", "{}")

    # Create second comment - +15s
    node.wait_number_of_blocks(5)
    wallet.api.post_comment("creator-1", "comment-of-creator-1", "", "someone1", "test-title", "this is a body", "{}")

    # Create second comment - +30s
    node.wait_number_of_blocks(5)
    wallet.api.post_comment("creator-2", "comment-of-creator-2", "", "someone2", "test-title", "this is a body", "{}")

    node.wait_for_irreversible_block()
    node.restart(time_offset="+58m")

    # 60s to reward of comment-0
    node.wait_number_of_blocks(2)
    wallet.api.vote("alice", "creator-0", "comment-of-creator-0", 100)

    # 60s to activate decline_voting_rights_operation
    wallet.api.decline_voting_rights("alice", True)

    wallet.api.vote("alice", "creator-1", "comment-of-creator-1", 100)
    node.wait_number_of_blocks(3)
    wallet.api.vote("alice", "creator-2", "comment-of-creator-2", 100)
    node.wait_number_of_blocks(21)

    vops = node.api.account_history.enum_virtual_ops(
        block_range_begin=0,
        block_range_end=2000,
        limit=2000,
        include_reversible=True,
        # filter only comment_payout_update and declined_voting_rights op
        filter=int("0x000800", base=16) + int("0x40000000000", base=16),
    )

    # The virtual operation confirms that decline_voting_rights_operation was done.
    assert vops["ops"][2]["op"]["type"] == "declined_voting_rights_operation"
    assert len(get_virtual_operations(node, "declined_voting_rights_operation")) == 1

    assert node.api.wallet_bridge.get_accounts(["alice"])[0]["reward_vesting_balance"] > tt.Asset.Vest(0)
    assert node.api.wallet_bridge.get_accounts(["alice"])[0]["reward_vesting_hive"] > tt.Asset.Test(0)
