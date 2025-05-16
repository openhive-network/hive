from __future__ import annotations

from typing import TYPE_CHECKING

import pytest
from beekeepy.exceptions import ErrorInResponseError

import test_tools as tt
from schemas.fields.hive_datetime import HiveDateTime

if TYPE_CHECKING:
    from python.functional.operation_tests.conftest import ProposalAccount


@pytest.mark.testnet()
@pytest.mark.parametrize(
    ("proposal_creator", "proposal_receiver"),
    [
        ("alice", "alice"),  # User A creates a post, user A creates a proposal with receiver = user A
        ("alice", "bob"),  # User A creates a post, user A creates a proposal with receiver = user B
        ("bob", "alice"),  # User A creates a post, user B creates a proposal with receiver = user A
    ],
    ids=[
        "proposal_receiver_and_creator_is_the_same_account",
        "creator_of_post_is_creator_of_proposal",
        "creator_of_post_is_receiver_of_proposal",
    ],
)
@pytest.mark.parametrize(
    ("update_proposal_args", "test_case_type"),
    [
        ({"daily_pay": tt.Asset.Tbd(4)}, "positive"),
        ({"permlink": "new-permlink"}, "positive"),
        ({"subject": "new-subject"}, "positive"),
        ({"end_date": HiveDateTime(tt.Time.from_now(days=15))}, "positive"),
        ({"daily_pay": tt.Asset.Tbd(6)}, "negative"),
        ({"end_date": HiveDateTime(tt.Time.from_now(days=80))}, "negative"),
    ],
    ids=[
        "reduce the amount of daily_pay",
        "change the permlink",
        "change the subject",
        "short the end_date",
        "increase the amount of daily_pay",
        "extend the end_date",
    ],
)
@pytest.mark.parametrize(
    "end_date",
    [
        HiveDateTime(tt.Time.from_now(days=30)),  # proposal duration is shorter than HIVE_PROPOSAL_FEE_INCREASE_DAYS
        HiveDateTime(tt.Time.from_now(days=65)),  # proposal duration is longer than HIVE_PROPOSAL_FEE_INCREASE_DAYS
    ],
    ids=["end_date_before_60_days", "end_date_after_60_days"],
)
@pytest.mark.parametrize(
    "start_date",
    [
        tt.Time.now(),
        tt.Time.from_now(days=3),
    ],
    ids=["modify_proposal_after_start_date", "modify_proposal_before_start_date"],
)
def test_update_one_parameter_in_proposal(
    alice: ProposalAccount,
    bob: ProposalAccount,
    wallet: tt.Wallet,
    prepared_node: tt.InitNode,
    proposal_creator: str,
    proposal_receiver: str,
    start_date: str,
    end_date: str,
    update_proposal_args: dict,
    test_case_type: str,
) -> None:
    # test cases 1-8, 13-16 from https://gitlab.syncad.com/hive/hive/-/issues/595
    wallet.api.post_comment("alice", "comment-permlink", "", "category", "title", "body", "{}")
    alice.update_account_info()  # to update mana after creating post

    if "permlink" in update_proposal_args:  # post another comment if permlink of proposal is being updated
        prepared_node.restart(
            time_control=tt.StartTimeControl(start_time=prepared_node.get_head_block_time() + tt.Time.minutes(6))
        )  # it's possible to post article every 5 minutes - time jump is needed
        wallet.api.post_comment("alice", "new-permlink", "", "category", "title", "body", "{}")
        alice.update_account_info()

    proposal_creator = eval(proposal_creator)
    proposal_creator.create_proposal(proposal_receiver, start_date, end_date)
    proposal_creator.update_account_info()

    proposal_parameter_to_change = next(iter(update_proposal_args.keys()))
    if test_case_type == "positive":
        transaction = proposal_creator.update_proposal(**update_proposal_args)
        proposal_creator.check_if_rc_current_mana_was_reduced(transaction)
        proposal_creator.check_if_proposal_was_updated(proposal_parameter_to_change)
        proposal_creator.assert_hbd_balance_wasnt_changed()
    else:
        with pytest.raises(ErrorInResponseError) as exception:
            proposal_creator.update_proposal(**update_proposal_args)
        expected_error_message = (
            "You cannot increase the end date of the proposal"
            if (proposal_parameter_to_change == "end_date")
            else "You cannot increase the daily pay"
        )
        assert expected_error_message in exception.value.error


@pytest.mark.parametrize(
    "update_proposal_args",
    [
        {"daily_pay": tt.Asset.Tbd(4)},
        {"permlink": "new-permlink"},
        {"subject": "new-subject"},
        {"end_date": HiveDateTime(tt.Time.from_now(days=15))},
    ],
    ids=["reduce_the_amount_of_daily_pay", "change_the_permlink", "change_the_subject", "short_the_end_date"],
)
@pytest.mark.parametrize(
    "end_date",
    [
        HiveDateTime(tt.Time.from_now(days=30)),  # proposal duration is shorter than HIVE_PROPOSAL_FEE_INCREASE_DAYS
        HiveDateTime(tt.Time.from_now(days=65)),  # proposal duration is longer than HIVE_PROPOSAL_FEE_INCREASE_DAYS
    ],
    ids=["end_date_before_60_days", "end_date_after_60_days"],
)
def test_try_to_update_proposal_from_unauthorised_account(
    alice: ProposalAccount,
    bob: ProposalAccount,
    wallet: tt.Wallet,
    prepared_node: tt.InitNode,
    end_date: str,
    update_proposal_args: dict,
) -> None:
    # test cases 9-12 from https://gitlab.syncad.com/hive/hive/-/issues/595
    wallet.api.post_comment("alice", "comment-permlink", "", "category", "title", "body", "{}")
    alice.update_account_info()  # to update mana after creating post
    bob.create_proposal("alice", tt.Time.now(), end_date)
    with pytest.raises(ErrorInResponseError) as exception:
        alice.update_proposal(proposal_to_update_details=bob.proposal_parameters, **update_proposal_args)
    assert "Cannot edit a proposal you are not the creator of" in exception.value.error
