from __future__ import annotations

import datetime

import pytest

import test_tools as tt
from hive_local_tools.constants import HIVE_CASHOUT_WINDOW_SECONDS
from hive_local_tools.functional.python.operation import Comment


@pytest.mark.parametrize("reply_type", ["reply_another_comment", "no_reply"], ids=["delete comment", "delete post"])
def test_delete_comment(prepared_node: tt.InitNode, wallet: tt.Wallet, reply_type: str) -> None:
    """
    Test cases 1, 2 from issue: https://gitlab.syncad.com/hive/hive/-/issues/504
    """
    comment_0 = Comment(prepared_node, wallet)
    if reply_type == "reply_another_comment":
        comment_0.create_parent_comment()

    comment_0.send(reply_type=reply_type)
    comment_0.delete()

    comment_0.assert_comment("deleted")
    comment_0.assert_is_rc_mana_decreased_after_comment_delete()


@pytest.mark.parametrize("reply_type", ["reply_another_comment", "no_reply"], ids=["delete comment", "delete post"])
def test_delete_comment_with_downvotes(prepared_node: tt.InitNode, wallet: tt.Wallet, reply_type: str) -> None:
    """
    Test cases 3, 4 from issue: https://gitlab.syncad.com/hive/hive/-/issues/504
    """
    comment_0 = Comment(prepared_node, wallet)
    if reply_type == "reply_another_comment":
        comment_0.create_parent_comment()

    comment_0.send(reply_type=reply_type)
    comment_0.downvote()
    comment_0.delete()

    comment_0.assert_comment("deleted")
    comment_0.assert_is_rc_mana_decreased_after_comment_delete()


@pytest.mark.parametrize("reply_type", ["reply_another_comment", "no_reply"], ids=["delete comment", "delete post"])
def test_try_to_delete_comment_with_votes(prepared_node: tt.InitNode, wallet: tt.Wallet, reply_type: str) -> None:
    """
    Test cases 5, 6 from issue: https://gitlab.syncad.com/hive/hive/-/issues/504
    """
    comment_0 = Comment(prepared_node, wallet)
    if reply_type == "reply_another_comment":
        comment_0.create_parent_comment()

    comment_0.send(reply_type=reply_type)
    comment_0.vote()

    with pytest.raises(tt.exceptions.CommunicationError) as error:
        comment_0.delete()

    assert "Cannot delete a comment with net positive votes" in error.value.error
    comment_0.assert_comment("not_deleted")


@pytest.mark.parametrize("reply_type", ["reply_another_comment", "no_reply"], ids=["delete comment", "delete post"])
def test_try_to_delete_comment_with_top_comment(prepared_node: tt.InitNode, wallet: tt.Wallet, reply_type: str) -> None:
    """
    Test cases 7, 8 from issue: https://gitlab.syncad.com/hive/hive/-/issues/504
    """
    comment_0 = Comment(prepared_node, wallet)
    if reply_type == "reply_another_comment":
        comment_0.create_parent_comment()

    comment_0.send(reply_type=reply_type)
    comment_0.reply(reply_type="reply_another_comment")
    with pytest.raises(tt.exceptions.CommunicationError) as error:
        comment_0.delete()

    assert "Cannot delete a comment with replies" in error.value.error
    comment_0.assert_comment("not_deleted")


@pytest.mark.parametrize("reply_type", ["reply_another_comment", "no_reply"], ids=["delete comment", "delete post"])
def test_try_to_delete_comment_after_payout(prepared_node: tt.InitNode, wallet: tt.Wallet, reply_type: str) -> None:
    """
    Test cases 9, 10 from issue: https://gitlab.syncad.com/hive/hive/-/issues/504
    """
    comment_0 = Comment(prepared_node, wallet)
    if reply_type == "reply_another_comment":
        comment_0.create_parent_comment()

    comment_0.send(reply_type=reply_type)

    # waiting for cashout 60 minutes
    time_offset = prepared_node.get_head_block_time() + datetime.timedelta(seconds=HIVE_CASHOUT_WINDOW_SECONDS)
    prepared_node.restart(time_offset=tt.Time.serialize(time_offset, format_=tt.TimeFormats.TIME_OFFSET_FORMAT))

    with pytest.raises(tt.exceptions.CommunicationError) as error:
        comment_0.delete()

    assert "Cannot delete comment after payout" in error.value.error
    comment_0.assert_comment("not_deleted")


@pytest.mark.parametrize("reply_type", ["reply_another_comment", "no_reply"], ids=["delete comment", "delete post"])
def test_reuse_deleted_permlink(prepared_node: tt.InitNode, wallet: tt.Wallet, reply_type: str) -> None:
    """
    Test cases 11, 12 from issue: https://gitlab.syncad.com/hive/hive/-/issues/504
    """
    comment_0 = Comment(prepared_node, wallet)
    if reply_type == "reply_another_comment":
        comment_0.create_parent_comment()

    comment_0.send(reply_type=reply_type)
    comment_0.delete()

    comment_0.assert_comment("deleted")
    comment_0.assert_is_rc_mana_decreased_after_comment_delete()

    # Skip 1 hour to create post again
    prepared_node.restart(time_offset="+1h")
    comment_0.send(reply_type=reply_type)

    comment_0.assert_is_rc_mana_decreased_after_post_or_update()
    comment_0.assert_is_comment_sent_or_update()
