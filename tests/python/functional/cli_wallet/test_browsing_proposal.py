from __future__ import annotations

from datetime import datetime as dt
from typing import TYPE_CHECKING

from hive_local_tools.functional.python.cli_wallet import (
    FundedAccountInfo,
    PreparedProposalDataWithId,
    create_funded_account,
    format_datetime,
    get_list_proposal_args,
    get_list_proposal_votes_args,
    prepare_proposal,
)

if TYPE_CHECKING:
    import test_tools as tt

active_values = ["all", "inactive", "active", "expired", "votable"]
proposals_order_by_values = ["by_creator", "by_start_date", "by_end_date", "by_total_votes"]
votes_order_by_values = ["by_voter_proposal"]
order_type_values = ["ascending", "descending"]


def test_list_proposals(
    wallet: tt.OldWallet, creator_proposal_id: PreparedProposalDataWithId, creator: tt.Account
) -> None:
    start_point_before_test = format_datetime(dt.now())

    [proposals_order_by_values[0]]
    date_order_by = [proposals_order_by_values[1], proposals_order_by_values[2]]
    value_order_by = [proposals_order_by_values[3]]

    for active in active_values:
        for order_type in order_type_values:
            for order_by in proposals_order_by_values:
                if order_by in date_order_by:
                    start = [start_point_before_test]
                elif order_by in value_order_by:
                    start = [0]
                else:
                    start = [creator.name]

                wallet.api.list_proposals(
                    **get_list_proposal_args(start=start, order_by=order_by, order_type=order_type, status=active)
                )


def test_list_voter_proposal(
    wallet: tt.OldWallet, creator_proposal_id: PreparedProposalDataWithId, creator: tt.Account
) -> None:
    wallet.api.update_proposal_votes(voter=creator.name, proposals=[creator_proposal_id.id], approve=True)

    for active in active_values:
        for order_by in votes_order_by_values:
            for order_type in order_type_values:
                wallet.api.list_proposal_votes(
                    **get_list_proposal_votes_args(
                        start=[creator.name], order_by=order_by, order_type=order_type, status=active
                    )
                )


def test_find_proposals(wallet: tt.OldWallet, creator: tt.Account) -> None:
    accounts_count = 8
    proposal_id_test_scheme = [[1], [2], [3], [1, 2], [1, 2, 3], [2, 3], [3, 4], [4, 5], [1, 2, 3, 4, 5, 6, 7]]

    def inline_create_proposal(input_account: FundedAccountInfo):
        data = prepare_proposal(input=input_account, author_is_creator=False)
        # with wallet.in_single_transaction():
        wallet.api.post_comment(**data.post_comment_arguments)
        wallet.api.create_proposal(**data.create_proposal_arguments)

    # create proposal for each account
    accounts = [create_funded_account(creator=creator, wallet=wallet, id=i) for i in range(accounts_count)]
    [inline_create_proposal(input_account=acc) for acc in accounts]

    for proposal_test_case in proposal_id_test_scheme:
        result = wallet.api.find_proposals(proposal_ids=proposal_test_case)
        assert len(result) == len(proposal_test_case), f"result: {result}"
        result_ids = [item["id"] for item in result]
        assert proposal_test_case == result_ids, f"expected: {proposal_test_case}\ngiven:{result_ids}"
