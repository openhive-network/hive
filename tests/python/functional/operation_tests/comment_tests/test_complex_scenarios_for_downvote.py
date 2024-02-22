"""Scenarios description: https://gitlab.syncad.com/hive/hive/-/issues/511"""
from __future__ import annotations

import datetime

import test_tools as tt
from hive_local_tools.constants import HIVE_CASHOUT_WINDOW_SECONDS
from hive_local_tools.functional.python.operation.comment import (
    Comment,
    CommentAccount,
    Vote,
)


def create_account(name: str, node, wallet):
    sample_vests_amount = 50
    wallet.create_account(name, vests=sample_vests_amount)
    return CommentAccount(name, node, wallet)


def test_2_1_case(prepared_node, wallet):
    [user_a, user_b, user_c] = [create_account(name, prepared_node, wallet) for name in ["user-a", "user-b", "user-c"]]

    post_1 = Comment(prepared_node, wallet, author=user_a)
    post_1.send("no_reply")

    reply_1 = Comment(prepared_node, wallet, parent=post_1, author=user_b)
    reply_1.send("reply_another_comment")

    for comment_obj, voter, vote_or_downvote, weight in [
        (post_1, user_b, "downvote", -50),
        (post_1, user_c, "downvote", -100),
        (reply_1, user_a, "downvote", -50),
        (reply_1, user_c, "downvote", -100),
    ]:
        vote = Vote(comment_obj, voter)
        getattr(vote, vote_or_downvote)(weight)

    # waiting for cashout 60 minutes
    start_time = prepared_node.get_head_block_time() + datetime.timedelta(seconds=HIVE_CASHOUT_WINDOW_SECONDS)
    prepared_node.restart(time_control=tt.StartTimeControl(start_time=start_time))

    for account in [user_a, user_b, user_c]:
        account.assert_reward_balance(prepared_node, "hbd")
        account.assert_reward_balance(prepared_node, "vesting")

    # 42. User does not receive the author reward for the post.
    post_1.assert_author_reward_virtual_operation("not_generated")  # 2.

    # 43. User does not receive the curation reward for the post.
    user_b.assert_curation_reward_virtual_operation("not_generated")  # 2.
    user_c.assert_curation_reward_virtual_operation("not_generated")  # 4.

    # 44. User does not receive the author reward for the comment.
    reply_1.assert_author_reward_virtual_operation("not_generated")  # 2.

    # 45. User does not receive the curation reward for the comment.
    user_a.assert_curation_reward_virtual_operation("not_generated")  # 2.
    # User C verified already in 43. case


def test_2_2_case(prepared_node, wallet):
    [user_a, user_b, user_c, user_d, user_e] = [
        create_account(name, prepared_node, wallet) for name in ["user-a", "user-b", "user-c", "user-d", "user-e"]
    ]

    post_1 = Comment(prepared_node, wallet, author=user_a)
    post_1.send("no_reply")

    reply_1 = Comment(prepared_node, wallet, parent=post_1, author=user_b)
    reply_1.send("reply_another_comment")

    votes = []
    for comment_obj, voter, vote_or_downvote, weight in [
        (post_1, user_b, "vote", 100),
        (post_1, user_d, "vote", 100),
        (post_1, user_c, "downvote", -100),
        (post_1, user_e, "downvote", -100),
        (reply_1, user_a, "vote", 50),
        (reply_1, user_d, "vote", 50),
        (reply_1, user_c, "downvote", -50),
        (reply_1, user_e, "downvote", -50),
    ]:
        vote = Vote(comment_obj, voter)
        getattr(vote, vote_or_downvote)(weight)
        votes.append(vote)

    # waiting for cashout 60 minutes
    start_time = prepared_node.get_head_block_time() + datetime.timedelta(seconds=HIVE_CASHOUT_WINDOW_SECONDS)
    prepared_node.restart(time_control=tt.StartTimeControl(start_time=start_time))

    for account in [user_a, user_b, user_c, user_d, user_e]:
        account.assert_reward_balance(prepared_node, "hbd")
        account.assert_reward_balance(prepared_node, "vesting")

    # 46. User receive the author reward for the post.
    post_1.assert_resource_percentage_in_reward(reward_type="author", hbd_percentage=50, vesting_percentage=50)  # 1.
    post_1.assert_author_reward_virtual_operation("generated")  # 2.

    # 47. Users who upvote the post receive the curation reward.
    votes[0].assert_curation_reward_virtual_operation("generated")  # 2.
    votes[1].assert_curation_reward_virtual_operation("generated")  # 4.

    # 48. Users who downvote the post don't receive the curation reward.
    user_c.assert_curation_reward_virtual_operation("not_generated")  # 2.
    user_e.assert_curation_reward_virtual_operation("not_generated")  # 4.

    # 49. User receive the author reward for the post.
    reply_1.assert_resource_percentage_in_reward(reward_type="author", hbd_percentage=50, vesting_percentage=50)  # 1.
    reply_1.assert_author_reward_virtual_operation("generated")  # 2.

    # 50. Users who upvote the post receive the curation reward.
    votes[4].assert_curation_reward_virtual_operation("generated")  # 2.
    votes[5].assert_curation_reward_virtual_operation("generated")  # 4.

    # 51. Users who downvote the post don't receive the curation reward.
    # Verified already in 48. case


def test_2_3_case(prepared_node, wallet):
    [user_a, user_b, user_c, user_d, user_e] = [
        create_account(name, prepared_node, wallet) for name in ["user-a", "user-b", "user-c", "user-d", "user-e"]
    ]

    post_1 = Comment(prepared_node, wallet, author=user_a)
    post_1.send("no_reply")

    reply_1 = Comment(prepared_node, wallet, parent=post_1, author=user_b)
    reply_1.send("reply_another_comment")

    for comment_obj, voter, vote_or_downvote, weight in [
        (post_1, user_b, "vote", 10),
        (post_1, user_d, "vote", 10),
        (post_1, user_c, "downvote", -100),
        (post_1, user_e, "downvote", -100),
        (reply_1, user_a, "vote", 5),
        (reply_1, user_d, "vote", 5),
        (reply_1, user_c, "downvote", -70),
        (reply_1, user_e, "downvote", -70),
    ]:
        vote = Vote(comment_obj, voter)
        getattr(vote, vote_or_downvote)(weight)

    # waiting for cashout 60 minutes
    start_time = prepared_node.get_head_block_time() + datetime.timedelta(seconds=HIVE_CASHOUT_WINDOW_SECONDS)
    prepared_node.restart(time_control=tt.StartTimeControl(start_time=start_time))

    for account in [user_a, user_b, user_c, user_d, user_e]:
        account.assert_reward_balance(prepared_node, "hbd")
        account.assert_reward_balance(prepared_node, "vesting")

    # 52. User does not receive the author reward for the post.
    post_1.assert_author_reward_virtual_operation("not_generated")  # 2.

    # 53. Users who upvote the post does not receive the curation reward.
    user_b.assert_curation_reward_virtual_operation("not_generated")  # 2.
    user_d.assert_curation_reward_virtual_operation("not_generated")  # 4.

    # 54. Users who downvote the post don't receive the curation reward.
    user_c.assert_curation_reward_virtual_operation("not_generated")  # 2.
    user_e.assert_curation_reward_virtual_operation("not_generated")  # 4.

    # 55. User does not receive the author reward for the post.
    reply_1.assert_author_reward_virtual_operation("not_generated")  # 2.

    # 56. Users who upvote the post does not receive the curation reward.
    user_a.assert_curation_reward_virtual_operation("not_generated")  # 2.
    # User D verified already in 53. case

    # 57. Users who downvote the post does not receive the curation reward.
    # Verified already in 54. case
