from __future__ import annotations

import test_tools as tt
from hive_local_tools import run_for
from hive_local_tools.constants import TIME_REQUIRED_TO_DECLINE_VOTING_RIGHTS
from hive_local_tools.functional.python.hf28 import post_comment
from hive_local_tools.functional.python.operation import Account, get_transaction_timestamp, get_virtual_operations
from schemas.operations.virtual import DeclinedVotingRightsOperation


@run_for("testnet", enable_plugins=["account_history_api"])
def test_delegate_vesting_shares_without_voting_rights(node: tt.InitNode) -> None:
    """
    Problem description: https://gitlab.syncad.com/hive/hive/-/issues/463
    """
    node.restart(time_control=tt.SpeedUpRateTimeControl(speed_up_rate=5))
    wallet = tt.Wallet(attach_to=node)

    node.set_vest_price(tt.Asset.Vest(1800))

    wallet.create_account("alice", vests=100_000)
    alice = Account("alice", node, wallet)

    wallet.create_account("bob")

    transaction = wallet.api.decline_voting_rights("alice", True)
    alice.rc_manabar.assert_rc_current_mana_is_reduced(
        transaction["rc_cost"], get_transaction_timestamp(node, transaction)
    )
    assert len(node.api.database.find_decline_voting_rights_requests(accounts=["alice"])["requests"]) == 1

    node.wait_number_of_blocks(TIME_REQUIRED_TO_DECLINE_VOTING_RIGHTS)

    assert node.api.database.find_accounts(accounts=["alice"]).accounts[0].can_vote is False
    assert len(node.api.database.find_decline_voting_rights_requests(accounts=["alice"])["requests"]) == 0
    assert len(get_virtual_operations(node, DeclinedVotingRightsOperation)) == 1

    wallet.api.delegate_vesting_shares("alice", "bob", tt.Asset.Vest(5))

    assert node.api.wallet_bridge.get_accounts(["bob"])[0].post_voting_power == tt.Asset.Vest(5)


@run_for("testnet", enable_plugins=["account_history_api"])
def test_vote_for_comment_with_vests_from_delegation_before_decline_voting_rights(node: tt.InitNode) -> None:
    node.restart(time_control=tt.SpeedUpRateTimeControl(speed_up_rate=5))
    wallet = tt.Wallet(attach_to=node)
    node.set_vest_price(tt.Asset.Vest(1800))

    wallet.create_account("alice", vests=100_000)
    alice = Account("alice", node, wallet)
    wallet.create_account("bob")
    bob = Account("bob", node, wallet)
    post_comment(wallet, number_of_comments=2)

    wallet.api.delegate_vesting_shares("alice", "bob", tt.Asset.Vest(10_000))
    bob.rc_manabar.update()
    transaction = wallet.api.vote("bob", "creator-0", "comment-of-creator-0", 100)
    bob.rc_manabar.assert_rc_current_mana_is_reduced(
        transaction["rc_cost"], get_transaction_timestamp(node, transaction)
    )

    alice.rc_manabar.update()
    transaction = wallet.api.decline_voting_rights("alice", True)
    alice.rc_manabar.assert_rc_current_mana_is_reduced(
        transaction["rc_cost"], get_transaction_timestamp(node, transaction)
    )
    assert node.api.database.get_comment_pending_payouts(comments=[["creator-0", "comment-of-creator-0"]]).cashout_infos[0].cashout_info.net_votes == 1
    assert len(node.api.database.find_decline_voting_rights_requests(accounts=["alice"])["requests"]) == 1
    node.wait_number_of_blocks(TIME_REQUIRED_TO_DECLINE_VOTING_RIGHTS)

    assert node.api.database.find_accounts(accounts=["alice"]).accounts[0].can_vote is False
    assert len(node.api.database.find_decline_voting_rights_requests(accounts=["alice"])["requests"]) == 0
    assert len(get_virtual_operations(node, DeclinedVotingRightsOperation)) == 1
    assert node.api.database.get_comment_pending_payouts(comments=[["creator-0", "comment-of-creator-0"]]).cashout_infos[0].cashout_info.net_votes == 1
    node.wait_for_irreversible_block()

    node.restart(time_control=tt.OffsetTimeControl(offset="+62m"))
    bob = node.api.wallet_bridge.get_accounts(["bob"])[0]
    assert bob.reward_vesting_balance > tt.Asset.Vest(0)
    assert bob.reward_vesting_hive > tt.Asset.Test(0)


@run_for("testnet", enable_plugins=["account_history_api"])
def test_vote_for_comment_with_vests_from_delegation_when_decline_voting_rights_is_being_executed(
    node: tt.InitNode,
) -> None:
    node.restart(time_control=tt.SpeedUpRateTimeControl(speed_up_rate=5))
    wallet = tt.Wallet(attach_to=node)

    node.set_vest_price(tt.Asset.Vest(1800))

    wallet.create_account("alice", vests=100_000)
    alice = Account("alice", node, wallet)

    wallet.create_account("bob")
    bob = Account("bob", node, wallet)
    post_comment(wallet, number_of_comments=2)

    transaction = wallet.api.decline_voting_rights("alice", True)
    alice.rc_manabar.assert_rc_current_mana_is_reduced(
        transaction["rc_cost"], get_transaction_timestamp(node, transaction)
    )

    assert len(node.api.database.find_decline_voting_rights_requests(accounts=["alice"])["requests"]) == 1

    wallet.api.delegate_vesting_shares("alice", "bob", tt.Asset.Vest(10_000))
    bob.rc_manabar.update()
    transaction = wallet.api.vote("bob", "creator-0", "comment-of-creator-0", 100)
    bob.rc_manabar.assert_rc_current_mana_is_reduced(
        transaction["rc_cost"], get_transaction_timestamp(node, transaction)
    )

    node.wait_number_of_blocks(TIME_REQUIRED_TO_DECLINE_VOTING_RIGHTS)

    assert node.api.database.find_accounts(accounts=["alice"]).accounts[0].can_vote is False
    assert len(node.api.database.find_decline_voting_rights_requests(accounts=["alice"])["requests"]) == 0
    assert len(get_virtual_operations(node, DeclinedVotingRightsOperation)) == 1

    node.restart(time_control=tt.OffsetTimeControl(offset="+62m"))
    bob = node.api.wallet_bridge.get_accounts(["bob"])[0]
    assert bob.reward_vesting_balance > tt.Asset.Vest(0)
    assert bob.reward_vesting_hive > tt.Asset.Test(0)


@run_for("testnet", enable_plugins=["account_history_api"])
def test_vote_for_comment_with_vests_from_delegation_after_creating_a_decline_voting_rights_request(
    node: tt.InitNode,
) -> None:
    node.restart(time_control=tt.SpeedUpRateTimeControl(speed_up_rate=5))
    wallet = tt.Wallet(attach_to=node)

    node.set_vest_price(tt.Asset.Vest(1800))

    wallet.create_account("alice", vests=100_000)
    alice = Account("alice", node, wallet)
    wallet.create_account("bob")
    post_comment(wallet, number_of_comments=2)

    wallet.api.delegate_vesting_shares("alice", "bob", tt.Asset.Vest(10_000))
    alice.rc_manabar.update()
    transaction = wallet.api.decline_voting_rights("alice", True)
    alice.rc_manabar.assert_rc_current_mana_is_reduced(
        transaction["rc_cost"], get_transaction_timestamp(node, transaction)
    )
    assert len(node.api.database.find_decline_voting_rights_requests(accounts=["alice"])["requests"]) == 1

    wallet.api.vote("bob", "creator-0", "comment-of-creator-0", 100)
    assert node.api.database.get_comment_pending_payouts(comments=[["creator-0", "comment-of-creator-0"]]).cashout_infos[0].cashout_info.net_votes == 1
    node.wait_number_of_blocks(TIME_REQUIRED_TO_DECLINE_VOTING_RIGHTS)

    assert node.api.database.find_accounts(accounts=["alice"]).accounts[0].can_vote is False
    assert len(node.api.database.find_decline_voting_rights_requests(accounts=["alice"])["requests"]) == 0
    assert len(get_virtual_operations(node, DeclinedVotingRightsOperation)) == 1
    assert node.api.database.get_comment_pending_payouts(comments=[["creator-0", "comment-of-creator-0"]]).cashout_infos[0].cashout_info.net_votes == 1
    node.restart(time_control=tt.OffsetTimeControl(offset="+62m"))
    bob = node.api.wallet_bridge.get_accounts(["bob"])[0]
    assert bob.reward_vesting_balance > tt.Asset.Vest(0)
    assert bob.reward_vesting_hive > tt.Asset.Test(0)
