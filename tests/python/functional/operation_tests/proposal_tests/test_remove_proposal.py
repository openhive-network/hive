from __future__ import annotations

from typing import TYPE_CHECKING

import pytest
from helpy.exceptions import ErrorInResponseError

import test_tools as tt
from hive_local_tools.functional.python.operation import get_number_of_active_proposals

if TYPE_CHECKING:
    from python.functional.operation_tests.conftest import ProposalAccount


@pytest.mark.testnet()
@pytest.mark.parametrize(
    ("proposal_creator", "proposal_receiver"),
    [
        ("alice", "alice"),  # User A creates a post, user A creates a proposal with receiver = user A
        ("alice", "bob"),  # User A creates a post, user A creates a proposal with receiver = user B
        ("bob", "alice"),  # # User A creates a post, user B creates a proposal with receiver = user A
    ],
    ids=[
        "post_and_proposal_is_done_by_one_account",
        "creator_of_post_is_creator_of_proposal",
        "creator_of_post_is_receiver_of_proposal",
    ],
)
@pytest.mark.parametrize(
    "end_date",
    [
        tt.Time.from_now(days=30),  # proposal duration is shorter than HIVE_PROPOSAL_FEE_INCREASE_DAYS
        tt.Time.from_now(days=65),  # proposal duration is longer than HIVE_PROPOSAL_FEE_INCREASE_DAYS
    ],
    ids=["end_date_before_60_days", "end_date_after_60_days"],
)
@pytest.mark.parametrize(
    "start_date",
    [
        tt.Time.now(),
        tt.Time.from_now(days=3),
    ],
    ids=["remove_proposal_after_start_date", "remove_proposal_before_start_date"],
)
def test_remove_proposal(
    alice, bob, wallet, prepared_node, proposal_creator, proposal_receiver, start_date, end_date
) -> None:
    # test cases 1-3 from https://gitlab.syncad.com/hive/hive/-/issues/596
    wallet.api.post_comment("alice", "comment-permlink", "", "category", "title", "body", "{}")
    alice.update_account_info()  # to update mana after creating post
    proposal_creator = eval(proposal_creator)
    proposal_creator.create_proposal(proposal_receiver, tt.Time.now(), end_date)
    proposal_creator.update_account_info()
    transaction = proposal_creator.remove_proposal()
    proposal_creator.check_if_rc_current_mana_was_reduced(transaction)
    proposal_creator.assert_hbd_balance_wasnt_changed()
    assert (
        get_number_of_active_proposals(prepared_node) == 0
    ), "Number of active proposals is wrong. Proposal wasn't removed."


@pytest.mark.parametrize(
    "end_date",
    [
        tt.Time.from_now(days=30),  # proposal duration is shorter than HIVE_PROPOSAL_FEE_INCREASE_DAYS
        tt.Time.from_now(days=65),  # proposal duration is longer than HIVE_PROPOSAL_FEE_INCREASE_DAYS
    ],
    ids=["end_date_before_60_days", "end_date_after_60_days"],
)
def test_try_to_remove_proposal_from_unauthorised_account(
    alice: ProposalAccount, bob: ProposalAccount, wallet: tt.Wallet, prepared_node: tt.InitNode, end_date: str
) -> None:
    # test cases 4 from https://gitlab.syncad.com/hive/hive/-/issues/596
    wallet.api.post_comment("alice", "comment-permlink", "", "category", "title", "body", "{}")
    alice.update_account_info()  # to update mana after creating post
    bob.create_proposal("alice", tt.Time.now(), end_date)
    with pytest.raises(ErrorInResponseError) as exception:
        alice.remove_proposal(proposal_to_remove_details=bob.proposal_parameters)
    assert "Only proposal owner can remove it..." in exception.value.error
