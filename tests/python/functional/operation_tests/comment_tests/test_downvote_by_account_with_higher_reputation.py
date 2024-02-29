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
    node.run(time_control="+0h x5")
    return node


@pytest.mark.parametrize("reply_type", ["reply_another_comment", "no_reply"], ids=["downvote comment", "downvote post"])
@pytest.mark.parametrize(("wait_for_payout", "comparison_sign"), [(True, "__eq__"), (False, "__lt__")])
def test_downvote_by_account_with_higher_reputation(
    prepared_node: tt.InitNode, wallet: tt.Wallet, reply_type: str, wait_for_payout: bool, comparison_sign: str
) -> None:
    """
    Test cases 41, 42, 59, 60 from issue: https://gitlab.syncad.com/hive/hive/-/issues/505
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

    if wait_for_payout:
        # waiting for cashout 60 minutes
        start_time = prepared_node.get_head_block_time() + datetime.timedelta(seconds=HIVE_CASHOUT_WINDOW_SECONDS)
        prepared_node.restart(time_control=tt.Time.serialize(start_time, format_=tt.TimeFormats.TIME_OFFSET_FORMAT))

    # Account with higher reputation downvote account with lower reputation
    vote3 = Vote(comment2, voter=comment1.author_obj)
    vote3.downvote(-50)

    vote3.assert_vote("occurred")
    vote3.assert_rc_mana_after_vote_or_downvote("decrease")

    account_reputation_after_downvote = comment2.get_author_reputation()

    account_reputation_after_downvote = comment2.get_author_reputation()
    assert_manabar_value = "is_unchanged" if wait_for_payout else "decrease"
    assert_vop_value = "not_generated" if wait_for_payout else "generated"
    assert getattr(account_reputation_after_downvote, comparison_sign)(
        account_reputation_before_downvote
    ), f"Account reputation {'decrease' if comparison_sign == '__lt__' else 'not decrease'} after downvote"
    vote3.assert_vote_or_downvote_manabar("downvote_manabar", assert_manabar_value)
    vote3.assert_effective_comment_vote_operation(assert_vop_value)
