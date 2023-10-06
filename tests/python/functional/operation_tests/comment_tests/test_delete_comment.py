import pytest

import test_tools as tt
from hive_local_tools.functional.python.operation import Comment


@pytest.mark.parametrize("reply_type", ["reply_another_comment", "no_reply"], ids=["delete comment", "delete post"])
def test_delete_comment(prepared_node, wallet, reply_type):
    """
    Test cases 1, 2 from issue: https://gitlab.syncad.com/hive/hive/-/issues/504
    """
    comment_0 = Comment(prepared_node, wallet)
    if reply_type == "reply_another_comment":
        comment_0.send_bottom_comment()

    comment_0.send(reply_type=reply_type)
    comment_0.delete()

    comment_0.assert_comment("deleted")
    comment_0.assert_is_rc_mana_decreased_after_comment_delete()


@pytest.mark.parametrize("reply_type", ["reply_another_comment", "no_reply"], ids=["delete comment", "delete post"])
def test_delete_comment_with_downvotes(prepared_node, wallet, reply_type):
    """
    Test cases 3, 4 from issue: https://gitlab.syncad.com/hive/hive/-/issues/504
    """
    comment_0 = Comment(prepared_node, wallet)
    if reply_type == "reply_another_comment":
        comment_0.send_bottom_comment()

    comment_0.send(reply_type=reply_type)
    comment_0.downvote()
    comment_0.delete()

    comment_0.assert_comment("deleted")
    comment_0.assert_is_rc_mana_decreased_after_comment_delete()


@pytest.mark.parametrize("reply_type", ["reply_another_comment", "no_reply"], ids=["delete comment", "delete post"])
def test_try_to_delete_comment_with_votes(prepared_node, wallet, reply_type):
    """
    Test cases 5, 6 from issue: https://gitlab.syncad.com/hive/hive/-/issues/504
    """
    comment_0 = Comment(prepared_node, wallet)
    if reply_type == "reply_another_comment":
        comment_0.send_bottom_comment()

    comment_0.send(reply_type=reply_type)
    comment_0.vote()

    with pytest.raises(tt.exceptions.CommunicationError) as error:
        comment_0.delete()

    assert "Cannot delete a comment with net positive votes" in error.value.response["error"]["message"]
    comment_0.assert_comment("not_deleted")


@pytest.mark.parametrize("reply_type", ["reply_another_comment", "no_reply"], ids=["delete comment", "delete post"])
def test_try_to_delete_comment_with_top_comment(prepared_node, wallet, reply_type):
    """
    Test cases 7, 8 from issue: https://gitlab.syncad.com/hive/hive/-/issues/504
    """
    comment_0 = Comment(prepared_node, wallet)
    if reply_type == "reply_another_comment":
        comment_0.send_bottom_comment()

    comment_0.send(reply_type=reply_type)
    comment_0.send_top_comment(reply_type="reply_another_comment")
    with pytest.raises(tt.exceptions.CommunicationError) as error:
        comment_0.delete()

    assert "Cannot delete a comment with replies" in error.value.response["error"]["message"]
    comment_0.assert_comment("not_deleted")


@pytest.mark.parametrize("reply_type", ["reply_another_comment", "no_reply"], ids=["delete comment", "delete post"])
def test_try_to_delete_comment_after_payout(prepared_node, wallet, reply_type):
    """
    Test cases 9, 10 from issue: https://gitlab.syncad.com/hive/hive/-/issues/504
    """
    comment_0 = Comment(prepared_node, wallet)
    if reply_type == "reply_another_comment":
        comment_0.send_bottom_comment()

    comment_0.send(reply_type=reply_type)
    # waiting for payout 7 days
    prepared_node.restart(time_offset="+7d")

    with pytest.raises(tt.exceptions.CommunicationError) as error:
        comment_0.delete()

    assert "Cannot delete comment after payout" in error.value.response["error"]["message"]
    comment_0.assert_comment("not_deleted")


@pytest.mark.parametrize("reply_type", ["reply_another_comment", "no_reply"], ids=["delete comment", "delete post"])
def test_reuse_deleted_permlink(prepared_node, wallet, reply_type):
    """
    Test cases 11, 12 from issue: https://gitlab.syncad.com/hive/hive/-/issues/504
    """
    comment_0 = Comment(prepared_node, wallet)
    if reply_type == "reply_another_comment":
        comment_0.send_bottom_comment()

    comment_0.send(reply_type=reply_type)
    comment_0.delete()

    comment_0.assert_comment("deleted")
    comment_0.assert_is_rc_mana_decreased_after_comment_delete()

    # Skip 1 hour to create post again
    if reply_type == "reply_another_comment":
        prepared_node.restart(time_offset="+1h")
    comment_0.send(reply_type=reply_type)

    comment_0.assert_is_rc_mana_decreased_after_post_or_update()
    comment_0.assert_is_comment_sent_or_update()
