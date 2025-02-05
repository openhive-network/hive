from __future__ import annotations

from typing import TYPE_CHECKING

import pytest
from helpy.exceptions import ErrorInResponseError

import test_tools as tt
from hive_local_tools.functional.python.operation import get_number_of_active_proposals, get_virtual_operations
from schemas.operations.virtual.proposal_fee_operation import ProposalFeeOperation

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
def test_create_proposal(
    alice: ProposalAccount,
    bob: ProposalAccount,
    wallet: tt.Wallet,
    prepared_node: tt.InitNode,
    proposal_creator: str,
    proposal_receiver: str,
    end_date: str,
) -> None:
    # test cases 1-6 from https://gitlab.syncad.com/hive/hive/-/issues/594
    wallet.api.post_comment("alice", "comment-permlink", "", "category", "title", "body", "{}")
    alice.update_account_info()  # to update mana after creating post
    proposal_creator = eval(proposal_creator)
    transaction = proposal_creator.create_proposal(proposal_receiver, tt.Time.now(), end_date)
    proposal_creator.check_if_rc_current_mana_was_reduced(transaction)
    proposal_creator.check_if_hive_treasury_fee_was_substracted_from_account()
    assert (
        get_number_of_active_proposals(prepared_node) == 1
    ), "Number of active proposals is wrong. Proposal wasn't created."
    assert (
        len(get_virtual_operations(prepared_node, ProposalFeeOperation, start_block=1)) == 1
    ), "Virtual operation proposal_fee_operation was not generated."


@pytest.mark.testnet()
@pytest.mark.parametrize(
    ("proposal_creator", "proposal_receiver"),
    [
        ("bob", "bob"),  # User A creates a post, user B tries to create a proposal with receiver = user B
        ("bob", "carol"),  # A creates a post, user B tries to create a proposal with receiver = user C
    ],
    ids=["proposal_creator_and_receiver_is_the_same_account", "proposal_creator_and_receiver_are_different"],
)
@pytest.mark.parametrize(
    "end_date",
    [
        tt.Time.from_now(days=30),  # proposal duration is shorter than HIVE_PROPOSAL_FEE_INCREASE_DAYS
        tt.Time.from_now(days=65),  # proposal duration is longer than HIVE_PROPOSAL_FEE_INCREASE_DAYS
    ],
    ids=["end_date_before_60_days", "end_date_after_60_days"],
)
def test_try_to_create_proposal(
    alice: ProposalAccount,
    bob: ProposalAccount,
    carol: ProposalAccount,
    wallet: tt.Wallet,
    prepared_node: tt.InitNode,
    proposal_creator: str,
    proposal_receiver: str,
    end_date: str,
) -> None:
    # test cases 7-10 from https://gitlab.syncad.com/hive/hive/-/issues/594
    proposal_creator = eval(proposal_creator)
    wallet.api.post_comment("alice", "comment-permlink", "", "category", "title", "body", "{}")
    with pytest.raises(ErrorInResponseError) as error:
        proposal_creator.create_proposal(proposal_receiver, tt.Time.now(), end_date)
    assert "Proposal permlink must point to the article posted by creator or receiver" in error.value.error
    assert get_number_of_active_proposals(prepared_node) == 0, "It shouldn't be any active proposals."
