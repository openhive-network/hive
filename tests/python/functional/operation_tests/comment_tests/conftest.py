from __future__ import annotations

from typing import TYPE_CHECKING

import pytest

from hive_local_tools.functional.python.operation.comment import Comment, CommentAccount

if TYPE_CHECKING:
    import test_tools as tt

BENEFACTORS = []


@pytest.fixture()
def posts(prepared_node, wallet, request):
    posts = [Comment(prepared_node, wallet) for _ in range(3)]

    for post in posts:
        post.send("no_reply")

    if "post_options" in request.fixturenames:
        for post in posts:
            post.options(**request.getfixturevalue("post_options"))

    if "beneficiaries_weight" in request.fixturenames:
        set_beneficiaries(prepared_node, wallet, posts, request.getfixturevalue("beneficiaries_weight"), 0)

    return posts


@pytest.fixture()
def comments(prepared_node, wallet, posts, request):
    post_1, post_2, post_3 = posts

    comment_1 = Comment(prepared_node, wallet, parent=post_1, author=post_2.author_obj)
    comment_2 = Comment(prepared_node, wallet, parent=comment_1, author=post_1.author_obj)
    comment_3 = Comment(prepared_node, wallet, parent=comment_1, author=post_3.author_obj)
    comments = [comment_1, comment_2, comment_3]

    for comment in comments:
        comment.send("reply_another_comment")

    if "comment_options" in request.fixturenames:
        for comment in comments:
            comment.options(**request.getfixturevalue("comment_options"))

    if "beneficiaries_weight" in request.fixturenames:
        set_beneficiaries(
            prepared_node, wallet, comments, request.getfixturevalue("beneficiaries_weight"), len(BENEFACTORS)
        )

    return comments


def create_beneficiary_account(name: str, node, wallet) -> CommentAccount:
    sample_vests_amount = 50
    wallet.create_account(name, vests=sample_vests_amount)
    return CommentAccount(name, node, wallet)


def set_beneficiaries(
    node: tt.InitNode,
    wallet: tt.Wallet,
    comments: list[Comment],
    beneficiaries_weight: list,
    first_benefactor_index: int,
) -> int:
    beneficiary_accounts = []
    for i in range(len(comments) * len(beneficiaries_weight)):
        beneficiary_accounts.append(create_beneficiary_account(f"ben-{first_benefactor_index + i}", node, wallet))

    BENEFACTORS.extend(beneficiary_accounts)

    beneficiary_account_number = 0
    for comment in comments:
        beneficiaries = []
        benefactors = []
        for beneficiary_weight in beneficiaries_weight:
            beneficiaries.append(
                {"account": beneficiary_accounts[beneficiary_account_number].name, "weight": beneficiary_weight}
            )
            benefactors.append(beneficiary_accounts[beneficiary_account_number])
            beneficiary_account_number += 1

        comment.options(beneficiaries=beneficiaries)
        comment.fill_beneficiares(beneficiaries, benefactors)
    return beneficiary_account_number
