import pytest

import test_tools as tt
from hive_local_tools import run_for
from hive_local_tools.functional.python.hf28 import post_comment
from hive_local_tools.functional.python.hf28.constants import TIME_REQUIRED_TO_DECLINE_VOTING_RIGHTS


# @run_for("testnet")
# def test_vote_for_comment_after_decline_voting_rights(node):
#     wallet = tt.Wallet(attach_to=node)
#
#     wallet.create_account("alice", vests=100)
#     post_comment(wallet, number_of_comments=3)
#
#     wallet.api.vote("alice", "creator-0", "comment-of-creator-0", 100)
#
#     wallet.api.decline_voting_rights("alice", True)
#
#     wallet.api.vote("alice", "creator-1", "comment-of-creator-1", 100)
#     node.wait_number_of_blocks(TIME_REQUIRED_TO_DECLINE_VOTING_RIGHTS)
#
#     with pytest.raises(tt.exceptions.CommunicationError) as exception:
#         wallet.api.vote("alice", "creator-2", "comment-of-creator-2", 100)
#
#     error_message = exception.value.response["error"]["message"]
#     assert "Voter has declined their voting rights." in error_message
#
#
# @run_for("testnet")
# def test_vote_for_comment_before_decline_voting_rights(node):
#     wallet = tt.Wallet(attach_to=node)
#
#     wallet.create_account("alice", vests=100)
#
#     post_comment(wallet, number_of_comments=2)
#     wallet.api.vote("alice", "creator-0", "comment-of-creator-0", 100)
#
#     wallet.api.decline_voting_rights("alice", True)
#     node.wait_number_of_blocks(TIME_REQUIRED_TO_DECLINE_VOTING_RIGHTS)
#
#     with pytest.raises(tt.exceptions.CommunicationError) as exception:
#         wallet.api.vote("alice", "creator-1", "comment-of-creator-1", 100)
#
#     error_message = exception.value.response["error"]["message"]
#     assert "Voter has declined their voting rights." in error_message


@run_for("testnet")
def test_alicja_i_alicja2_2a(node):
    wallet = tt.Wallet(attach_to=node)

    wallet.create_account("alice", vests=100)

    wallet.create_account("bob")

    wallet.api.delegate_vesting_shares("alice", "bob", tt.Asset.Vest(5))

    post_comment(wallet, number_of_comments=3)

    wallet.api.vote("bob", "creator-0", "comment-of-creator-0", 100)

    wallet.api.decline_voting_rights("alice", True)

    wallet.api.vote("bob", "creator-1", "comment-of-creator-1", 100)

    node.wait_number_of_blocks(TIME_REQUIRED_TO_DECLINE_VOTING_RIGHTS)

    # TODO: tu chyba powinien lecieć wyjątek
    wallet.api.vote("bob", "creator-2", "comment-of-creator-2", 100)


#TODO: CO TO MA TESTOWAĆ?
@run_for("testnet")
def test_2b(node):
    wallet = tt.Wallet(attach_to=node)

    wallet.create_account("alice", vests=100)

    wallet.create_account("bob")

    post_comment(wallet, number_of_comments=3)

    wallet.api.vote("bob", "creator-0", "comment-of-creator-0", 100)

    wallet.api.decline_voting_rights("alice", True)

    wallet.api.delegate_vesting_shares("alice", "bob", tt.Asset.Vest(5))


# @run_for("testnet")
# def test_wyplaty_za_commnet(node):
#     wallet = tt.Wallet(attach_to=node)
#     wallet.create_account("alice", vests=9999999)
#     post_comment(wallet, number_of_comments=3)
#     wallet.api.vote("alice", "creator-0", "comment-of-creator-0", 100)
#     lis_com = node.api.database.find_comments(comments=[["creator-0", "comment-of-creator-0"]])
#     node.wait_for_irreversible_block()
#     node.restart(time_offset="+58m")  # Czas na wypłatę środków a komentarz to 60 min
#     c1 =  node.api.condenser.get_accounts(["creator-0"])[0]["reward_hbd_balance"]
#     node.wait_number_of_blocks(21)
#     c2 =  node.api.condenser.get_accounts(["creator-0"])[0]["reward_hbd_balance"]
#     print()


def post_comment2(wallet: tt.Wallet, account_name: str):
    wallet.create_account(name=account_name, vests=tt.Asset.Test(1))
    wallet.api.post_comment(f"{account_name}", f"comment-of-{account_name}", "", "someone",
                            "test-title", "this is a body", "{}")


def get_head_block_time(node):
    headblock_number = node.get_last_block_number()
    timestamp = node.api.block.get_block(block_num= headblock_number)["block"]["timestamp"]
    return timestamp


@run_for("testnet")
def test_wyplaty_za_komentarz (node):
    wallet = tt.Wallet(attach_to=node)
    wallet.create_account("alice", vests=100_000_000)
    wallet.api.set_voting_proxy("alice", "initminer")

    accounts = wallet.create_accounts(number_of_accounts=3, name_base="creator")
    with wallet.in_single_transaction():
        for account in accounts:
            wallet.api.transfer_to_vesting('initminer', account.name, tt.Asset.Test(1))

    # Create first comment - +0s
    wallet.api.post_comment(f"creator-0", f"comment-of-creator-0", "", "someone0",
                            "test-title", "this is a body", "{}")

    # Create second comment - +15s
    node.wait_number_of_blocks(5)
    wallet.api.post_comment(f"creator-1", f"comment-of-creator-1", "", "someone1",
                            "test-title", "this is a body", "{}")

    node.wait_for_irreversible_block()
    node.restart(time_offset="+58m")

    #TODO: tuaj dokończyć
