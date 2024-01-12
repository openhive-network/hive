from __future__ import annotations

import pytest

import test_tools as tt
from hive_local_tools.functional.python.operation import Comment

UPDATED_COMMENT_OPTIONS = {
    "max_accepted_payout": tt.Asset.Tbd(100).as_legacy(),
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
    updated_comment_options = UPDATED_COMMENT_OPTIONS.copy()

    comment_0 = Comment(prepared_node, wallet)
    if reply_type == "reply_another_comment":
        comment_0.create_parent_comment()

    comment_0.send(reply_type=reply_type, **updated_comment_options)

    comment_0.assert_is_rc_mana_decreased_after_post_or_update()
    comment_0.assert_is_comment_sent_or_update()
    comment_0.assert_options_are_applied()


@pytest.mark.parametrize("reply_type", ["reply_another_comment", "no_reply"], ids=["comment", "post"])
def test_comment_and_comment_options_operations_in_the_different_transaction(
    prepared_node: tt.InitNode, wallet: tt.Wallet, reply_type: str
) -> None:
    """
    Test case 3, 4 from issue: https://gitlab.syncad.com/hive/hive/-/issues/509
    """
    updated_comment_options = UPDATED_COMMENT_OPTIONS.copy()

    comment_0 = Comment(prepared_node, wallet)
    if reply_type == "reply_another_comment":
        comment_0.create_parent_comment()

    comment_0.send(reply_type=reply_type)
    comment_0.assert_is_rc_mana_decreased_after_post_or_update()

    comment_0.options(**updated_comment_options)

    comment_0.assert_rc_mana_after_change_comment_options("decrease")
    comment_0.assert_is_comment_sent_or_update()
    comment_0.assert_options_are_applied()


@pytest.mark.parametrize("reply_type", ["reply_another_comment", "no_reply"], ids=["comment", "post"])
def test_change_comment_options_operations_twice(
    prepared_node: tt.InitNode, wallet: tt.Wallet, reply_type: str
) -> None:
    """
    Test case 5, 6 from issue: https://gitlab.syncad.com/hive/hive/-/issues/509
    """
    updated_comment_options = UPDATED_COMMENT_OPTIONS.copy()

    comment_0 = Comment(prepared_node, wallet)
    if reply_type == "reply_another_comment":
        comment_0.create_parent_comment()

    updated_comment_options["allow_votes"] = True
    updated_comment_options["allow_curation_rewards"] = True

    comment_0.send(reply_type=reply_type, **updated_comment_options)

    comment_0.assert_is_rc_mana_decreased_after_post_or_update()
    comment_0.assert_is_comment_sent_or_update()
    comment_0.assert_options_are_applied()

    updated_comment_options["max_accepted_payout"] = tt.Asset.Tbd(70).as_legacy()
    updated_comment_options["percent_hbd"] = 30
    updated_comment_options["allow_votes"] = False
    updated_comment_options["allow_curation_rewards"] = False

    comment_0.options(**updated_comment_options)

    comment_0.assert_rc_mana_after_change_comment_options("decrease")
    comment_0.assert_is_comment_sent_or_update()
    comment_0.assert_options_are_applied()


@pytest.mark.parametrize(
    ("option_name_and_value_sets_1_list", "option_name_and_value_set_2", "error_message"),
    [
        (
            [("allow_votes", True), ("allow_curation_rewards", True)],
            ("max_accepted_payout", tt.Asset.Tbd(120).as_legacy()),
            "A comment cannot accept a greater payout.",
        ),
        (
            [("allow_votes", True), ("allow_curation_rewards", True)],
            ("percent_hbd", 70),
            "A comment cannot accept a greater percent HBD.",
        ),
        (
            [("allow_curation_rewards", True)],
            ("allow_votes", True),
            "Voting cannot be re-enabled.",
        ),
        (
            [("allow_votes", True)],
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
    option_name_and_value_sets_1_list: list,
    option_name_and_value_set_2: tuple,
    error_message: str,
) -> None:
    """
    Test case 7, 8, 9, 10, 11, 12, 13, 14 from issue: https://gitlab.syncad.com/hive/hive/-/issues/509
    """
    updated_comment_options = UPDATED_COMMENT_OPTIONS.copy()

    comment_0 = Comment(prepared_node, wallet)
    if reply_type == "reply_another_comment":
        comment_0.create_parent_comment()

    for option_name_and_value_set in option_name_and_value_sets_1_list:
        updated_comment_options[option_name_and_value_set[0]] = option_name_and_value_set[1]

    comment_0.send(reply_type=reply_type, **updated_comment_options)

    comment_0.assert_is_rc_mana_decreased_after_post_or_update()
    comment_0.assert_is_comment_sent_or_update()
    comment_0.assert_options_are_applied()

    updated_comment_options[option_name_and_value_set_2[0]] = option_name_and_value_set_2[1]

    with pytest.raises(tt.exceptions.CommunicationError) as error:
        comment_0.options(**updated_comment_options)

    assert error_message in error.value.error
    comment_0.assert_rc_mana_after_change_comment_options("is_unchanged")


@pytest.mark.parametrize("reply_type", ["reply_another_comment", "no_reply"], ids=["comment", "post"])
def test_change_options_of_comment_with_reply(prepared_node: tt.InitNode, wallet: tt.Wallet, reply_type: str) -> None:
    """
    Test case 15, 16 from issue: https://gitlab.syncad.com/hive/hive/-/issues/509
    """
    updated_comment_options = UPDATED_COMMENT_OPTIONS.copy()

    comment_0 = Comment(prepared_node, wallet)
    if reply_type == "reply_another_comment":
        comment_0.create_parent_comment()

    comment_0.send(reply_type=reply_type)
    comment_0.assert_is_rc_mana_decreased_after_post_or_update()
    comment_0.create_parent_comment()

    comment_0.options(**updated_comment_options)
    comment_0.assert_rc_mana_after_change_comment_options("decrease")

    comment_0.assert_is_comment_sent_or_update()
    comment_0.assert_options_are_applied()


@pytest.mark.parametrize("reply_type", ["reply_another_comment", "no_reply"], ids=["comment", "post"])
def test_change_percent_hbd_after_vote(prepared_node: tt.InitNode, wallet: tt.Wallet, reply_type: str) -> None:
    """
    Test case 17, 18 from issue: https://gitlab.syncad.com/hive/hive/-/issues/509
    """
    comment_0 = Comment(prepared_node, wallet)
    if reply_type == "reply_another_comment":
        comment_0.create_parent_comment()

    comment_0.send(reply_type=reply_type)
    comment_0.vote()

    comment_0.options(
        percent_hbd=50,
    )
    comment_0.assert_rc_mana_after_change_comment_options("decrease")
    comment_0.assert_options_are_applied()


@pytest.mark.parametrize(
    "comment_options",
    [
        {
            "allow_votes": False,
        },
        {
            "max_accepted_payout": tt.Asset.Tbd(100).as_legacy(),
        },
        {
            "allow_curation_rewards": False,
        },
    ],
)
@pytest.mark.parametrize("reply_type", ["reply_another_comment", "no_reply"], ids=["comment", "post"])
def test_change_comment_options_after_vote(
    prepared_node: tt.InitNode, wallet: tt.Wallet, reply_type: str, comment_options: str
) -> None:
    """
    Test case 19:24 from issue: https://gitlab.syncad.com/hive/hive/-/issues/509
    """
    comment_0 = Comment(prepared_node, wallet)
    if reply_type == "reply_another_comment":
        comment_0.create_parent_comment()

    comment_0.send(reply_type=reply_type)
    comment_0.vote()

    with pytest.raises(tt.exceptions.CommunicationError) as error:
        comment_0.options(**comment_options)

    assert (
        "One of the included comment options requires the comment to have no rshares allocated to it."
        in error.value.error
    )
    comment_0.assert_rc_mana_after_change_comment_options("is_unchanged")


@pytest.mark.parametrize("reply_type", ["reply_another_comment", "no_reply"], ids=["comment", "post"])
def test_adds_the_beneficiary_after_comment(prepared_node: tt.InitNode, wallet: tt.Wallet, reply_type: str) -> None:
    """
    Test case 25, 28 from issue: https://gitlab.syncad.com/hive/hive/-/issues/509
    """
    comment_0 = Comment(prepared_node, wallet)
    if reply_type == "reply_another_comment":
        comment_0.create_parent_comment()

    comment_0.send(reply_type=reply_type)

    comment_0.create_parent_comment()

    comment_0.options(beneficiaries=[{"account": "initminer", "weight": 100}])

    comment_0.assert_rc_mana_after_change_comment_options("decrease")
    comment_0.assert_options_are_applied()


@pytest.mark.parametrize("reply_type", ["reply_another_comment", "no_reply"], ids=["comment", "post"])
@pytest.mark.parametrize(
    ("beneficiaries", "error_message"),
    [
        ([], "Must specify at least one beneficiary"),
        (
            [{"account": "alice", "weight": 20}, {"account": "initminer", "weight": 20}],
            "Comment already has beneficiaries specified.",
        ),
    ],
    ids=["try_remove_the_beneficiary_after_comment", "try_add_another_beneficiary_after_comment"],
)
def test_beneficiary_after_comment(
    prepared_node: tt.InitNode, wallet: tt.Wallet, reply_type: str, beneficiaries: list, error_message: str
) -> None:
    """
    Test case 26, 27, 29, 30 from issue: https://gitlab.syncad.com/hive/hive/-/issues/509
    """
    comment_0 = Comment(prepared_node, wallet)
    if reply_type == "reply_another_comment":
        comment_0.create_parent_comment()

    comment_0.send(reply_type=reply_type, beneficiaries=[{"account": "initminer", "weight": 100}])

    comment_0.create_parent_comment()

    with pytest.raises(tt.exceptions.CommunicationError) as error:
        comment_0.options(beneficiaries=beneficiaries)
    assert error_message in error.value.error

    comment_0.assert_rc_mana_after_change_comment_options("is_unchanged")


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
    comment_0 = Comment(prepared_node, wallet)
    if reply_type == "reply_another_comment":
        comment_0.create_parent_comment()

    comment_0.send(reply_type=reply_type)

    comment_0.vote()

    with pytest.raises(tt.exceptions.CommunicationError) as error:
        comment_0.options(beneficiaries=beneficiaries)

    assert error_message in error.value.error
    comment_0.assert_rc_mana_after_change_comment_options("is_unchanged")
