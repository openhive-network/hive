from __future__ import annotations

import random

from generate_block_log import generate_random_text

import test_tools as tt
from schemas.operations.comment_operation import CommentOperation
from schemas.operations.custom_json_operation import CustomJsonOperation
from schemas.operations.transfer_operation import TransferOperation
from schemas.operations.vote_operation import VoteOperation


def draw_content_permlink(depth_level: int, comment_permlinks: list):
    return comment_permlinks[depth_level][random.randint(0, len(comment_permlinks[depth_level]) - 1)]


def get_author_from_permlink(permlink: str):
    return f"account-{permlink[permlink.rfind('-') + 1:]}"


def __prepare_operation(
    account_name: str, depth_level: int, comment_permlinks: list, permlinks_for_current_iteration: list
) -> VoteOperation | CommentOperation | CustomJsonOperation | TransferOperation:
    # range 0-24 -> transfer
    # range 25-49 -> vote
    # range 50-74 -> comment
    # range 75-100 -> custom json
    random_number = random.randint(0, 100)
    # random_number = 27
    if 0 <= random_number < 25:
        return create_transfer(account_name)
    if 25 <= random_number < 50:
        return create_vote(
            account_name,
            depth_level,
            comment_permlinks,
        )
    if 50 <= random_number < 75:
        return create_comment_operation(account_name, depth_level, comment_permlinks, permlinks_for_current_iteration)
    return create_custom_json()


def create_transfer(from_account: str) -> TransferOperation:
    return TransferOperation(
        from_=from_account,
        to=from_account,
        amount=tt.Asset.Test(0.001),
        memo=generate_random_text(0, 200),
    )


def create_vote(account_name: str, depth_level: int, comment_permlinks: list) -> VoteOperation:
    vote_weight = 0
    while vote_weight == 0:
        vote_weight = random.randint(-100, 100)

    permlink_to_vote_on = draw_content_permlink(depth_level, comment_permlinks)
    content_author = get_author_from_permlink(permlink_to_vote_on)
    return VoteOperation(voter=account_name, author=content_author, permlink=permlink_to_vote_on, weight=vote_weight)


def create_comment_operation(
    account_name: str, depth_level: int, comment_permlinks: list, permlinks_for_current_iteration: list
) -> CommentOperation:
    parent_comment_permlink = draw_content_permlink(depth_level, comment_permlinks)
    parent_author = get_author_from_permlink(parent_comment_permlink)
    current_permlink = parent_comment_permlink + "-" + account_name[account_name.rfind("-") + 1 :]
    permlinks_for_current_iteration.append(current_permlink)

    return CommentOperation(
        parent_author=parent_author,
        parent_permlink=parent_comment_permlink,
        author=account_name,
        permlink=current_permlink,
        title=generate_random_text(5, 15),
        body=generate_random_text(5, 100),
        json_metadata="{}",
    )


def create_custom_json() -> CustomJsonOperation:
    random.randint(0, 99_999)
    return CustomJsonOperation(
        # fixme:
        required_auths=["initminer"],
        required_posting_auths=["initminer"],
        id_=generate_random_text(0, 32),
        json_="{}",
    )


def create_list_of_operations(iterations: int, ops_in_one_element: int) -> list:
    """
    :param iterations: every iteration will add 100_000 records to output list
    :param ops_in_one_element: determines how many operations will be inserted in every element of output list
    :return:
    """
    output = []  # final list of lists that contains operations
    # list of lists containing permlinks of all comments, every first-dimensional list matches comments created during
    # every main iteration
    # block log contains 99999 prepared comments for first iteration
    comment_permlinks = [[f"permlink-account-{x}" for x in range(100_000)]]
    for main_iteration in range(iterations):
        permlinks_for_current_iteration = []
        for y in range(100_000):  # for y in range(100_000):
            list_of_ops = []

            for _z in range(ops_in_one_element):
                generated_op = __prepare_operation(
                    f"account-{y}", main_iteration, comment_permlinks, permlinks_for_current_iteration
                )
                list_of_ops.append(generated_op)

            output.append(list_of_ops)
        comment_permlinks.append(permlinks_for_current_iteration)
    return output
