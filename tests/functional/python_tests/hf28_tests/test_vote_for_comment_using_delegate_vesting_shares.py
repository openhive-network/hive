import test_tools as tt
from hive_local_tools import run_for
from hive_local_tools.constants import TIME_REQUIRED_TO_DECLINE_VOTING_RIGHTS
from hive_local_tools.functional.python.hf28 import post_comment, stabilize_the_price


@run_for("testnet")
def test_delegate_vesting_shares_without_voting_rights(node):
    """
    Problem description: https://gitlab.syncad.com/hive/hive/-/issues/463
    """
    node.restart(time_offset="+0h x5")
    wallet = tt.Wallet(attach_to=node)

    stabilize_the_price(node)

    wallet.create_account("alice", vests=100_000)

    wallet.create_account("bob")

    wallet.api.decline_voting_rights("alice", True)
    node.wait_number_of_blocks(TIME_REQUIRED_TO_DECLINE_VOTING_RIGHTS)

    wallet.api.delegate_vesting_shares("alice", "bob", tt.Asset.Vest(5))

    assert node.api.wallet_bridge.get_accounts(["bob"])[0]["post_voting_power"] == tt.Asset.Vest(5)


@run_for("testnet")
def test_vote_for_comment_with_vests_from_delegation_before_decline_voting_rights(node):
    node.restart(time_offset="+0h x5")
    wallet = tt.Wallet(attach_to=node)

    stabilize_the_price(node)

    wallet.create_account("alice", vests=100_000)
    wallet.create_account("bob")
    post_comment(wallet, number_of_comments=2)

    wallet.api.delegate_vesting_shares("alice", "bob", tt.Asset.Vest(10_000))
    wallet.api.vote("bob", "creator-0", "comment-of-creator-0", 100)

    wallet.api.decline_voting_rights("alice", True)
    node.wait_number_of_blocks(TIME_REQUIRED_TO_DECLINE_VOTING_RIGHTS)
    node.wait_for_irreversible_block()

    node.restart(time_offset="+62m")
    assert node.api.wallet_bridge.get_accounts(["creator-0"])[0]["reward_hbd_balance"] > tt.Asset.Tbd(0)


@run_for("testnet")
def test_vote_for_comment_with_vests_from_delegation_when_decline_voting_rights_is_being_executed(node):
    node.restart(time_offset="+0h x5")
    wallet = tt.Wallet(attach_to=node)

    stabilize_the_price(node)

    wallet.create_account("alice", vests=100_000)
    wallet.create_account("bob")
    post_comment(wallet, number_of_comments=2)

    wallet.api.decline_voting_rights("alice", True)

    wallet.api.delegate_vesting_shares("alice", "bob", tt.Asset.Vest(10_000))
    wallet.api.vote("bob", "creator-0", "comment-of-creator-0", 100)
    node.wait_number_of_blocks(TIME_REQUIRED_TO_DECLINE_VOTING_RIGHTS)

    node.restart(time_offset="+62m")
    assert node.api.wallet_bridge.get_accounts(["creator-0"])[0]["reward_hbd_balance"] > tt.Asset.Tbd(0)


@run_for("testnet")
def test_vote_for_comment_with_vests_from_delegation_after_creating_a_decline_voting_rights_request(node):
    node.restart(time_offset="+0h x5")
    wallet = tt.Wallet(attach_to=node)

    stabilize_the_price(node)

    wallet.create_account("alice", vests=100_000)
    wallet.create_account("bob")
    post_comment(wallet, number_of_comments=2)

    wallet.api.delegate_vesting_shares("alice", "bob", tt.Asset.Vest(10_000))

    wallet.api.decline_voting_rights("alice", True)

    wallet.api.vote("bob", "creator-0", "comment-of-creator-0", 100)
    node.wait_number_of_blocks(TIME_REQUIRED_TO_DECLINE_VOTING_RIGHTS)

    node.restart(time_offset="+62m")
    assert (
            int(
                node.api.wallet_bridge.get_accounts(["creator-0"])[0][
                    "reward_hbd_balance"
                ]["amount"]
            )
            > tt.Asset.Tbd(0).amount
    )
