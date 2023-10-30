from __future__ import annotations

import datetime

import pytest

import test_tools as tt
from hive_local_tools.constants import HIVE_CASHOUT_WINDOW_SECONDS
from hive_local_tools.functional.python.operation import Comment, Vote
from test_tools.__private.exceptions import CommunicationError


@pytest.mark.parametrize("reply_type", ["reply_another_comment", "no_reply"], ids=["vote comment", "vote post"])
@pytest.mark.parametrize("voter", ["same_as_comment", "random"])
@pytest.mark.parametrize(("vote_type", "vote_weight"), [("vote", 100), ("downvote", -20)])
def test_vote_on_not_payout_comment(
    prepared_node: tt.InitNode, wallet: tt.Wallet, voter, reply_type, vote_type, vote_weight
) -> None:
    """
    Test cases 1:8 from issue: https://gitlab.syncad.com/hive/hive/-/issues/505
    """
    comment = Comment(prepared_node, wallet)
    if reply_type == "reply_another_comment":
        comment.create_parent_comment()

    comment.send(reply_type=reply_type)

    vote = Vote(comment, voter=voter)
    getattr(vote, vote_type)(vote_weight)

    vote.assert_vote("occurred")
    vote.assert_rc_mana_after_vote_or_downvote("decrease")
    vote.assert_vote_or_downvote_manabar(f"{vote_type}_manabar", "decrease")
    vote.assert_effective_comment_vote_operation("generated")


@pytest.mark.parametrize(
    ("first_vote_type", "first_vote_weight", "second_vote_type", "second_vote_weight"),
    [
        ("vote", 50, "vote", 100),
        ("vote", 100, "vote", 50),
        ("vote", 100, "vote", 0),
        ("downvote", -70, "downvote", -20),
        ("downvote", -70, "downvote", -100),
        ("downvote", -70, "downvote", 0),
        ("vote", 100, "downvote", -50),
        ("downvote", -50, "vote", 100),
    ],
    ids=[
        "second vote has a higher weight",
        "second vote has a lower weight",
        "second vote weight = 0",
        "second downvote has a higher weight",
        "second downvote has a lower weight",
        "second downvote weight = 0",
        "vote and downvote",
        "downvote and vote",
    ],
)
@pytest.mark.parametrize("voter", ["same_as_comment", "random"])
@pytest.mark.parametrize("reply_type", ["reply_another_comment", "no_reply"], ids=["vote comment", "vote post"])
def test_double_vote_on_not_payout_comment(
    prepared_node: tt.InitNode,
    wallet: tt.Wallet,
    voter,
    first_vote_type,
    first_vote_weight,
    second_vote_type,
    second_vote_weight,
    reply_type,
) -> None:
    """
    Test cases 9:40 from issue: https://gitlab.syncad.com/hive/hive/-/issues/505
    """
    comment = Comment(prepared_node, wallet)
    if reply_type == "reply_another_comment":
        comment.create_parent_comment()

    comment.send(reply_type=reply_type)

    vote = Vote(comment, voter=voter)
    getattr(vote, first_vote_type)(first_vote_weight)

    vote.assert_vote("occurred")
    vote.assert_rc_mana_after_vote_or_downvote("decrease")
    vote.assert_vote_or_downvote_manabar(f"{first_vote_type}_manabar", "decrease")
    vote.assert_effective_comment_vote_operation("generated")

    getattr(vote, second_vote_type)(second_vote_weight)

    vote.assert_vote("occurred")
    vote.assert_rc_mana_after_vote_or_downvote("decrease")
    # comment delete don't reduce vote/downvote manabar
    if second_vote_weight != 0:
        vote.assert_vote_or_downvote_manabar(f"{second_vote_type}_manabar", "decrease")
    vote.assert_effective_comment_vote_operation("generated")


@pytest.mark.parametrize("reply_type", ["reply_another_comment", "no_reply"], ids=["vote comment", "vote post"])
def test_vote_on_not_payout_comment_with_not_allowed_votes(
    prepared_node: tt.InitNode, wallet: tt.Wallet, reply_type
) -> None:
    """
    Test cases 43, 45 from issue: https://gitlab.syncad.com/hive/hive/-/issues/505
    """
    comment = Comment(prepared_node, wallet)

    if reply_type == "reply_another_comment":
        comment.create_parent_comment()

    comment.send(reply_type=reply_type)
    comment.options(allow_votes=False)

    vote = Vote(comment, voter="random")
    with pytest.raises(CommunicationError) as error:
        vote.vote(100)

    assert "Votes are not allowed on the comment." in error.value.error

    vote.assert_vote("not_occurred")
    vote.assert_rc_mana_after_vote_or_downvote("is_unchanged")
    vote.assert_vote_or_downvote_manabar("vote_manabar", "is_unchanged")
    vote.assert_effective_comment_vote_operation("not_generated")


@pytest.mark.parametrize("reply_type", ["reply_another_comment", "no_reply"], ids=["downvote comment", "downvote post"])
def test_downvote_on_not_payout_comment_with_not_allowed_votes(
    prepared_node: tt.InitNode, wallet: tt.Wallet, reply_type
) -> None:
    """
    Test cases 44, 46 from issue: https://gitlab.syncad.com/hive/hive/-/issues/505
    """
    comment = Comment(prepared_node, wallet)

    if reply_type == "reply_another_comment":
        comment.create_parent_comment()

    comment.send(reply_type=reply_type)
    comment.options(allow_votes=False)

    vote = Vote(comment, voter="random")
    vote.downvote(-50)

    vote.assert_vote("occurred")
    vote.assert_rc_mana_after_vote_or_downvote("decrease")
    vote.assert_vote_or_downvote_manabar("downvote_manabar", "decrease")
    vote.assert_effective_comment_vote_operation("generated")


@pytest.mark.parametrize("reply_type", ["reply_another_comment", "no_reply"], ids=["vote comment", "vote post"])
@pytest.mark.parametrize(("vote_type", "vote_weight"), [("vote", 100), ("downvote", -50)])
def test_vote_on_payout_comment_with_not_allowed_votes(
    prepared_node: tt.InitNode, wallet: tt.Wallet, reply_type, vote_type, vote_weight
) -> None:
    """
    Test cases 47:50 from issue: https://gitlab.syncad.com/hive/hive/-/issues/505
    """
    comment = Comment(prepared_node, wallet)

    if reply_type == "reply_another_comment":
        comment.create_parent_comment()

    comment.send(reply_type=reply_type)
    comment.options(allow_votes=False)

    # waiting for cashout 60 minutes
    time_offset = prepared_node.get_head_block_time() + datetime.timedelta(seconds=HIVE_CASHOUT_WINDOW_SECONDS)
    prepared_node.restart(time_offset=tt.Time.serialize(time_offset, format_=tt.TimeFormats.TIME_OFFSET_FORMAT))

    comment.assert_is_comment_sent_or_update()

    vote = Vote(comment, voter="random")
    getattr(vote, vote_type)(vote_weight)

    vote.assert_vote("occurred")
    vote.assert_rc_mana_after_vote_or_downvote("decrease")
    vote.assert_vote_or_downvote_manabar(f"{vote_type}_manabar", "is_unchanged")
    vote.assert_effective_comment_vote_operation("not_generated")


@pytest.mark.parametrize("reply_type", ["reply_another_comment", "no_reply"], ids=["vote comment", "vote post"])
@pytest.mark.parametrize("voter", ["same_as_comment", "random"])
@pytest.mark.parametrize(("vote_type", "vote_weight"), [("vote", 100), ("downvote", -20)])
def test_vote_on_payout_comment(
    prepared_node: tt.InitNode, wallet: tt.Wallet, voter, reply_type, vote_type, vote_weight
) -> None:
    """
    Test cases 51:58 from issue: https://gitlab.syncad.com/hive/hive/-/issues/505
    """
    comment = Comment(prepared_node, wallet)
    if reply_type == "reply_another_comment":
        comment.create_parent_comment()

    comment.send(reply_type=reply_type)

    # waiting for cashout 60 minutes
    time_offset = prepared_node.get_head_block_time() + datetime.timedelta(seconds=HIVE_CASHOUT_WINDOW_SECONDS)
    prepared_node.restart(time_offset=tt.Time.serialize(time_offset, format_=tt.TimeFormats.TIME_OFFSET_FORMAT))

    vote = Vote(comment, voter=voter)
    getattr(vote, vote_type)(vote_weight)

    vote.assert_vote("occurred")
    vote.assert_rc_mana_after_vote_or_downvote("decrease")
    vote.assert_vote_or_downvote_manabar(f"{vote_type}_manabar", "is_unchanged")
    vote.assert_effective_comment_vote_operation("not_generated")
