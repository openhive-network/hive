from __future__ import annotations

import datetime

import pytest

import test_tools as tt
from hive_local_tools.constants import HIVE_CASHOUT_WINDOW_SECONDS
from hive_local_tools.functional.python.operation.comment import Comment, Vote


@pytest.fixture()
def speed_up_node() -> tt.InitNode:
    node = tt.InitNode()
    node.config.plugin.append("account_history_api")
    node.config.plugin.append("reputation_api")
    node.run(time_offset="+0h x5")
    return node


@pytest.mark.parametrize("reply_type", ["reply_another_comment", "no_reply"], ids=["downvote comment", "downvote post"])
def test_downvote_by_account_with_higher_reputation_before_paid_out(
    prepared_node: tt.InitNode, wallet: tt.Wallet, reply_type
) -> None:
    """
    Test cases 41, 42 from issue: https://gitlab.syncad.com/hive/hive/-/issues/505
    """
    comment1 = Comment(prepared_node, wallet)
    comment2 = Comment(prepared_node, wallet)

    if reply_type == "reply_another_comment":
        comment2.create_parent_comment()

    # Prepare account with higher reputation
    comment1.send("no_reply")
    vote1 = Vote(comment1, "random")
    vote1.vote(100)

    # Prepare account with lower reputation
    comment2.send(reply_type)
    vote2 = Vote(comment2, "random")
    vote2.vote(10)

    assert comment1.get_author_reputation() > comment2.get_author_reputation()
    account_reputation_before_downvote = comment2.get_author_reputation()

    # Account with higher reputation downvote account with lower reputation
    vote3 = Vote(comment2, voter=comment1.author_obj)
    vote3.downvote(-50)

    account_reputation_after_downvote = comment2.get_author_reputation()
    assert account_reputation_after_downvote < account_reputation_before_downvote

    vote3.assert_vote("occurred")
    vote3.assert_rc_mana_after_vote_or_downvote("decrease")
    vote3.assert_vote_or_downvote_manabar("downvote_manabar", "decrease")
    vote3.assert_effective_comment_vote_operation("generated")


@pytest.mark.parametrize("reply_type", ["reply_another_comment", "no_reply"], ids=["downvote comment", "downvote post"])
def test_downvote_by_account_with_higher_reputation_after_paid_out(
    prepared_node: tt.InitNode, wallet: tt.Wallet, reply_type
) -> None:
    """
    Test cases 59, 60 from issue: https://gitlab.syncad.com/hive/hive/-/issues/505
    """
    comment1 = Comment(prepared_node, wallet)
    comment2 = Comment(prepared_node, wallet)

    if reply_type == "reply_another_comment":
        comment2.create_parent_comment()

    # Prepare account with higher reputation
    comment1.send("no_reply")
    vote1 = Vote(comment1, "random")
    vote1.vote(100)

    # Prepare account with lower reputation
    comment2.send(reply_type)
    vote2 = Vote(comment2, "random")
    vote2.vote(10)

    assert comment1.get_author_reputation() > comment2.get_author_reputation()
    account_reputation_before_downvote = comment2.get_author_reputation()

    # waiting for cashout 60 minutes
    time_offset = prepared_node.get_head_block_time() + datetime.timedelta(seconds=HIVE_CASHOUT_WINDOW_SECONDS)
    prepared_node.restart(time_offset=tt.Time.serialize(time_offset, format_=tt.TimeFormats.TIME_OFFSET_FORMAT))

    # Account with higher reputation downvote account with lower reputation
    vote3 = Vote(comment2, voter=comment1.author_obj)
    vote3.downvote(-50)

    account_reputation_after_downvote = comment2.get_author_reputation()

    assert account_reputation_after_downvote == account_reputation_before_downvote

    vote3.assert_vote("occurred")
    vote3.assert_rc_mana_after_vote_or_downvote("decrease")
    vote3.assert_vote_or_downvote_manabar("downvote_manabar", "is_unchanged")
    vote3.assert_effective_comment_vote_operation("not_generated")
