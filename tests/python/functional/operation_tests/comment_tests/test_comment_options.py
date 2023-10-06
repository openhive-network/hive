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
def test_comment_and_comment_options_operations_in_the_same_transaction(prepared_node, wallet, reply_type):
    """
    Test case 1, 2 from issue: https://gitlab.syncad.com/hive/hive/-/issues/503
    """
    comment_0 = Comment(prepared_node, wallet)
    if reply_type == "reply_another_comment":
        comment_0.create_parent_comment()

    comment_0.send(reply_type=reply_type, **UPDATED_COMMENT_OPTIONS)

    comment_0.assert_is_rc_mana_decreased_after_post_or_update()
    comment_0.assert_is_comment_sent_or_update()
    comment_0.assert_options_are_apply()


@pytest.mark.parametrize("reply_type", ["reply_another_comment", "no_reply"], ids=["comment", "post"])
def test_comment_and_comment_options_operations_in_the_different_transaction(prepared_node, wallet, reply_type):
    """
    Test case 3, 4 from issue: https://gitlab.syncad.com/hive/hive/-/issues/503
    """
    comment_0 = Comment(prepared_node, wallet)
    if reply_type == "reply_another_comment":
        comment_0.create_parent_comment()

    comment_0.send(reply_type=reply_type)
    comment_0.assert_is_rc_mana_decreased_after_post_or_update()

    comment_0.options(**UPDATED_COMMENT_OPTIONS)

    comment_0.assert_rc_mana_after_change_comment_options("decrease")
    comment_0.assert_is_comment_sent_or_update()
    comment_0.assert_options_are_apply()


@pytest.mark.parametrize("reply_type", ["reply_another_comment", "no_reply"], ids=["comment", "post"])
def test_change_comment_options_operations_twice(prepared_node, wallet, reply_type):
    """
    Test case 5, 6 from issue: https://gitlab.syncad.com/hive/hive/-/issues/503
    """
    comment_0 = Comment(prepared_node, wallet)
    if reply_type == "reply_another_comment":
        comment_0.create_parent_comment()

    comment_0.send(
        reply_type=reply_type,
        max_accepted_payout=tt.Asset.Tbd(100).as_legacy(),
        percent_hbd=50,
        allow_votes=True,
        allow_curation_rewards=True,
    )

    comment_0.assert_is_rc_mana_decreased_after_post_or_update()
    comment_0.assert_is_comment_sent_or_update()
    comment_0.assert_options_are_apply()

    comment_0.options(
        max_accepted_payout=tt.Asset.Tbd(70).as_legacy(),
        percent_hbd=30,
        allow_votes=False,
        allow_curation_rewards=False,
    )
    comment_0.assert_rc_mana_after_change_comment_options("decrease")
    comment_0.assert_is_comment_sent_or_update()
    comment_0.assert_options_are_apply()


@pytest.mark.parametrize("reply_type", ["reply_another_comment", "no_reply"], ids=["comment", "post"])
def test_change_max_accepted_payout_twice(prepared_node, wallet, reply_type):
    """
    Test case 7, 8 from issue: https://gitlab.syncad.com/hive/hive/-/issues/503
    """
    comment_0 = Comment(prepared_node, wallet)
    if reply_type == "reply_another_comment":
        comment_0.create_parent_comment()

    comment_0.send(
        reply_type=reply_type,
        max_accepted_payout=tt.Asset.Tbd(100).as_legacy(),
        percent_hbd=50,
        allow_votes=True,
        allow_curation_rewards=True,
    )

    comment_0.assert_is_rc_mana_decreased_after_post_or_update()
    comment_0.assert_is_comment_sent_or_update()
    comment_0.assert_options_are_apply()
    with pytest.raises(tt.exceptions.CommunicationError) as error:
        comment_0.options(
            max_accepted_payout=tt.Asset.Tbd(120).as_legacy(),
            percent_hbd=50,
            allow_votes=True,
            allow_curation_rewards=True,
        )

    assert "A comment cannot accept a greater payout." in error.value.error
    comment_0.assert_rc_mana_after_change_comment_options("is_unchanged")


@pytest.mark.parametrize("reply_type", ["reply_another_comment", "no_reply"], ids=["comment", "post"])
def test_increase_percent_hbd_again(prepared_node, wallet, reply_type):
    """
    Test case 9, 10 from issue: https://gitlab.syncad.com/hive/hive/-/issues/503
    """
    comment_0 = Comment(prepared_node, wallet)
    if reply_type == "reply_another_comment":
        comment_0.create_parent_comment()

    comment_0.send(
        reply_type=reply_type,
        max_accepted_payout=tt.Asset.Tbd(100).as_legacy(),
        percent_hbd=50,
        allow_votes=True,
        allow_curation_rewards=True,
    )

    comment_0.assert_is_rc_mana_decreased_after_post_or_update()
    comment_0.assert_is_comment_sent_or_update()
    comment_0.assert_options_are_apply()
    with pytest.raises(tt.exceptions.CommunicationError) as error:
        comment_0.options(
            max_accepted_payout=tt.Asset.Tbd(100).as_legacy(),
            percent_hbd=70,
            allow_votes=True,
            allow_curation_rewards=True,
        )

    assert "A comment cannot accept a greater percent HBD." in error.value.error
    comment_0.assert_rc_mana_after_change_comment_options("is_unchanged")


@pytest.mark.parametrize("reply_type", ["reply_another_comment", "no_reply"], ids=["comment", "post"])
def test_set_allow_votes_as_true_again(prepared_node, wallet, reply_type):
    """
    Test case 11, 12 from issue: https://gitlab.syncad.com/hive/hive/-/issues/503
    """
    comment_0 = Comment(prepared_node, wallet)
    if reply_type == "reply_another_comment":
        comment_0.create_parent_comment()

    comment_0.send(
        reply_type=reply_type,
        max_accepted_payout=tt.Asset.Tbd(100).as_legacy(),
        percent_hbd=50,
        allow_votes=False,
        allow_curation_rewards=True,
    )

    comment_0.assert_is_rc_mana_decreased_after_post_or_update()
    comment_0.assert_is_comment_sent_or_update()
    comment_0.assert_options_are_apply()
    with pytest.raises(tt.exceptions.CommunicationError) as error:
        comment_0.options(
            max_accepted_payout=tt.Asset.Tbd(100).as_legacy(),
            percent_hbd=50,
            allow_votes=True,
            allow_curation_rewards=True,
        )

    assert "Voting cannot be re-enabled." in error.value.error
    comment_0.assert_rc_mana_after_change_comment_options("is_unchanged")


@pytest.mark.parametrize("reply_type", ["reply_another_comment", "no_reply"], ids=["comment", "post"])
def test_set_allow_curation_rewards_as_true_again(prepared_node, wallet, reply_type):
    """
    Test case 13, 14 from issue: https://gitlab.syncad.com/hive/hive/-/issues/503
    """
    comment_0 = Comment(prepared_node, wallet)
    if reply_type == "reply_another_comment":
        comment_0.create_parent_comment()

    comment_0.send(
        reply_type=reply_type,
        max_accepted_payout=tt.Asset.Tbd(100).as_legacy(),
        percent_hbd=50,
        allow_votes=True,
        allow_curation_rewards=False,
    )

    comment_0.assert_is_rc_mana_decreased_after_post_or_update()
    comment_0.assert_is_comment_sent_or_update()
    comment_0.assert_options_are_apply()
    with pytest.raises(tt.exceptions.CommunicationError) as error:
        comment_0.options(
            max_accepted_payout=tt.Asset.Tbd(100).as_legacy(),
            percent_hbd=50,
            allow_votes=True,
            allow_curation_rewards=True,
        )

    assert "Curation rewards cannot be re-enabled." in error.value.error
    comment_0.assert_rc_mana_after_change_comment_options("is_unchanged")


@pytest.mark.parametrize("reply_type", ["reply_another_comment", "no_reply"], ids=["comment", "post"])
def test_change_options_of_comment_with_reply(prepared_node, wallet, reply_type):
    """
    Test case 15, 16 from issue: https://gitlab.syncad.com/hive/hive/-/issues/503
    """
    comment_0 = Comment(prepared_node, wallet)
    if reply_type == "reply_another_comment":
        comment_0.create_parent_comment()

    comment_0.send(reply_type=reply_type)
    comment_0.assert_is_rc_mana_decreased_after_post_or_update()
    comment_0.create_parent_comment()

    comment_0.options(**UPDATED_COMMENT_OPTIONS)
    comment_0.assert_rc_mana_after_change_comment_options("decrease")

    comment_0.assert_is_comment_sent_or_update()
    comment_0.assert_options_are_apply()


@pytest.mark.parametrize("reply_type", ["reply_another_comment", "no_reply"], ids=["comment", "post"])
def test_change_percent_hbd_after_vote(prepared_node, wallet, reply_type):
    """
    Test case 17, 18 from issue: https://gitlab.syncad.com/hive/hive/-/issues/503
    """
    comment_0 = Comment(prepared_node, wallet)
    if reply_type == "reply_another_comment":
        comment_0.create_parent_comment()

    comment_0.send(reply_type=reply_type)
    comment_0.vote()

    comment_0.options(
        percent_hbd=UPDATED_COMMENT_OPTIONS["percent_hbd"],
    )
    comment_0.assert_rc_mana_after_change_comment_options("decrease")
    comment_0.assert_options_are_apply()


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
def test_change_comment_options_after_vote(prepared_node, wallet, reply_type, comment_options):
    """
    Test case 19:24 from issue: https://gitlab.syncad.com/hive/hive/-/issues/503
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
def test_adds_the_beneficiary_after_comment(prepared_node, wallet, reply_type):
    """
    Test case 25, 28 from issue: https://gitlab.syncad.com/hive/hive/-/issues/503
    """
    comment_0 = Comment(prepared_node, wallet)
    if reply_type == "reply_another_comment":
        comment_0.create_parent_comment()

    comment_0.send(reply_type=reply_type)

    comment_0.create_parent_comment()

    comment_0.options(beneficiaries=[{"account": "initminer", "weight": 100}])

    comment_0.assert_rc_mana_after_change_comment_options("decrease")
    comment_0.assert_options_are_apply()


@pytest.mark.parametrize("reply_type", ["reply_another_comment", "no_reply"], ids=["comment", "post"])
def test_try_remove_the_beneficiary_after_comment(prepared_node, wallet, reply_type):
    """
    Test case 26, 29 from issue: https://gitlab.syncad.com/hive/hive/-/issues/503
    """
    comment_0 = Comment(prepared_node, wallet)
    if reply_type == "reply_another_comment":
        comment_0.create_parent_comment()

    comment_0.send(reply_type=reply_type, beneficiaries=[{"account": "initminer", "weight": 100}])

    comment_0.create_parent_comment()

    with pytest.raises(tt.exceptions.CommunicationError) as error:
        comment_0.options(beneficiaries=[])
    assert "Must specify at least one beneficiary" in error.value.error

    comment_0.assert_rc_mana_after_change_comment_options("is_unchanged")


@pytest.mark.parametrize("reply_type", ["reply_another_comment", "no_reply"], ids=["comment", "post"])
def test_try_add_another_beneficiary_after_comment(prepared_node, wallet, reply_type):
    """
    Test case 27, 30 from issue: https://gitlab.syncad.com/hive/hive/-/issues/503
    """
    comment_0 = Comment(prepared_node, wallet)
    if reply_type == "reply_another_comment":
        comment_0.create_parent_comment()

    comment_0.send(reply_type=reply_type, beneficiaries=[{"account": "initminer", "weight": 100}])

    comment_0.create_parent_comment()

    with pytest.raises(tt.exceptions.CommunicationError) as error:
        comment_0.options(beneficiaries=[{"account": "alice", "weight": 100}])

    assert "Comment already has beneficiaries specified." in error.value.error
    comment_0.assert_rc_mana_after_change_comment_options("is_unchanged")


@pytest.mark.parametrize("reply_type", ["reply_another_comment", "no_reply"], ids=["comment", "post"])
def test_adds_the_beneficiary_after_vote(prepared_node, wallet, reply_type):
    """
    Test case 31, 34 from issue: https://gitlab.syncad.com/hive/hive/-/issues/503
    """
    comment_0 = Comment(prepared_node, wallet)
    if reply_type == "reply_another_comment":
        comment_0.create_parent_comment()

    comment_0.send(reply_type=reply_type)

    comment_0.vote()

    with pytest.raises(tt.exceptions.CommunicationError) as error:
        comment_0.options(beneficiaries=[{"account": "initminer", "weight": 100}])

    assert "Comment must not have been voted on before specifying beneficiaries." in error.value.error
    comment_0.assert_rc_mana_after_change_comment_options("is_unchanged")


@pytest.mark.parametrize("reply_type", ["reply_another_comment", "no_reply"], ids=["comment", "post"])
def test_try_remove_the_beneficiary_after_vote(prepared_node, wallet, reply_type):
    """
    Test case 32, 35 from issue: https://gitlab.syncad.com/hive/hive/-/issues/503
    """
    comment_0 = Comment(prepared_node, wallet)
    if reply_type == "reply_another_comment":
        comment_0.create_parent_comment()

    comment_0.send(reply_type=reply_type, beneficiaries=[{"account": "initminer", "weight": 100}])

    comment_0.create_parent_comment()

    with pytest.raises(tt.exceptions.CommunicationError) as error:
        comment_0.options(beneficiaries=[])

    assert "Must specify at least one beneficiary" in error.value.error
    comment_0.assert_rc_mana_after_change_comment_options("is_unchanged")


@pytest.mark.parametrize("reply_type", ["reply_another_comment", "no_reply"], ids=["comment", "post"])
def test_try_add_another_beneficiary_after_vote(prepared_node, wallet, reply_type):
    """
    Test case 33, 36 from issue: https://gitlab.syncad.com/hive/hive/-/issues/503
    """
    comment_0 = Comment(prepared_node, wallet)
    if reply_type == "reply_another_comment":
        comment_0.create_parent_comment()

    comment_0.send(reply_type=reply_type, beneficiaries=[{"account": "initminer", "weight": 100}])

    comment_0.create_parent_comment()

    with pytest.raises(tt.exceptions.CommunicationError) as error:
        comment_0.options(beneficiaries=[{"account": "alice", "weight": 100}])

    assert "Comment already has beneficiaries specified." in error.value.error
    comment_0.assert_rc_mana_after_change_comment_options("is_unchanged")
