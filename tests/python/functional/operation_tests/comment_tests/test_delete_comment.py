from __future__ import annotations

import datetime

import pytest
from beekeepy.exceptions import ErrorInResponseError

import test_tools as tt
from hive_local_tools.constants import HIVE_CASHOUT_WINDOW_SECONDS
from hive_local_tools.functional.python.operation.comment import Comment


@pytest.mark.parametrize("reply_type", ["reply_another_comment", "no_reply"], ids=["delete comment", "delete post"])
def test_delete_comment(prepared_node: tt.InitNode, wallet: tt.Wallet, reply_type: str) -> None:
    """
    Test cases 1, 2 from issue: https://gitlab.syncad.com/hive/hive/-/issues/504
    """
    comment = Comment(prepared_node, wallet)
    if reply_type == "reply_another_comment":
        comment.create_parent_comment()

    comment.send(reply_type=reply_type)
    comment.delete()

    comment.assert_comment("deleted")
    comment.assert_is_rc_mana_decreased_after_comment_delete()


@pytest.mark.parametrize("reply_type", ["reply_another_comment", "no_reply"], ids=["delete comment", "delete post"])
def test_delete_comment_with_downvotes(prepared_node: tt.InitNode, wallet: tt.Wallet, reply_type: str) -> None:
    """
    Test cases 3, 4 from issue: https://gitlab.syncad.com/hive/hive/-/issues/504
    """
    comment = Comment(prepared_node, wallet)
    if reply_type == "reply_another_comment":
        comment.create_parent_comment()

    comment.send(reply_type=reply_type)
    comment.downvote()
    comment.delete()

    comment.assert_comment("deleted")
    comment.assert_is_rc_mana_decreased_after_comment_delete()


@pytest.mark.parametrize("reply_type", ["reply_another_comment", "no_reply"], ids=["delete comment", "delete post"])
def test_try_to_delete_comment_with_votes(prepared_node: tt.InitNode, wallet: tt.Wallet, reply_type: str) -> None:
    """
    Test cases 5, 6 from issue: https://gitlab.syncad.com/hive/hive/-/issues/504
    """
    comment = Comment(prepared_node, wallet)
    if reply_type == "reply_another_comment":
        comment.create_parent_comment()

    comment.send(reply_type=reply_type)
    comment.vote()

    with pytest.raises(ErrorInResponseError) as error:
        comment.delete()

    assert "Cannot delete a comment with net positive votes" in error.value.error
    comment.assert_comment("not_deleted")


@pytest.mark.parametrize("reply_type", ["reply_another_comment", "no_reply"], ids=["delete comment", "delete post"])
def test_try_to_delete_comment_with_top_comment(prepared_node: tt.InitNode, wallet: tt.Wallet, reply_type: str) -> None:
    """
    Test cases 7, 8 from issue: https://gitlab.syncad.com/hive/hive/-/issues/504
    """
    comment = Comment(prepared_node, wallet)
    if reply_type == "reply_another_comment":
        comment.create_parent_comment()

    comment.send(reply_type=reply_type)
    comment.reply(reply_type="reply_another_comment")
    with pytest.raises(ErrorInResponseError) as error:
        comment.delete()

    assert "Cannot delete a comment with replies" in error.value.error
    comment.assert_comment("not_deleted")


@pytest.mark.parametrize("reply_type", ["reply_another_comment", "no_reply"], ids=["delete comment", "delete post"])
def test_try_to_delete_comment_after_payout(prepared_node: tt.InitNode, wallet: tt.Wallet, reply_type: str) -> None:
    """
    Test cases 9, 10 from issue: https://gitlab.syncad.com/hive/hive/-/issues/504
    """
    comment = Comment(prepared_node, wallet)
    if reply_type == "reply_another_comment":
        comment.create_parent_comment()

    comment.send(reply_type=reply_type)

    # waiting for cashout 60 minutes
    start_time = prepared_node.get_head_block_time() + datetime.timedelta(seconds=HIVE_CASHOUT_WINDOW_SECONDS)
    prepared_node.restart(time_control=tt.StartTimeControl(start_time=start_time))

    with pytest.raises(ErrorInResponseError) as error:
        comment.delete()

    assert "Cannot delete comment after payout" in error.value.error
    comment.assert_comment("not_deleted")


@pytest.mark.parametrize("reply_type", ["reply_another_comment", "no_reply"], ids=["delete comment", "delete post"])
def test_reuse_deleted_permlink(prepared_node: tt.InitNode, wallet: tt.Wallet, reply_type: str) -> None:
    """
    Test cases 11, 12 from issue: https://gitlab.syncad.com/hive/hive/-/issues/504
    """
    comment = Comment(prepared_node, wallet)
    if reply_type == "reply_another_comment":
        comment.create_parent_comment()

    comment.send(reply_type=reply_type)
    comment.delete()

    comment.assert_comment("deleted")
    comment.assert_is_rc_mana_decreased_after_comment_delete()

    # Skip 1 hour to create post again
    prepared_node.restart(time_control=tt.OffsetTimeControl(offset="+1h"))
    comment.send(reply_type=reply_type)

    comment.assert_is_rc_mana_decreased_after_post_or_update()
    comment.assert_is_comment_sent_or_update()
