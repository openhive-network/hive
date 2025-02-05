from __future__ import annotations

import pytest
from helpy._interfaces.wax import WaxOperationFailedError
from helpy.exceptions import ErrorInResponseError, HelpyError

import test_tools as tt
from hive_local_tools.functional.python.operation.comment import Comment

UPDATED_COMMENT_OPTIONS = {
    "max_accepted_payout": tt.Asset.Tbd(100),
    "percent_hbd": 50,
    "allow_votes": False,
    "allow_curation_rewards": False,
}


@pytest.mark.parametrize("reply_type", ["reply_another_comment", "no_reply"], ids=["comment", "post"])
def test_comment_and_comment_options_operations_in_the_same_transaction(
    prepared_node: tt.InitNode, wallet: tt.Wallet, reply_type: str
) -> None:
    """
    Test case 1, 2 from issue: https://gitlab.syncad.com/hive/hive/-/issues/509
    """
    comment = Comment(prepared_node, wallet)
    if reply_type == "reply_another_comment":
        comment.create_parent_comment()

    comment.send(reply_type=reply_type, **UPDATED_COMMENT_OPTIONS)

    comment.assert_is_rc_mana_decreased_after_post_or_update()
    comment.assert_is_comment_sent_or_update()
    comment.assert_options_are_applied()


@pytest.mark.parametrize("reply_type", ["reply_another_comment", "no_reply"], ids=["comment", "post"])
def test_comment_and_comment_options_operations_in_the_different_transaction(
    prepared_node: tt.InitNode, wallet: tt.Wallet, reply_type: str
) -> None:
    """
    Test case 3, 4 from issue: https://gitlab.syncad.com/hive/hive/-/issues/509
    """
    comment = Comment(prepared_node, wallet)
    if reply_type == "reply_another_comment":
        comment.create_parent_comment()

    comment.send(reply_type=reply_type)
    comment.assert_is_rc_mana_decreased_after_post_or_update()

    comment.options(**UPDATED_COMMENT_OPTIONS)

    comment.assert_rc_mana_after_change_comment_options("decrease")
    comment.assert_is_comment_sent_or_update()
    comment.assert_options_are_applied()


@pytest.mark.parametrize("reply_type", ["reply_another_comment", "no_reply"], ids=["comment", "post"])
def test_change_comment_options_operations_twice(
    prepared_node: tt.InitNode, wallet: tt.Wallet, reply_type: str
) -> None:
    """
    Test case 5, 6 from issue: https://gitlab.syncad.com/hive/hive/-/issues/509
    """
    updated_comment_options = UPDATED_COMMENT_OPTIONS.copy()

    comment = Comment(prepared_node, wallet)
    if reply_type == "reply_another_comment":
        comment.create_parent_comment()

    updated_comment_options["allow_votes"] = True
    updated_comment_options["allow_curation_rewards"] = True

    comment.send(reply_type=reply_type, **updated_comment_options)

    comment.assert_is_rc_mana_decreased_after_post_or_update()
    comment.assert_is_comment_sent_or_update()
    comment.assert_options_are_applied()

    updated_comment_options["max_accepted_payout"] = tt.Asset.Tbd(70)
    updated_comment_options["percent_hbd"] = 30
    updated_comment_options["allow_votes"] = False
    updated_comment_options["allow_curation_rewards"] = False

    comment.options(**updated_comment_options)

    comment.assert_rc_mana_after_change_comment_options("decrease")
    comment.assert_is_comment_sent_or_update()
    comment.assert_options_are_applied()


@pytest.mark.parametrize(
    (
        "options_to_modify_before_creating_comment",
        "options_and_values_to_modify_after_creating_comment",
        "error_message",
    ),
    [
        (
            ("allow_votes", "allow_curation_rewards"),
            ("max_accepted_payout", tt.Asset.Tbd(120)),
            "A comment cannot accept a greater payout.",
        ),
        (
            ("allow_votes", "allow_curation_rewards"),
            ("percent_hbd", 70),
            "A comment cannot accept a greater percent HBD.",
        ),
        (
            ("allow_curation_rewards",),
            ("allow_votes", True),
            "Voting cannot be re-enabled.",
        ),
        (
            ("allow_votes",),
            ("allow_curation_rewards", True),
            "Curation rewards cannot be re-enabled.",
        ),
    ],
    ids=[
        "change_max_accepted_payout_twice",
        "increase_percent_hbd_again",
        "set_allow_votes_as_true_again",
        "set_allow_curation_rewards_as_true_again",
    ],
)
@pytest.mark.parametrize("reply_type", ["reply_another_comment", "no_reply"], ids=["comment", "post"])
def test_try_change_comment_option_again(
    prepared_node: tt.InitNode,
    wallet: tt.Wallet,
    reply_type: str,
    options_to_modify_before_creating_comment: list,
    options_and_values_to_modify_after_creating_comment: tuple,
    error_message: str,
) -> None:
    """
    Test case 7, 8, 9, 10, 11, 12, 13, 14 from issue: https://gitlab.syncad.com/hive/hive/-/issues/509
    """
    updated_comment_options = UPDATED_COMMENT_OPTIONS.copy()

    comment = Comment(prepared_node, wallet)
    if reply_type == "reply_another_comment":
        comment.create_parent_comment()

    for option_name in options_to_modify_before_creating_comment:
        updated_comment_options[option_name] = True

    comment.send(reply_type=reply_type, **updated_comment_options)

    comment.assert_is_rc_mana_decreased_after_post_or_update()
    comment.assert_is_comment_sent_or_update()
    comment.assert_options_are_applied()

    updated_comment_options[options_and_values_to_modify_after_creating_comment[0]] = (
        options_and_values_to_modify_after_creating_comment[1]
    )

    with pytest.raises(ErrorInResponseError) as error:
        comment.options(**updated_comment_options)

    assert error_message in error.value.error, "Appropriate error message is not raise"
    comment.assert_rc_mana_after_change_comment_options("is_unchanged")


@pytest.mark.parametrize("reply_type", ["reply_another_comment", "no_reply"], ids=["comment", "post"])
def test_change_options_of_comment_with_reply(prepared_node: tt.InitNode, wallet: tt.Wallet, reply_type: str) -> None:
    """
    Test case 15, 16 from issue: https://gitlab.syncad.com/hive/hive/-/issues/509
    """
    comment = Comment(prepared_node, wallet)
    if reply_type == "reply_another_comment":
        comment.create_parent_comment()

    comment.send(reply_type=reply_type)
    comment.assert_is_rc_mana_decreased_after_post_or_update()
    comment.create_parent_comment()

    comment.options(**UPDATED_COMMENT_OPTIONS)
    comment.assert_rc_mana_after_change_comment_options("decrease")

    comment.assert_is_comment_sent_or_update()
    comment.assert_options_are_applied()


@pytest.mark.parametrize("reply_type", ["reply_another_comment", "no_reply"], ids=["comment", "post"])
def test_change_percent_hbd_after_vote(prepared_node: tt.InitNode, wallet: tt.Wallet, reply_type: str) -> None:
    """
    Test case 17, 18 from issue: https://gitlab.syncad.com/hive/hive/-/issues/509
    """
    comment = Comment(prepared_node, wallet)
    if reply_type == "reply_another_comment":
        comment.create_parent_comment()

    comment.send(reply_type=reply_type)
    comment.vote()

    comment.options(
        percent_hbd=50,
    )
    comment.assert_rc_mana_after_change_comment_options("decrease")
    comment.assert_options_are_applied()


@pytest.mark.parametrize(
    "comment_options",
    [
        {
            "allow_votes": False,
        },
        {
            "max_accepted_payout": tt.Asset.Tbd(100),
        },
        {
            "allow_curation_rewards": False,
        },
    ],
)
@pytest.mark.parametrize("reply_type", ["reply_another_comment", "no_reply"], ids=["comment", "post"])
def test_change_comment_options_after_vote(
    prepared_node: tt.InitNode, wallet: tt.Wallet, reply_type: str, comment_options: dict
) -> None:
    """
    Test case 19:24 from issue: https://gitlab.syncad.com/hive/hive/-/issues/509
    """
    comment = Comment(prepared_node, wallet)
    if reply_type == "reply_another_comment":
        comment.create_parent_comment()

    comment.send(reply_type=reply_type)
    comment.vote()

    with pytest.raises(ErrorInResponseError) as error:
        comment.options(**comment_options)

    assert (
        "One of the included comment options requires the comment to have no rshares allocated to it."
        in error.value.error
    ), "Appropriate error message is not raise"
    comment.assert_rc_mana_after_change_comment_options("is_unchanged")


@pytest.mark.parametrize("reply_type", ["reply_another_comment", "no_reply"], ids=["comment", "post"])
def test_adds_the_beneficiary_after_comment(prepared_node: tt.InitNode, wallet: tt.Wallet, reply_type: str) -> None:
    """
    Test case 25, 28 from issue: https://gitlab.syncad.com/hive/hive/-/issues/509
    """
    comment = Comment(prepared_node, wallet)
    if reply_type == "reply_another_comment":
        comment.create_parent_comment()

    comment.send(reply_type=reply_type)

    comment.create_parent_comment()

    comment.options(beneficiaries=[{"account": "initminer", "weight": 100}])

    comment.assert_rc_mana_after_change_comment_options("decrease")
    comment.assert_options_are_applied()


@pytest.mark.parametrize("reply_type", ["reply_another_comment", "no_reply"], ids=["comment", "post"])
@pytest.mark.parametrize(
    ("beneficiaries", "error_message", "error_type"),
    [
        ([], "Must specify at least one beneficiary", WaxOperationFailedError),
        (
            [{"account": "alice", "weight": 20}, {"account": "initminer", "weight": 20}],
            "Comment already has beneficiaries specified.",
            ErrorInResponseError,
        ),
    ],
    ids=["try_remove_the_beneficiary_after_comment", "try_add_another_beneficiary_after_comment"],
)
def test_beneficiary_after_comment(
    prepared_node: tt.InitNode,
    wallet: tt.Wallet,
    reply_type: str,
    beneficiaries: list,
    error_message: str,
    error_type: HelpyError,
) -> None:
    """
    Test case 26, 27, 29, 30 from issue: https://gitlab.syncad.com/hive/hive/-/issues/509
    """
    comment = Comment(prepared_node, wallet)
    if reply_type == "reply_another_comment":
        comment.create_parent_comment()

    comment.send(reply_type=reply_type, beneficiaries=[{"account": "initminer", "weight": 100}])

    comment.create_parent_comment()

    with pytest.raises(error_type) as error:
        comment.options(beneficiaries=beneficiaries)

    assert error_message in str(error.value), "Appropriate error message is not raise"
    comment.assert_rc_mana_after_change_comment_options("is_unchanged")


@pytest.mark.parametrize("reply_type", ["reply_another_comment", "no_reply"], ids=["comment", "post"])
@pytest.mark.parametrize(
    ("beneficiaries", "error_message"),
    [
        (
            [{"account": "initminer", "weight": 100}],
            "Comment must not have been voted on before specifying beneficiaries.",
        ),
        ([], "Must specify at least one beneficiary"),
        (
            [{"account": "alice", "weight": 20}, {"account": "initminer", "weight": 20}],
            "Comment must not have been voted on before specifying beneficiaries.",
        ),
    ],
    ids=[
        "adds_the_beneficiary_after_vote",
        "try_remove_the_beneficiary_after_votet",
        "try_add_another_beneficiary_after_vote",
    ],
)
def test_beneficiary_after_vote(
    prepared_node: tt.InitNode, wallet: tt.Wallet, reply_type: str, beneficiaries: list, error_message: str
) -> None:
    """
    Test case 31:36 from issue: https://gitlab.syncad.com/hive/hive/-/issues/509
    """
    comment = Comment(prepared_node, wallet)
    if reply_type == "reply_another_comment":
        comment.create_parent_comment()

    comment.send(reply_type=reply_type)

    comment.vote()

    with pytest.raises(HelpyError) as error:
        comment.options(beneficiaries=beneficiaries)

    if isinstance(error.value, WaxOperationFailedError):
        error_value = error.value.args[0]
    if isinstance(error.value, ErrorInResponseError):
        error_value = error.value.error

    assert error_message in error_value, "Appropriate error message is not raise"
    comment.assert_rc_mana_after_change_comment_options("is_unchanged")
