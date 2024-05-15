"""Scenarios description: https://gitlab.syncad.com/hive/hive/-/issues/511"""
from __future__ import annotations

import datetime

import pytest

import test_tools as tt
from hive_local_tools.constants import HIVE_CASHOUT_WINDOW_SECONDS
from hive_local_tools.functional.python.operation.comment import Comment, Vote


def votes(posts, comments):
    post_1, post_2, post_3 = posts
    comment_1, comment_2, comment_3 = comments

    vote_parameters_set = [
        (post_1, post_1, 100),
        (post_2, post_1, 100),
        (comment_1, post_1, 100),
        (comment_2, post_1, 100),
        (post_1, post_2, 50),
        (comment_2, post_2, 50),
        (post_2, post_3, 100),
    ]

    votes = []

    for comment_obj, voter_extraction_obj, vote_weight in vote_parameters_set:
        vote = Vote(comment_obj, voter_extraction_obj.author_obj)
        vote.vote(vote_weight)
        votes.append(vote)

    return tuple(votes)


def test_1_1_case(prepared_node, posts, comments):
    post_1, post_2, post_3 = posts
    comment_1, comment_2, comment_3 = comments
    vote_1, vote_2, vote_3, vote_4, vote_5, vote_6, vote_7 = votes(posts, comments)

    # waiting for cashout 60 minutes
    start_time = prepared_node.get_head_block_time() + datetime.timedelta(seconds=HIVE_CASHOUT_WINDOW_SECONDS)
    prepared_node.restart(time_control=tt.StartTimeControl(start_time=start_time))

    for post in posts:
        post.verify_balances()

    # 1. User receives the author reward for the post.
    for post in [post_1, post_2]:
        post.assert_resource_percentage_in_reward(
            reward_type="author", hbd_percentage=50, vesting_percentage=50
        )  # 1. 3.
        post.assert_author_reward_virtual_operation("generated")  # 2. 4.

    for reward_type in ["hbd_payout", "vesting_payout"]:
        assert post_1.get_reward(reward_type="author", mode=reward_type) < post_2.get_reward(
            reward_type="author", mode=reward_type
        ), f"Incorrect value of {reward_type} in author_reward"  # 5.

    # 2. User receives the author reward for the comment.
    for comment in [comment_1, comment_2]:
        comment.assert_resource_percentage_in_reward(
            reward_type="author", hbd_percentage=50, vesting_percentage=50
        )  # 1. 3.
        comment.assert_author_reward_virtual_operation("generated")  # 2. 4.

    for reward_type in ["hbd_payout", "vesting_payout"]:
        assert comment_1.get_reward(reward_type="author", mode=reward_type) < comment_2.get_reward(
            reward_type="author", mode=reward_type
        ), f"Incorrect value of {reward_type} in author_reward"  # 5.

    # 3. User does not receive the author reward for the post if the post has no votes.
    post_3.assert_author_reward_virtual_operation("not_generated")  # 2.

    # 4. User does not receive the author reward for the comment if the comment has no votes.
    comment_3.assert_author_reward_virtual_operation("not_generated")  # 2.

    # 5. User receives the curation reward for own post.
    vote_1.assert_curation_reward_virtual_operation("generated")  # 2.

    # 6. User receives the curation reward for someone else post.
    for vote in [vote_5, vote_2, vote_7]:  # 2. 4. 6.
        vote.assert_curation_reward_virtual_operation("generated")

    assert (
        vote_7.get_curation_vesting_reward() > vote_5.get_curation_vesting_reward()
    ), "Incorrect value of vesting_payout in curation reward"  # 7.

    # 7. User receives the curation reward for own comment.
    vote_4.assert_curation_reward_virtual_operation("generated")  # 2.

    # 8. User receives the curation reward for someone else comment.
    for vote in [vote_3, vote_6]:  # 2. 4.
        vote.assert_curation_reward_virtual_operation("generated")

    assert (
        vote_3.get_curation_vesting_reward() > vote_6.get_curation_vesting_reward()
    ), "Incorrect value of vesting_payout in curation reward"  # 7.


@pytest.mark.parametrize("post_options", [{"percent_hbd": 50}])
@pytest.mark.parametrize("comment_options", [{"percent_hbd": 50}])
def test_1_2_case(prepared_node, posts, comments, post_options, comment_options):
    post_1, post_2, post_3 = posts
    comment_1, comment_2, comment_3 = comments
    votes(posts, comments)

    # waiting for cashout 60 minutes
    start_time = prepared_node.get_head_block_time() + datetime.timedelta(seconds=HIVE_CASHOUT_WINDOW_SECONDS)
    prepared_node.restart(time_control=tt.StartTimeControl(start_time=start_time))

    for post in posts:
        post.verify_balances()

    # 9. User receives the author reward for the post.
    for post in [post_1, post_2]:
        post.assert_resource_percentage_in_reward(
            reward_type="author", hbd_percentage=25, vesting_percentage=75
        )  # 1. 3.
        post.assert_author_reward_virtual_operation("generated")  # 2. 4.

    for reward_type in ["hbd_payout", "vesting_payout"]:
        assert post_1.get_reward(reward_type="author", mode=reward_type) < post_2.get_reward(
            reward_type="author", mode=reward_type
        ), f"Incorrect value of {reward_type} in author_reward"  # 5.

    # 10. User receives the author reward for the comment.
    for comment in [comment_1, comment_2]:
        comment.assert_resource_percentage_in_reward(
            reward_type="author", hbd_percentage=25, vesting_percentage=75
        )  # 1. 3.
        comment.assert_author_reward_virtual_operation("generated")  # 2. 4.

    for reward_type in ["hbd_payout", "vesting_payout"]:
        assert comment_1.get_reward(reward_type="author", mode=reward_type) < comment_2.get_reward(
            reward_type="author", mode=reward_type
        ), f"Incorrect value of {reward_type} in author_reward"  # 5.

    # 11. User does not receive the author reward for the post if the post has no votes.
    post_3.assert_author_reward_virtual_operation("not_generated")  # 2.

    # 12. User does not receive the author reward for the comment if the comment has no votes.
    comment_3.assert_author_reward_virtual_operation("not_generated")  # 2.


@pytest.mark.parametrize("post_options", [{"percent_hbd": 0}])
@pytest.mark.parametrize("comment_options", [{"percent_hbd": 0}])
def test_1_3_case(prepared_node, posts, comments, post_options, comment_options):
    post_1, post_2, post_3 = posts
    comment_1, comment_2, comment_3 = comments
    votes(posts, comments)

    # waiting for cashout 60 minutes
    start_time = prepared_node.get_head_block_time() + datetime.timedelta(seconds=HIVE_CASHOUT_WINDOW_SECONDS)
    prepared_node.restart(time_control=tt.StartTimeControl(start_time=start_time))

    for post in posts:
        post.verify_balances()

    # 13. User receives the author reward for the post.
    for post in [post_1, post_2]:
        post.assert_resource_percentage_in_reward(
            reward_type="author", hbd_percentage=0, vesting_percentage=100
        )  # 1. 3.
        post.assert_author_reward_virtual_operation("generated")  # 2. 4.

    assert post_1.get_reward(reward_type="author", mode="vesting_payout") < post_2.get_reward(
        reward_type="author", mode="vesting_payout"
    ), "Incorrect value of vesting_payout in author_reward"  # 5.

    # 14. User receives the author reward for the comment.
    for comment in [comment_1, comment_2]:
        comment.assert_resource_percentage_in_reward(
            reward_type="author", hbd_percentage=0, vesting_percentage=100
        )  # 1. 3.
        comment.assert_author_reward_virtual_operation("generated")  # 2. 4.

    assert comment_1.get_reward(reward_type="author", mode="vesting_payout") < comment_2.get_reward(
        reward_type="author", mode="vesting_payout"
    ), "Incorrect value of vesting_payout in author_reward"  # 5.

    # 15. User does not receive the author reward for the post if the post has no votes.
    post_3.assert_author_reward_virtual_operation("not_generated")  # 2.

    # 16. User does not receive the author reward for the comment if the comment has no votes.
    comment_3.assert_author_reward_virtual_operation("not_generated")  # 2.


def test_1_4_case(prepared_node, wallet):
    post_1_unbounded_hbd_reward = tt.Asset.from_legacy("4.136 TBD")
    post_2_unbounded_hbd_reward = tt.Asset.from_legacy("6.185 TBD")
    comment_1_unbounded_hbd_reward = tt.Asset.from_legacy("6.460 TBD")
    comment_2_unbounded_hbd_reward = tt.Asset.from_legacy("8.527 TBD")

    post_1_unbounded_vesting_reward = tt.Asset.from_legacy("11627.974929 VESTS")
    post_2_unbounded_vesting_reward = tt.Asset.from_legacy("15350.366904 VESTS")
    comment_1_unbounded_vesting_reward = tt.Asset.from_legacy("7446.583944 VESTS")
    comment_2_unbounded_vesting_reward = tt.Asset.from_legacy("11134.775993 VESTS")

    vote_1_unbounded_vesting_reward = tt.Asset.from_legacy("15503.366574 VESTS")
    vote_2_unbounded_vesting_reward = tt.Asset.from_legacy("15193.767241 VESTS")
    vote_3_unbounded_vesting_reward = tt.Asset.from_legacy("14889.567897 VESTS")
    vote_4_unbounded_vesting_reward = tt.Asset.from_legacy("14590.768541 VESTS")
    vote_5_unbounded_vesting_reward = tt.Asset.from_legacy("7750.783289 VESTS")
    vote_6_unbounded_vesting_reward = tt.Asset.from_legacy("7673.383455 VESTS")
    vote_7_unbounded_vesting_reward = tt.Asset.from_legacy("15503.366574 VESTS")

    post_1 = Comment(prepared_node, wallet)
    post_2 = Comment(prepared_node, wallet)
    post_3 = Comment(prepared_node, wallet)

    post_1.send("no_reply", max_accepted_payout=(post_1_unbounded_hbd_reward / 2))
    post_2.send("no_reply", max_accepted_payout=(post_2_unbounded_hbd_reward / 2))
    post_3.send("no_reply")

    comment_1 = Comment(prepared_node, wallet, parent=post_1, author=post_2.author_obj)
    comment_2 = Comment(prepared_node, wallet, parent=comment_1, author=post_1.author_obj)
    comment_3 = Comment(prepared_node, wallet, parent=comment_1, author=post_3.author_obj)

    comment_1.send("reply_another_comment", max_accepted_payout=(comment_1_unbounded_hbd_reward / 2))
    comment_2.send("reply_another_comment", max_accepted_payout=(comment_2_unbounded_hbd_reward / 2))
    comment_3.send("reply_another_comment")

    vote_1, vote_2, vote_3, vote_4, vote_5, vote_6, vote_7 = votes(
        [post_1, post_2, post_3], [comment_1, comment_2, comment_3]
    )

    # waiting for cashout 60 minutes
    start_time = prepared_node.get_head_block_time() + datetime.timedelta(seconds=HIVE_CASHOUT_WINDOW_SECONDS)
    prepared_node.restart(time_control=tt.StartTimeControl(start_time=start_time))

    for post in [post_1, post_2, post_3]:
        post.verify_balances()

    # 17. User receives the author reward for the post.
    for post in [post_1, post_2]:
        post.assert_resource_percentage_in_reward(
            reward_type="author", hbd_percentage=50, vesting_percentage=50
        )  # 1. 3.
        post.assert_author_reward_virtual_operation("generated")  # 2. 4.

    for reward_type in ["hbd_payout", "vesting_payout"]:
        assert post_1.get_reward(reward_type="author", mode=reward_type) < post_2.get_reward(
            reward_type="author", mode=reward_type
        ), f"Incorrect value of {reward_type} in author_reward"  # 5.

    assert (
        post_1.get_reward(reward_type="author", mode="hbd_payout") < post_1_unbounded_hbd_reward
    ), "Incorrect value of hbd_payout in author_reward"  # 6.
    assert (
        post_1.get_reward(reward_type="author", mode="vesting_payout") < post_1_unbounded_vesting_reward
    ), "Incorrect value of vesting_payout in author_reward"  # 6.
    assert (
        post_2.get_reward(reward_type="author", mode="hbd_payout") < post_2_unbounded_hbd_reward
    ), "Incorrect value of hbd_payout in author_reward"  # 7.
    assert (
        post_2.get_reward(reward_type="author", mode="vesting_payout") < post_2_unbounded_vesting_reward
    ), "Incorrect value of vesting_payout in author_reward"  # 7.

    # 18. User receives the author reward for the comment.
    for comment in [comment_1, comment_2]:
        comment.assert_resource_percentage_in_reward(
            reward_type="author", hbd_percentage=50, vesting_percentage=50
        )  # 1. 3.
        comment.assert_author_reward_virtual_operation("generated")  # 2. 4.

    for reward_type in ["hbd_payout", "vesting_payout"]:
        assert comment_1.get_reward(reward_type="author", mode=reward_type) < comment_2.get_reward(
            reward_type="author", mode=reward_type
        ), f"Incorrect value of {reward_type} in author_reward"  # 5.

    assert (
        comment_1.get_reward(reward_type="author", mode="hbd_payout") < comment_1_unbounded_hbd_reward
    ), "Incorrect value of hbd_payout in author_reward"  # 6.
    assert (
        comment_1.get_reward(reward_type="author", mode="vesting_payout") < comment_1_unbounded_vesting_reward
    ), "Incorrect value of vesting_payout in author_reward"  # 6.
    assert (
        comment_2.get_reward(reward_type="author", mode="hbd_payout") < comment_2_unbounded_hbd_reward
    ), "Incorrect value of hbd_payout in author_reward"  # 7.
    assert (
        comment_2.get_reward(reward_type="author", mode="vesting_payout") < comment_2_unbounded_vesting_reward
    ), "Incorrect value of vesting_payout in author_reward"  # 7.

    # 19. User receives the curation reward for own post.
    vote_1.assert_curation_reward_virtual_operation("generated")  # 2.
    assert (
        vote_1.get_curation_vesting_reward() < vote_1_unbounded_vesting_reward
    ), "Incorrect value of vesting_payout in curation_reward"  # 3.

    # 20. User receives the curation reward for someone else post.
    for vote in [vote_5, vote_2, vote_7]:  # 2. 4. 6.
        vote.assert_curation_reward_virtual_operation("generated")

    assert (
        vote_7.get_curation_vesting_reward() > vote_5.get_curation_vesting_reward()
    ), "Incorrect value of vesting_payout in curation_reward"  # 7.
    assert (
        vote_5.get_curation_vesting_reward() < vote_5_unbounded_vesting_reward
    ), "Incorrect value of vesting_payout in curation_reward"  # 9.
    assert (
        vote_2.get_curation_vesting_reward() < vote_2_unbounded_vesting_reward
    ), "Incorrect value of vesting_payout in curation_reward"  # 8.
    assert (
        vote_7.get_curation_vesting_reward() < vote_7_unbounded_vesting_reward
    ), "Incorrect value of vesting_payout in curation_reward"  # 10.

    # 7. User receives the curation reward for own comment.
    vote_4.assert_curation_reward_virtual_operation("generated")  # 2.
    assert (
        vote_4.get_curation_vesting_reward() < vote_4_unbounded_vesting_reward
    ), "Incorrect value of vesting_payout in curation_reward"  # 3.

    # 8. User receives the curation reward for someone else comment.
    for vote in [vote_3, vote_6]:  # 2. 4.
        vote.assert_curation_reward_virtual_operation("generated")

    assert (
        vote_3.get_curation_vesting_reward() > vote_6.get_curation_vesting_reward()
    ), "Incorrect value of vesting_payout in curation_reward"  # 7.
    assert (
        vote_3.get_curation_vesting_reward() < vote_3_unbounded_vesting_reward
    ), "Incorrect value of vesting_payout in curation_reward"  # 8.
    assert (
        vote_6.get_curation_vesting_reward() < vote_6_unbounded_vesting_reward
    ), "Incorrect value of vesting_payout in curation_reward"  # 9.


@pytest.mark.parametrize("post_options", [{"max_accepted_payout": tt.Asset.Tbd(0)}])
@pytest.mark.parametrize("comment_options", [{"max_accepted_payout": tt.Asset.Tbd(0)}])
def test_1_5_case(prepared_node, posts, comments, post_options, comment_options):
    post_1, post_2, post_3 = posts
    comment_1, comment_2, comment_3 = comments
    vote_1, vote_2, vote_3, vote_4, vote_5, vote_6, vote_7 = votes(posts, comments)

    # waiting for cashout 60 minutes
    start_time = prepared_node.get_head_block_time() + datetime.timedelta(seconds=HIVE_CASHOUT_WINDOW_SECONDS)
    prepared_node.restart(time_control=tt.StartTimeControl(start_time=start_time))

    for post in posts:
        post.verify_balances()

    # 23. User receives the author reward for the post.
    for post in [post_1, post_2]:
        post.assert_author_reward_virtual_operation("not_generated")  # 2. 4.

    # 24. User receives the author reward for the comment.
    for comment in [comment_1, comment_2]:
        comment.assert_author_reward_virtual_operation("not_generated")  # 2. 4.

    for vote_object in [
        vote_1,  # 25. User receives the curation reward for own post - check #2
        vote_5,  # 26. User receives the curation reward for someone else post. - check #2
        vote_2,  # 26. User receives the curation reward for someone else post. - check #4
        vote_7,  # 26. User receives the curation reward for someone else post. - check #6
        vote_4,  # 27. User receives the curation reward for own comment. - check #2
        vote_3,  # 28. User receives the curation reward for someone else comment. - check #2
        vote_6,  # 28. User receives the curation reward for someone else comment. - check #4
    ]:
        vote_object.assert_curation_reward_virtual_operation("not_generated")


@pytest.mark.parametrize("post_options", [{"allow_curation_rewards": False}])
@pytest.mark.parametrize("comment_options", [{"allow_curation_rewards": False}])
def test_1_6_case(prepared_node, posts, comments, post_options, comment_options):
    post_1, post_2, post_3 = posts
    comment_1, comment_2, comment_3 = comments
    vote_1, vote_2, vote_3, vote_4, vote_5, vote_6, vote_7 = votes(posts, comments)

    # waiting for cashout 60 minutes
    start_time = prepared_node.get_head_block_time() + datetime.timedelta(seconds=HIVE_CASHOUT_WINDOW_SECONDS)
    prepared_node.restart(time_control=tt.StartTimeControl(start_time=start_time))

    for post in posts:
        post.verify_balances()

    # 29. User receives the author reward for the post.
    for post in [post_1, post_2]:
        post.assert_resource_percentage_in_reward(
            reward_type="author", hbd_percentage=50, vesting_percentage=50
        )  # 1. 3.
        post.assert_author_reward_virtual_operation("generated")  # 2. 4.

    for mode in ("hbd_payout", "vesting_payout"):
        assert post_1.get_reward(reward_type="author", mode=mode) < post_2.get_reward(
            reward_type="author", mode=mode
        ), f"Incorrect value of {mode} in author_reward"  # 5.

    # 30. User receives the author reward for the comment.
    for comment in [comment_1, comment_2]:
        comment.assert_resource_percentage_in_reward(
            reward_type="author", hbd_percentage=50, vesting_percentage=50
        )  # 1. 3.
        comment.assert_author_reward_virtual_operation("generated")  # 2. 4.

    for mode in ("hbd_payout", "vesting_payout"):
        assert comment_1.get_reward(reward_type="author", mode=mode) < comment_2.get_reward(
            reward_type="author", mode=mode
        ), f"Incorrect value of {mode} in author_reward"  # 5.

    # 31. User receives the curation reward for own post.
    vote_1.assert_curation_reward_virtual_operation("not_generated")  # 2.

    # 32. User receives the curation reward for someone else post.
    for vote in [vote_5, vote_2, vote_7]:  # 2. 4. 6.
        vote.assert_curation_reward_virtual_operation("not_generated")

    # 33. User receives the curation reward for own comment.
    vote_4.assert_curation_reward_virtual_operation("not_generated")  # 2.

    # 34. User receives the curation reward for someone else comment.
    for vote in [vote_3, vote_6]:  # 2. 4.
        vote.assert_curation_reward_virtual_operation("not_generated")


@pytest.mark.parametrize("beneficiaries_weight", [[100]])
def test_1_7_case(prepared_node, posts, comments, beneficiaries_weight):
    post_1, post_2, post_3 = posts
    comment_1, comment_2, comment_3 = comments
    votes(posts, comments)

    # waiting for cashout 60 minutes
    start_time = prepared_node.get_head_block_time() + datetime.timedelta(seconds=HIVE_CASHOUT_WINDOW_SECONDS)
    prepared_node.restart(time_control=tt.StartTimeControl(start_time=start_time))

    for post in [post_1, post_2, post_3, comment_1, comment_2, comment_3]:
        post.verify_balances()

    # 35. Beneficiary receives the author reward for the post.
    for post in [post_1, post_2]:
        assert post.get_reward(reward_type="author", mode="hbd_payout") == tt.Asset.Tbd(
            0
        ), "Incorrect value of hbd_payout in author_reward"  # 1. 5.
        assert post.get_reward(reward_type="author", mode="vesting_payout") == tt.Asset.Vest(
            0
        ), "Incorrect value of vesting_payout in author_reward"  # 1. 5.
        post.assert_resource_percentage_in_reward(
            reward_type="comment_benefactor", hbd_percentage=50, vesting_percentage=50
        )  # 2. 6.
        post.assert_author_reward_virtual_operation("generated")  # 3. 7.
        post.assert_comment_benefactors_reward_virtual_operations("generated")  # 4. 8.

    # 36. Beneficiary receives the author reward for the comment.
    for comment in [comment_1, comment_2]:
        assert comment.get_reward(reward_type="author", mode="hbd_payout") == tt.Asset.Tbd(
            0
        ), "Incorrect value of hbd_payout in author_reward"  # 1.
        assert comment.get_reward(reward_type="author", mode="vesting_payout") == tt.Asset.Vest(
            0
        ), "Incorrect value of vesting_payout in author_reward"  # 1.
        comment.assert_resource_percentage_in_reward(
            reward_type="comment_benefactor", hbd_percentage=50, vesting_percentage=50
        )  # 2. 6.
        comment.assert_author_reward_virtual_operation("generated")  # 3. 7.
        comment.assert_comment_benefactors_reward_virtual_operations("generated")  # 4. 8.

    # 37. Beneficiaries do not receive the curation reward.
    for post in [post_1, post_2, post_3, comment_1, comment_2, comment_3]:
        post.benefactors[0].assert_curation_reward_virtual_operation("not_generated")  # 1.


@pytest.mark.parametrize("beneficiaries_weight", [[30, 70]])
def test_1_8_case(prepared_node, posts, comments, beneficiaries_weight):
    post_1, post_2, post_3 = posts
    comment_1, comment_2, comment_3 = comments
    votes(posts, comments)

    # waiting for cashout 60 minutes
    start_time = prepared_node.get_head_block_time() + datetime.timedelta(seconds=HIVE_CASHOUT_WINDOW_SECONDS)
    prepared_node.restart(time_control=tt.StartTimeControl(start_time=start_time))

    for post in [post_1, post_2, post_3, comment_1, comment_2, comment_3]:
        post.verify_balances()

    # 38. Beneficiary receives the author reward for the post.
    for post in [post_1, post_2]:
        assert post.get_reward(reward_type="author", mode="hbd_payout") == tt.Asset.Tbd(
            0
        ), "Incorrect value of hbd_payout in author_reward"  # 1. 7.
        #  The total allocation of resources is not possible to more than 1 beneficiary. Therefore, the author may
        #  receive trace amounts of vests even if he transfers all the resources to the beneficiaries.
        assert (
            tt.Asset.Vest(2) > post.get_reward(reward_type="author", mode="vesting_payout") >= tt.Asset.Vest(0)
        ), "Value of vesting_payout in author_reward out of range"  # 1. 7.
        post.assert_resource_percentage_in_reward(
            reward_type="comment_benefactor", hbd_percentage=50, vesting_percentage=50
        )  # 2. 8.
        post.assert_resource_percentage_in_reward(
            reward_type="comment_benefactor", hbd_percentage=50, vesting_percentage=50
        )  # 3. 9.
        post.assert_author_reward_virtual_operation("generated")  # 4. 10.
        post.assert_comment_benefactors_reward_virtual_operations("generated")  # 4. 5. 11. 12.

    # 39. Beneficiary receives the author reward for the comment.
    for comment in [comment_1, comment_2]:
        assert comment.get_reward(reward_type="author", mode="hbd_payout") == tt.Asset.Tbd(
            0
        ), "Incorrect value of hbd_payout in author_reward"  # 1. 7.
        assert (
            tt.Asset.Vest(2) > comment.get_reward(reward_type="author", mode="vesting_payout") >= tt.Asset.Vest(0)
        ), "Value of vesting_payout in author_reward out of range"  # 1. 7.
        comment.assert_resource_percentage_in_reward(
            reward_type="comment_benefactor", hbd_percentage=50, vesting_percentage=50
        )  # 2. 8.
        comment.assert_resource_percentage_in_reward(
            reward_type="comment_benefactor", hbd_percentage=50, vesting_percentage=50
        )  # 3. 9.
        comment.assert_author_reward_virtual_operation("generated")  # 4. 10.
        comment.assert_comment_benefactors_reward_virtual_operations("generated")  # 4. 5. 11. 12.


@pytest.mark.parametrize("beneficiaries_weight", [[20, 30]])
def test_1_9_case(prepared_node, posts, comments, beneficiaries_weight):
    post_1, post_2, post_3 = posts
    comment_1, comment_2, comment_3 = comments
    votes(posts, comments)

    # waiting for cashout 60 minutes
    start_time = prepared_node.get_head_block_time() + datetime.timedelta(seconds=HIVE_CASHOUT_WINDOW_SECONDS)
    prepared_node.restart(time_control=tt.StartTimeControl(start_time=start_time))

    for post in [post_1, post_2, post_3, comment_1, comment_2, comment_3]:
        post.verify_balances()

    # 38. Beneficiary receives the author reward for the post.
    for post in [post_1, post_2]:
        post.assert_resource_percentage_in_reward(
            reward_type="author", hbd_percentage=50, vesting_percentage=50
        )  # 1. 7.
        post.assert_resource_percentage_in_reward(
            reward_type="comment_benefactor", hbd_percentage=50, vesting_percentage=50
        )  # 2. 8.
        post.assert_resource_percentage_in_reward(
            reward_type="comment_benefactor", hbd_percentage=50, vesting_percentage=50
        )  # 3. 9.
        post.assert_author_reward_virtual_operation("generated")  # 4. 10.
        post.assert_comment_benefactors_reward_virtual_operations("generated")  # 4. 5. 11. 12.

    # 39. Beneficiary receives the author reward for the comment.
    for comment in [comment_1, comment_2]:
        comment.assert_resource_percentage_in_reward(
            reward_type="author", hbd_percentage=50, vesting_percentage=50
        )  # 1. 7.
        comment.assert_resource_percentage_in_reward(
            reward_type="comment_benefactor", hbd_percentage=50, vesting_percentage=50
        )  # 2. 8.
        comment.assert_resource_percentage_in_reward(
            reward_type="comment_benefactor", hbd_percentage=50, vesting_percentage=50
        )  # 3. 9.
        comment.assert_author_reward_virtual_operation("generated")  # 4. 10.
        comment.assert_comment_benefactors_reward_virtual_operations("generated")  # 4. 5. 11. 12.
