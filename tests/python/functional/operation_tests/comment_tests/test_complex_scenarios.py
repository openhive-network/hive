from __future__ import annotations

import datetime

import test_tools as tt
from hive_local_tools.constants import HIVE_CASHOUT_WINDOW_SECONDS
from hive_local_tools.functional.python.operation import (
    Comment,
    CommentAccount,
    Vote,
)


def posts(node, wallet, **comment_options):
    post_1 = Comment(node, wallet)
    post_2 = Comment(node, wallet)
    post_3 = Comment(node, wallet)
    posts = [post_1, post_2, post_3]

    for post in posts:
        post.send("no_reply")

    if comment_options is not None:
        for post in posts:
            post.options(**comment_options)

    return posts


def comments(node, wallet, posts, **comment_options):
    post_1, post_2, post_3 = posts

    comment_1 = Comment(node, wallet, parent=post_1, author=post_2.author_obj)
    comment_2 = Comment(node, wallet, parent=comment_1, author=post_1.author_obj)
    comment_3 = Comment(node, wallet, parent=comment_1, author=post_3.author_obj)
    comments = [comment_1, comment_2, comment_3]

    for comment in comments:
        comment.send("reply_another_comment")

    if comment_options is not None:
        for comment in comments:
            comment.options(**comment_options)

    return comments


def posts_and_comments_with_beneficiaries(node, wallet, beneficiaries_weight: list[int]):
    beneficiary_accounts = []
    for i in range(6 * len(beneficiaries_weight)):
        beneficiary_accounts.append(create_beneficiary_account(f"ben-{i}", node, wallet))

    post_1 = Comment(node, wallet)
    post_2 = Comment(node, wallet)
    post_3 = Comment(node, wallet)

    posts = [post_1, post_2, post_3]

    beneficiary_account_number = 0
    for post in posts:
        beneficiaries = []
        benefactors = []

        for beneficiary_weight in beneficiaries_weight:
            beneficiaries.append(
                {"account": beneficiary_accounts[beneficiary_account_number].name, "weight": beneficiary_weight}
            )
            benefactors.append(beneficiary_accounts[beneficiary_account_number])
            beneficiary_account_number += 1

        post.send("no_reply", beneficiaries=beneficiaries)
        post.append_beneficiares(beneficiaries, benefactors)

    comment_1 = Comment(node, wallet, parent=post_1, author=post_2.author_obj)
    comment_2 = Comment(node, wallet, parent=comment_1, author=post_1.author_obj)
    comment_3 = Comment(node, wallet, parent=comment_1, author=post_3.author_obj)
    comments = [comment_1, comment_2, comment_3]

    for comment in comments:
        beneficiaries = []
        benefactors = []
        for beneficiary_weight in beneficiaries_weight:
            beneficiaries.append(
                {"account": beneficiary_accounts[beneficiary_account_number].name, "weight": beneficiary_weight}
            )
            benefactors.append(beneficiary_accounts[beneficiary_account_number])
            beneficiary_account_number += 1

        comment.send("reply_another_comment", beneficiaries=beneficiaries)
        comment.append_beneficiares(beneficiaries, benefactors)

    return *posts, *comments


def votes(posts, comments):
    post_1, post_2, post_3 = posts
    comment_1, comment_2, comment_3 = comments

    vote_1 = Vote(post_1, post_1.author_obj)
    vote_1.vote(100)

    vote_2 = Vote(post_2, post_1.author_obj)
    vote_2.vote(100)

    vote_3 = Vote(comment_1, post_1.author_obj)
    vote_3.vote(100)

    vote_4 = Vote(comment_2, post_1.author_obj)
    vote_4.vote(100)

    vote_5 = Vote(post_1, post_2.author_obj)
    vote_5.vote(50)

    vote_6 = Vote(comment_2, post_2.author_obj)
    vote_6.vote(50)

    vote_7 = Vote(post_2, post_3.author_obj)
    vote_7.vote(100)

    return vote_1, vote_2, vote_3, vote_4, vote_5, vote_6, vote_7


def create_beneficiary_account(name: str, node, wallet):
    sample_vests_amount = 50
    wallet.create_account(name, vests=sample_vests_amount)
    return CommentAccount(name, node, wallet)


def test_1_1_case(prepared_node, wallet):
    post_1, post_2, post_3 = posts(prepared_node, wallet)
    comment_1, comment_2, comment_3 = comments(prepared_node, wallet, [post_1, post_2, post_3])
    vote_1, vote_2, vote_3, vote_4, vote_5, vote_6, vote_7 = votes(
        [post_1, post_2, post_3], [comment_1, comment_2, comment_3]
    )

    # waiting for cashout 60 minutes
    time_offset = prepared_node.get_head_block_time() + datetime.timedelta(seconds=HIVE_CASHOUT_WINDOW_SECONDS)
    prepared_node.restart(time_offset=tt.Time.serialize(time_offset, format_=tt.TimeFormats.TIME_OFFSET_FORMAT))

    # Verify balances
    for post in [post_1, post_2, post_3]:
        post.author_obj.assert_reward_hbd_balance(prepared_node)
        post.author_obj.assert_reward_vesting_balance(prepared_node)

    # 1. User receives the author reward for the post.
    for post in [post_1, post_2]:
        post.assert_resource_percentage_in_author_reward(hbd_percentage=50, vesting_percentage=50)  # 1. 3.
        post.assert_author_reward_virtual_operation("generated")  # 2. 4.

    for reward_type in ["hbd_payout", "vesting_payout"]:
        assert post_1.get_author_reward(reward_type) < post_2.get_author_reward(reward_type)  # 5.

    # 2. User receives the author reward for the comment.
    for comment in [comment_1, comment_2]:
        comment.assert_resource_percentage_in_author_reward(hbd_percentage=50, vesting_percentage=50)  # 1. 3.
        comment.assert_author_reward_virtual_operation("generated")  # 2. 4.

    for reward_type in ["hbd_payout", "vesting_payout"]:
        assert comment_1.get_author_reward(reward_type) < comment_2.get_author_reward(reward_type)  # 5.

    # 3. User does not receive the author reward for the post if the post has no votes.
    post_3.assert_author_reward_virtual_operation("not_generated")  # 2.

    # 4. User does not receive the author reward for the comment if the comment has no votes.
    comment_3.assert_author_reward_virtual_operation("not_generated")  # 2.

    # 5. User receives the curation reward for own post.
    vote_1.assert_curation_reward_virtual_operation("generated")  # 2.

    # 6. User receives the curation reward for someone else post.
    for vote in [vote_5, vote_2, vote_7]:  # 2. 4. 6.
        vote.assert_curation_reward_virtual_operation("generated")

    assert vote_7.get_curation_vesting_reward() > vote_5.get_curation_vesting_reward()  # 7.

    # 7. User receives the curation reward for own comment.
    vote_4.assert_curation_reward_virtual_operation("generated")  # 2.

    # 8. User receives the curation reward for someone else comment.
    for vote in [vote_3, vote_6]:  # 2. 4.
        vote.assert_curation_reward_virtual_operation("generated")

    assert vote_3.get_curation_vesting_reward() > vote_6.get_curation_vesting_reward()  # 7.


def test_1_2_case(prepared_node, wallet):
    post_1, post_2, post_3 = posts(prepared_node, wallet, percent_hbd=50)
    comment_1, comment_2, comment_3 = comments(prepared_node, wallet, [post_1, post_2, post_3], percent_hbd=50)
    vote_1, vote_2, vote_3, vote_4, vote_5, vote_6, vote_7 = votes(
        [post_1, post_2, post_3], [comment_1, comment_2, comment_3]
    )

    # waiting for cashout 60 minutes
    time_offset = prepared_node.get_head_block_time() + datetime.timedelta(seconds=HIVE_CASHOUT_WINDOW_SECONDS)
    prepared_node.restart(time_offset=tt.Time.serialize(time_offset, format_=tt.TimeFormats.TIME_OFFSET_FORMAT))

    # Verify balances
    for post in [post_1, post_2, post_3]:
        post.author_obj.assert_reward_hbd_balance(prepared_node)
        post.author_obj.assert_reward_vesting_balance(prepared_node)

    # 9. User receives the author reward for the post.
    for post in [post_1, post_2]:
        post.assert_resource_percentage_in_author_reward(hbd_percentage=25, vesting_percentage=75)  # 1. 3.
        post.assert_author_reward_virtual_operation("generated")  # 2. 4.

    for reward_type in ["hbd_payout", "vesting_payout"]:
        assert post_1.get_author_reward(reward_type) < post_2.get_author_reward(reward_type)  # 5.

    # 10. User receives the author reward for the comment.
    for comment in [comment_1, comment_2]:
        comment.assert_resource_percentage_in_author_reward(hbd_percentage=25, vesting_percentage=75)  # 1. 3.
        comment.assert_author_reward_virtual_operation("generated")  # 2. 4.

    for reward_type in ["hbd_payout", "vesting_payout"]:
        assert comment_1.get_author_reward(reward_type) < comment_2.get_author_reward(reward_type)  # 5.

    # 11. User does not receive the author reward for the post if the post has no votes.
    post_3.assert_author_reward_virtual_operation("not_generated")  # 2.

    # 12. User does not receive the author reward for the comment if the comment has no votes.
    comment_3.assert_author_reward_virtual_operation("not_generated")  # 2.


def test_1_3_case(prepared_node, wallet):
    post_1, post_2, post_3 = posts(prepared_node, wallet, percent_hbd=0)
    comment_1, comment_2, comment_3 = comments(prepared_node, wallet, [post_1, post_2, post_3], percent_hbd=0)
    vote_1, vote_2, vote_3, vote_4, vote_5, vote_6, vote_7 = votes(
        [post_1, post_2, post_3], [comment_1, comment_2, comment_3]
    )

    # waiting for cashout 60 minutes
    time_offset = prepared_node.get_head_block_time() + datetime.timedelta(seconds=HIVE_CASHOUT_WINDOW_SECONDS)
    prepared_node.restart(time_offset=tt.Time.serialize(time_offset, format_=tt.TimeFormats.TIME_OFFSET_FORMAT))

    # Verify balances
    for post in [post_1, post_2, post_3]:
        post.author_obj.assert_reward_hbd_balance(prepared_node)
        post.author_obj.assert_reward_vesting_balance(prepared_node)

    # 13. User receives the author reward for the post.
    for post in [post_1, post_2]:
        post.assert_resource_percentage_in_author_reward(hbd_percentage=00, vesting_percentage=100)  # 1. 3.
        post.assert_author_reward_virtual_operation("generated")  # 2. 4.

    assert post_1.get_author_reward("vesting_payout") < post_2.get_author_reward("vesting_payout")  # 5.

    # 14. User receives the author reward for the comment.
    for comment in [comment_1, comment_2]:
        comment.assert_resource_percentage_in_author_reward(hbd_percentage=0, vesting_percentage=100)  # 1. 3.
        comment.assert_author_reward_virtual_operation("generated")  # 2. 4.

    assert comment_1.get_author_reward("vesting_payout") < comment_2.get_author_reward("vesting_payout")  # 5.

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

    post_1.send("no_reply", max_accepted_payout=(post_1_unbounded_hbd_reward / 2).as_legacy())
    post_2.send("no_reply", max_accepted_payout=(post_2_unbounded_hbd_reward / 2).as_legacy())
    post_3.send("no_reply")

    comment_1 = Comment(prepared_node, wallet, parent=post_1, author=post_2.author_obj)
    comment_2 = Comment(prepared_node, wallet, parent=comment_1, author=post_1.author_obj)
    comment_3 = Comment(prepared_node, wallet, parent=comment_1, author=post_3.author_obj)

    comment_1.send("reply_another_comment", max_accepted_payout=(comment_1_unbounded_hbd_reward / 2).as_legacy())
    comment_2.send("reply_another_comment", max_accepted_payout=(comment_2_unbounded_hbd_reward / 2).as_legacy())
    comment_3.send("reply_another_comment")

    vote_1, vote_2, vote_3, vote_4, vote_5, vote_6, vote_7 = votes(
        [post_1, post_2, post_3], [comment_1, comment_2, comment_3]
    )

    # waiting for cashout 60 minutes
    time_offset = prepared_node.get_head_block_time() + datetime.timedelta(seconds=HIVE_CASHOUT_WINDOW_SECONDS)
    prepared_node.restart(time_offset=tt.Time.serialize(time_offset, format_=tt.TimeFormats.TIME_OFFSET_FORMAT))

    # Verify balances
    for post in [post_1, post_2, post_3]:
        post.author_obj.assert_reward_hbd_balance(prepared_node)
        post.author_obj.assert_reward_vesting_balance(prepared_node)

    # 17. User receives the author reward for the post.
    for post in [post_1, post_2]:
        post.assert_resource_percentage_in_author_reward(hbd_percentage=50, vesting_percentage=50)  # 1. 3.
        post.assert_author_reward_virtual_operation("generated")  # 2. 4.

    for reward_type in ["hbd_payout", "vesting_payout"]:
        assert post_1.get_author_reward(reward_type) < post_2.get_author_reward(reward_type)  # 5.

    assert post_1.get_author_reward("hbd_payout") < post_1_unbounded_hbd_reward  # 6.
    assert post_1.get_author_reward("vesting_payout") < post_1_unbounded_vesting_reward  # 6.
    assert post_2.get_author_reward("hbd_payout") < post_2_unbounded_hbd_reward  # 7.
    assert post_2.get_author_reward("vesting_payout") < post_2_unbounded_vesting_reward  # 7.

    # 18. User receives the author reward for the comment.
    for comment in [comment_1, comment_2]:
        comment.assert_resource_percentage_in_author_reward(hbd_percentage=50, vesting_percentage=50)  # 1. 3.
        comment.assert_author_reward_virtual_operation("generated")  # 2. 4.

    for reward_type in ["hbd_payout", "vesting_payout"]:
        assert comment_1.get_author_reward(reward_type) < comment_2.get_author_reward(reward_type)  # 5.

    assert comment_1.get_author_reward("hbd_payout") < comment_1_unbounded_hbd_reward  # 6.
    assert comment_1.get_author_reward("vesting_payout") < comment_1_unbounded_vesting_reward  # 6.
    assert comment_2.get_author_reward("hbd_payout") < comment_2_unbounded_hbd_reward  # 7.
    assert comment_2.get_author_reward("vesting_payout") < comment_2_unbounded_vesting_reward  # 7.

    # 19. User receives the curation reward for own post.
    vote_1.assert_curation_reward_virtual_operation("generated")  # 2.
    assert vote_1.get_curation_vesting_reward() < vote_1_unbounded_vesting_reward  # 3.

    # 20. User receives the curation reward for someone else post.
    for vote in [vote_5, vote_2, vote_7]:  # 2. 4. 6.
        vote.assert_curation_reward_virtual_operation("generated")

    assert vote_7.get_curation_vesting_reward() > vote_5.get_curation_vesting_reward()  # 7.
    assert vote_5.get_curation_vesting_reward() < vote_5_unbounded_vesting_reward  # 9.
    assert vote_2.get_curation_vesting_reward() < vote_2_unbounded_vesting_reward  # 8.
    assert vote_7.get_curation_vesting_reward() < vote_7_unbounded_vesting_reward  # 10.

    # 7. User receives the curation reward for own comment.
    vote_4.assert_curation_reward_virtual_operation("generated")  # 2.
    assert vote_4.get_curation_vesting_reward() < vote_4_unbounded_vesting_reward  # 3.

    # 8. User receives the curation reward for someone else comment.
    for vote in [vote_3, vote_6]:  # 2. 4.
        vote.assert_curation_reward_virtual_operation("generated")

    assert vote_3.get_curation_vesting_reward() > vote_6.get_curation_vesting_reward()  # 7.
    assert vote_3.get_curation_vesting_reward() < vote_3_unbounded_vesting_reward  # 8.
    assert vote_6.get_curation_vesting_reward() < vote_6_unbounded_vesting_reward  # 9.


def test_1_5_case(prepared_node, wallet):
    post_1, post_2, post_3 = posts(prepared_node, wallet, max_accepted_payout=tt.Asset.Tbd(0).as_legacy())
    comment_1, comment_2, comment_3 = comments(
        prepared_node, wallet, [post_1, post_2, post_3], max_accepted_payout=tt.Asset.Tbd(0).as_legacy()
    )
    vote_1, vote_2, vote_3, vote_4, vote_5, vote_6, vote_7 = votes(
        [post_1, post_2, post_3], [comment_1, comment_2, comment_3]
    )

    # waiting for cashout 60 minutes
    time_offset = prepared_node.get_head_block_time() + datetime.timedelta(seconds=HIVE_CASHOUT_WINDOW_SECONDS)
    prepared_node.restart(time_offset=tt.Time.serialize(time_offset, format_=tt.TimeFormats.TIME_OFFSET_FORMAT))

    # Verify balances
    for post in [post_1, post_2, post_3]:
        post.author_obj.assert_reward_hbd_balance(prepared_node)
        post.author_obj.assert_reward_vesting_balance(prepared_node)

    # 23. User receives the author reward for the post.
    for post in [post_1, post_2]:
        post.assert_author_reward_virtual_operation("not_generated")  # 2. 4.

    # 24. User receives the author reward for the comment.
    for comment in [comment_1, comment_2]:
        comment.assert_author_reward_virtual_operation("not_generated")  # 2. 4.

    # 25. User receives the curation reward for own post.
    vote_1.assert_curation_reward_virtual_operation("not_generated")  # 2.

    # 26. User receives the curation reward for someone else post.
    for vote in [vote_5, vote_2, vote_7]:  # 2. 4. 6.
        vote.assert_curation_reward_virtual_operation("not_generated")

    # 27. User receives the curation reward for own comment.
    vote_4.assert_curation_reward_virtual_operation("not_generated")  # 2.

    # 28. User receives the curation reward for someone else comment.
    for vote in [vote_3, vote_6]:  # 2. 4.
        vote.assert_curation_reward_virtual_operation("not_generated")


def test_1_6_case(prepared_node, wallet):
    post_1, post_2, post_3 = posts(prepared_node, wallet, allow_curation_rewards=False)
    comment_1, comment_2, comment_3 = comments(
        prepared_node, wallet, [post_1, post_2, post_3], allow_curation_rewards=False
    )
    vote_1, vote_2, vote_3, vote_4, vote_5, vote_6, vote_7 = votes(
        [post_1, post_2, post_3], [comment_1, comment_2, comment_3]
    )

    # waiting for cashout 60 minutes
    time_offset = prepared_node.get_head_block_time() + datetime.timedelta(seconds=HIVE_CASHOUT_WINDOW_SECONDS)
    prepared_node.restart(time_offset=tt.Time.serialize(time_offset, format_=tt.TimeFormats.TIME_OFFSET_FORMAT))

    # Verify balances
    for post in [post_1, post_2, post_3]:
        post.author_obj.assert_reward_hbd_balance(prepared_node)
        post.author_obj.assert_reward_vesting_balance(prepared_node)

    # 29. User receives the author reward for the post.
    for post in [post_1, post_2]:
        post.assert_resource_percentage_in_author_reward(hbd_percentage=50, vesting_percentage=50)  # 1. 3.
        post.assert_author_reward_virtual_operation("generated")  # 2. 4.

    assert post_1.get_author_reward("hbd_payout") < post_2.get_author_reward("hbd_payout")  # 5.
    assert post_1.get_author_reward("vesting_payout") < post_2.get_author_reward("vesting_payout")  # 5.

    # 30. User receives the author reward for the comment.
    for comment in [comment_1, comment_2]:
        comment.assert_resource_percentage_in_author_reward(hbd_percentage=50, vesting_percentage=50)  # 1. 3.
        comment.assert_author_reward_virtual_operation("generated")  # 2. 4.

    assert comment_1.get_author_reward("hbd_payout") < comment_2.get_author_reward("hbd_payout")  # 5.
    assert comment_1.get_author_reward("vesting_payout") < comment_2.get_author_reward("vesting_payout")  # 5.

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


def test_1_7_case(prepared_node, wallet):
    post_1, post_2, post_3, comment_1, comment_2, comment_3 = posts_and_comments_with_beneficiaries(
        prepared_node, wallet, [100]
    )

    vote_1, vote_2, vote_3, vote_4, vote_5, vote_6, vote_7 = votes(
        [post_1, post_2, post_3], [comment_1, comment_2, comment_3]
    )

    # waiting for cashout 60 minutes
    time_offset = prepared_node.get_head_block_time() + datetime.timedelta(seconds=HIVE_CASHOUT_WINDOW_SECONDS)
    prepared_node.restart(time_offset=tt.Time.serialize(time_offset, format_=tt.TimeFormats.TIME_OFFSET_FORMAT))

    # Verify balances
    for post in [post_1, post_2, post_3, comment_1, comment_2, comment_3]:
        post.author_obj.assert_reward_hbd_balance(prepared_node)
        post.author_obj.assert_reward_vesting_balance(prepared_node)
        post.benefactors[0].assert_reward_hbd_balance(prepared_node)
        post.benefactors[0].assert_reward_vesting_balance(prepared_node)

    # 35. Beneficiary receives the author reward for the post.
    for post in [post_1, post_2]:
        assert post.get_author_reward("hbd_payout") == tt.Asset.Tbd(0)  # 1. 5.
        assert post.get_author_reward("vesting_payout") == tt.Asset.Vest(0)  # 1. 5.
        post.assert_resource_percentage_in_comment_benefactor_reward(hbd_percentage=50, vesting_percentage=50)  # 2. 6.
        post.assert_author_reward_virtual_operation("generated")  # 3. 7.
        post.assert_comment_benefactors_reward_virtual_operations("generated")  # 4. 8.

    # 36. Beneficiary receives the author reward for the comment.
    for comment in [comment_1, comment_2]:
        assert comment.get_author_reward("hbd_payout") == tt.Asset.Tbd(0)  # 1.
        assert comment.get_author_reward("vesting_payout") == tt.Asset.Vest(0)  # 1.
        comment.assert_resource_percentage_in_comment_benefactor_reward(
            hbd_percentage=50, vesting_percentage=50
        )  # 2. 6.
        comment.assert_author_reward_virtual_operation("generated")  # 3. 7.
        comment.assert_comment_benefactors_reward_virtual_operations("generated")  # 4. 8.

    # 37. Beneficiaries do not receive the curation reward.
    for post in [post_1, post_2, post_3, comment_1, comment_2, comment_3]:
        post.benefactors[0].assert_curation_reward_virtual_operation("not_generated")  # 1.


def test_1_8_case(prepared_node, wallet):
    post_1, post_2, post_3, comment_1, comment_2, comment_3 = posts_and_comments_with_beneficiaries(
        prepared_node, wallet, [30, 70]
    )

    vote_1, vote_2, vote_3, vote_4, vote_5, vote_6, vote_7 = votes(
        [post_1, post_2, post_3], [comment_1, comment_2, comment_3]
    )

    # waiting for cashout 60 minutes
    time_offset = prepared_node.get_head_block_time() + datetime.timedelta(seconds=HIVE_CASHOUT_WINDOW_SECONDS)
    prepared_node.restart(time_offset=tt.Time.serialize(time_offset, format_=tt.TimeFormats.TIME_OFFSET_FORMAT))

    # Verify balances
    for post in [post_1, post_2, post_3, comment_1, comment_2, comment_3]:
        post.author_obj.assert_reward_hbd_balance(prepared_node)
        post.author_obj.assert_reward_vesting_balance(prepared_node)
        post.benefactors[0].assert_reward_hbd_balance(prepared_node)
        post.benefactors[0].assert_reward_vesting_balance(prepared_node)
        post.benefactors[1].assert_reward_hbd_balance(prepared_node)
        post.benefactors[1].assert_reward_vesting_balance(prepared_node)

    # 38. Beneficiary receives the author reward for the post.
    for post in [post_1, post_2]:
        assert post.get_author_reward("hbd_payout") == tt.Asset.Tbd(0)  # 1. 7.
        #  The total allocation of resources is not possible to more than 1 beneficiary. Therefore, the author may
        #  receive trace amounts of vests even if he transfers all the resources to the beneficiaries.
        assert tt.Asset.Vest(2) > post.get_author_reward("vesting_payout") >= tt.Asset.Vest(0)  # 1. 7.
        post.assert_resource_percentage_in_comment_benefactor_reward(hbd_percentage=50, vesting_percentage=50)  # 2. 8.
        post.assert_resource_percentage_in_comment_benefactor_reward(hbd_percentage=50, vesting_percentage=50)  # 3. 9.
        post.assert_author_reward_virtual_operation("generated")  # 4. 10.
        post.assert_comment_benefactors_reward_virtual_operations("generated")  # 4. 5. 11. 12.

    # 39. Beneficiary receives the author reward for the comment.
    for comment in [comment_1, comment_2]:
        assert comment.get_author_reward("hbd_payout") == tt.Asset.Tbd(0)  # 1. 7.
        assert tt.Asset.Vest(2) > comment.get_author_reward("vesting_payout") >= tt.Asset.Vest(0)  # 1. 7.
        comment.assert_resource_percentage_in_comment_benefactor_reward(
            hbd_percentage=50, vesting_percentage=50
        )  # 2. 8.
        comment.assert_resource_percentage_in_comment_benefactor_reward(
            hbd_percentage=50, vesting_percentage=50
        )  # 3. 9.
        comment.assert_author_reward_virtual_operation("generated")  # 4. 10.
        comment.assert_comment_benefactors_reward_virtual_operations("generated")  # 4. 5. 11. 12.


def test_1_9_case(prepared_node, wallet):
    post_1, post_2, post_3, comment_1, comment_2, comment_3 = posts_and_comments_with_beneficiaries(
        prepared_node, wallet, [20, 30]
    )

    vote_1, vote_2, vote_3, vote_4, vote_5, vote_6, vote_7 = votes(
        [post_1, post_2, post_3], [comment_1, comment_2, comment_3]
    )

    # waiting for cashout 60 minutes
    time_offset = prepared_node.get_head_block_time() + datetime.timedelta(seconds=HIVE_CASHOUT_WINDOW_SECONDS)
    prepared_node.restart(time_offset=tt.Time.serialize(time_offset, format_=tt.TimeFormats.TIME_OFFSET_FORMAT))

    # Verify balances
    for post in [post_1, post_2, post_3, comment_1, comment_2, comment_3]:
        post.author_obj.assert_reward_hbd_balance(prepared_node)
        post.author_obj.assert_reward_vesting_balance(prepared_node)
        post.benefactors[0].assert_reward_hbd_balance(prepared_node)
        post.benefactors[0].assert_reward_vesting_balance(prepared_node)
        post.benefactors[1].assert_reward_hbd_balance(prepared_node)
        post.benefactors[1].assert_reward_vesting_balance(prepared_node)

    # 38. Beneficiary receives the author reward for the post.
    for post in [post_1, post_2]:
        post.assert_resource_percentage_in_author_reward(hbd_percentage=50, vesting_percentage=50)  # 1. 7.
        post.assert_resource_percentage_in_comment_benefactor_reward(hbd_percentage=50, vesting_percentage=50)  # 2. 8.
        post.assert_resource_percentage_in_comment_benefactor_reward(hbd_percentage=50, vesting_percentage=50)  # 3. 9.
        post.assert_author_reward_virtual_operation("generated")  # 4. 10.
        post.assert_comment_benefactors_reward_virtual_operations("generated")  # 4. 5. 11. 12.

    # 39. Beneficiary receives the author reward for the comment.
    for comment in [comment_1, comment_2]:
        comment.assert_resource_percentage_in_author_reward(hbd_percentage=50, vesting_percentage=50)  # 1. 7.
        comment.assert_resource_percentage_in_comment_benefactor_reward(
            hbd_percentage=50, vesting_percentage=50
        )  # 2. 8.
        comment.assert_resource_percentage_in_comment_benefactor_reward(
            hbd_percentage=50, vesting_percentage=50
        )  # 3. 9.
        comment.assert_author_reward_virtual_operation("generated")  # 4. 10.
        comment.assert_comment_benefactors_reward_virtual_operations("generated")  # 4. 5. 11. 12.
