from __future__ import annotations

import datetime

import pytest

import test_tools as tt
from hive_local_tools.constants import HIVE_CASHOUT_WINDOW_SECONDS
from hive_local_tools.functional.python.operation.comment import Comment


def test_if_comment_exist(prepared_node: tt.InitNode, wallet: tt.Wallet) -> None:
    """
    Test case 1 from issue: https://gitlab.syncad.com/hive/hive/-/issues/503
    """
    comment = Comment(prepared_node, wallet)
    comment.send(reply_type="no_reply")
    comment.assert_is_rc_mana_decreased_after_post_or_update()

    comment.assert_is_comment_sent_or_update()


@pytest.mark.parametrize(
    "reply_type", ["reply_own_comment", "reply_another_comment"], ids=["comment own post", "comment someone else post"]
)
def test_if_comment_with_parent_exist(prepared_node: tt.InitNode, wallet: tt.Wallet, reply_type: str) -> None:
    """
    Test cases 2, 3 from issue: https://gitlab.syncad.com/hive/hive/-/issues/503
    """
    comment = Comment(prepared_node, wallet)
    comment.create_parent_comment()
    comment.send(reply_type=reply_type)
    comment.assert_is_rc_mana_decreased_after_post_or_update()

    comment.assert_is_comment_sent_or_update()


@pytest.mark.parametrize("reply_type", ["reply_another_comment", "no_reply"], ids=["update comment", "update post"])
def test_update_comment_without_replies(prepared_node: tt.InitNode, wallet: tt.Wallet, reply_type: str) -> None:
    """
    Test cases 4, 5 from issue: https://gitlab.syncad.com/hive/hive/-/issues/503
    """
    comment = Comment(prepared_node, wallet)
    if reply_type == "reply_another_comment":
        comment.create_parent_comment()

    comment.send(reply_type=reply_type)

    comment.update()
    comment.assert_is_rc_mana_decreased_after_post_or_update()

    comment.assert_is_comment_sent_or_update()


@pytest.mark.parametrize("reply_type", ["reply_another_comment", "no_reply"], ids=["update comment", "update post"])
def test_update_comment_with_replies(prepared_node: tt.InitNode, wallet: tt.Wallet, reply_type: str) -> None:
    """
    Test cases 6, 7 from issue: https://gitlab.syncad.com/hive/hive/-/issues/503
    """
    comment = Comment(prepared_node, wallet)
    if reply_type == "reply_another_comment":
        comment.create_parent_comment()

    comment.send(reply_type=reply_type)

    comment.reply(reply_type="reply_another_comment")

    comment.update()
    comment.assert_is_rc_mana_decreased_after_post_or_update()

    comment.assert_is_comment_sent_or_update()


@pytest.mark.parametrize("reply_type", ["reply_another_comment", "no_reply"], ids=["update comment", "update post"])
def test_update_comment_with_replies_votes_and_downvotes(
    wallet: tt.Wallet, prepared_node: tt.InitNode, reply_type: str
) -> None:
    """
    Test cases 8, 9 from issue: https://gitlab.syncad.com/hive/hive/-/issues/503

    To compare whether the vote and downvote were changed after updating the comment, the updated comments are compared with not updated- the rewards should be the same.
    """
    # Verifying account
    author0_comment = Comment(prepared_node, wallet)
    # Supporting accounts
    author1_comment = Comment(prepared_node, wallet)
    author2_comment = Comment(prepared_node, wallet)
    author3_comment = Comment(prepared_node, wallet)

    comments = [author0_comment, author1_comment, author2_comment, author3_comment]

    for comment in comments:
        comment.create_parent_comment()
        comment.send(reply_type=reply_type)
        comment.reply(reply_type="reply_another_comment")

    # Not in single transaction, because it is possible to vote every 3 seconds.
    for comment in comments:
        comment.vote()

    author0_comment.downvote()
    author2_comment.downvote()

    author0_comment.update()
    author1_comment.update()

    # waiting for cashout 60 minutes
    start_time = prepared_node.get_head_block_time() + datetime.timedelta(seconds=HIVE_CASHOUT_WINDOW_SECONDS)
    prepared_node.restart(time_control=tt.StartTimeControl(start_time=start_time))

    reward_hbd_balances = []
    for comment in comments:
        reward_hbd_balances.append(wallet.api.get_account(comment.author).reward_hbd_balance)

    assert (
        reward_hbd_balances[2] < reward_hbd_balances[3]
    ), "HBD not reduce. Downvoting does not work correctly after comment update."
    assert reward_hbd_balances[0] < reward_hbd_balances[1], "HBD not reduce. Downvoting does not work correctly."
    assert (
        reward_hbd_balances[0] == reward_hbd_balances[2]
    ), "Account update have an impact on voting. Votes are changed"
    assert (
        reward_hbd_balances[1] == reward_hbd_balances[3]
    ), "Account update have an impact on downvoting. Downvotes are changed"

    author0_comment.assert_is_comment_sent_or_update()
    author0_comment.assert_is_rc_mana_decreased_after_post_or_update()


@pytest.mark.parametrize("reply_type", ["reply_another_comment", "no_reply"], ids=["update comment", "update post"])
def test_update_comment_with_replies_after_cashout(
    prepared_node: tt.InitNode, wallet: tt.Wallet, reply_type: str
) -> None:
    """
    Test cases 10, 11 from issue: https://gitlab.syncad.com/hive/hive/-/issues/503
    """
    comment = Comment(prepared_node, wallet)
    if reply_type == "reply_another_comment":
        comment.create_parent_comment()

    comment.send(reply_type=reply_type)

    comment.reply(reply_type="reply_another_comment")

    # vote is necessary to cashout
    comment.vote()

    # waiting for cashout 60 minutes
    start_time = prepared_node.get_head_block_time() + datetime.timedelta(seconds=HIVE_CASHOUT_WINDOW_SECONDS)
    prepared_node.restart(time_control=tt.StartTimeControl(start_time=start_time))

    comment.update()
    comment.assert_is_rc_mana_decreased_after_post_or_update()

    comment.assert_is_comment_sent_or_update()
