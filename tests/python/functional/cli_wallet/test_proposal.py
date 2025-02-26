from __future__ import annotations

from datetime import timedelta

import pytest
from beekeepy.exceptions import CommunicationError

import test_tools as tt
from hive_local_tools import run_for
from hive_local_tools.functional.python.cli_wallet import (
    FundedAccountInfo,
    PreparedProposalDataWithId,
    find_proposals_by_creator_name,
    find_proposals_by_voter_name,
    format_datetime,
    get_list_proposal_args,
    get_list_proposal_votes_args,
    prepare_proposal,
)


def list_proposals_by_creator(wallet: tt.OldWallet, creator_name: str) -> list:
    all_proposals = wallet.api.list_proposals(**get_list_proposal_args(creator_name))
    return find_proposals_by_creator_name(_creator=creator_name, _proposal_list=all_proposals)


def list_proposal_votes_by_voter(wallet: tt.OldWallet, voter_name: str) -> list:
    all_proposal_votes = wallet.api.list_proposal_votes(**get_list_proposal_votes_args(start=[voter_name]))
    return find_proposals_by_voter_name(voter_name, all_proposal_votes)


def test_create_proposal(wallet: tt.OldWallet, creator: tt.Account, creator_proposal_id) -> None:
    creator_proposals_after_count = len(list_proposals_by_creator(wallet, creator.name))
    assert creator_proposals_after_count == 1


def test_remove_proposal(
    wallet: tt.OldWallet,
    funded_account: FundedAccountInfo,
    creator_proposal_id: PreparedProposalDataWithId,
    account_proposal_id: PreparedProposalDataWithId,
) -> None:
    wallet.api.remove_proposal(deleter=funded_account.creator.name, ids=[creator_proposal_id.id])

    proposals_after_creator = list_proposals_by_creator(wallet, creator_name=funded_account.creator.name)
    proposals_after_account = list_proposals_by_creator(wallet, creator_name=funded_account.account.name)
    assert len(proposals_after_creator) == 0
    assert len(proposals_after_account) == 1
    assert (
        len([x for x in proposals_after_creator if x["id"] == creator_proposal_id.id]) == 0
    ), "removed invalid proposal"
    assert (
        len([x for x in proposals_after_account if x["id"] == creator_proposal_id.id]) == 0
    ), "removed invalid proposal"


def test_update_proposal_votes(
    wallet: tt.OldWallet, creator_proposal_id: PreparedProposalDataWithId, creator: tt.Account
) -> None:
    voter_proposals_before_count = len(list_proposal_votes_by_voter(wallet, creator.name))

    wallet.api.update_proposal_votes(voter=creator.name, proposals=[creator_proposal_id.id], approve=True)

    voter_proposals_after_count = len(list_proposal_votes_by_voter(wallet, creator.name))
    assert voter_proposals_before_count + 1 == voter_proposals_after_count


def test_create_proposal_fail_negative_payment(
    wallet: tt.OldWallet, funded_account: FundedAccountInfo, creator: tt.Account
) -> None:
    assert len(list_proposals_by_creator(wallet, creator.name)) == 0

    prepared_proposal = prepare_proposal(funded_account)
    # create asset with negative value manually - pydantic doesn't allow to do it via constructor
    negative_value = tt.Asset.Tbd(1)
    negative_value.amount = -1000
    prepared_proposal.create_proposal_arguments["daily_pay"] = negative_value  # "-1.000 TBD"
    wallet.api.post_comment(**prepared_proposal.post_comment_arguments)

    with pytest.raises(CommunicationError) as exception:
        wallet.api.create_proposal(**prepared_proposal.create_proposal_arguments)

    response = exception.value.get_response_error_messages()[0]
    assert "daily_pay.amount >= 0" in response
    assert "Daily pay can't be negative value" in response

    assert len(list_proposals_by_creator(wallet, creator.name)) == 0


@run_for("testnet", enable_plugins=["account_history_api"])
def test_update_proposal_xxx(wallet: tt.OldWallet, funded_account: FundedAccountInfo) -> None:
    from datetime import datetime as date_type

    def check_is_proposal_update_exists(block_number: int, end_date: date_type) -> bool:
        from time import sleep

        if not isinstance(end_date, str):
            end_date = format_datetime(end_date)

        time_to_make_blocks_irreverrsibble = 3 * 22

        tt.logger.info(f"Awaiting for making current reversible - irreversible: {time_to_make_blocks_irreverrsibble} s")
        sleep(time_to_make_blocks_irreverrsibble)
        tt.logger.info("End of waiting")

        for bn in range(block_number, block_number + 5):
            tt.logger.info(f"checking block: {bn}")
            response = wallet.api.get_ops_in_block(block_num=bn, only_virtual=False)
            if len(response) > 0:
                tt.logger.info(f"got_ops_in_block response: {response}")
                break

        op = response[0]["op"]
        extensions = op[1]["extensions"][0]
        return extensions[0] == 1 and extensions[1]["end_date"] == end_date

    author = funded_account.account
    current_daily_pay = tt.Asset.Tbd(10)

    prepared_proposal = prepare_proposal(input=funded_account, author_is_creator=False)
    wallet.api.post_comment(**prepared_proposal.post_comment_arguments)

    prepared_proposal.create_proposal_arguments["daily_pay"] = current_daily_pay
    wallet.api.create_proposal(**prepared_proposal.create_proposal_arguments)

    proposals = list_proposals_by_creator(wallet=wallet, creator_name=author.name)
    assert len(proposals) > 0
    proposal_id = proposals[0]["id"]

    current_daily_pay -= tt.Asset.Tbd(1)
    update_args = {
        "proposal_id": proposal_id,
        "creator": author.name,
        "daily_pay": current_daily_pay,
        "subject": "updated subject",
        "permlink": prepared_proposal.permlink,
        "end_date": format_datetime(prepared_proposal.end_date - timedelta(days=2)),
    }
    block = wallet.api.update_proposal(**update_args)["ref_block_num"] + 1
    assert check_is_proposal_update_exists(block, update_args["end_date"])

    proposal = wallet.api.find_proposals([proposal_id])[0]

    assert proposal["daily_pay"] == current_daily_pay.as_legacy()
    assert proposal["subject"] == update_args["subject"]
    assert proposal["permlink"] == prepared_proposal.permlink
    assert proposal["end_date"] == update_args["end_date"]

    tt.logger.info("testing updating the proposal without the end date")
    from copy import deepcopy

    last_date = deepcopy(update_args["end_date"])

    current_daily_pay -= tt.Asset.Tbd(1)
    tt.logger.info(current_daily_pay)
    update_args["daily_pay"] = current_daily_pay
    update_args["subject"] = "updated subject again"
    update_args["end_date"] = None

    wallet.api.update_proposal(**update_args)
    proposal = wallet.api.find_proposals([proposal_id])[0]

    assert proposal["daily_pay"] == current_daily_pay.as_legacy()
    assert proposal["subject"] == update_args["subject"]
    assert proposal["permlink"] == prepared_proposal.permlink
    assert proposal["end_date"] == last_date
