from datetime import timedelta

import pytest

import test_tools as tt
from hive_local_tools.functional.python.cli_wallet import (
    prepared_proposal_data_with_id,
    get_list_proposal_args,
    get_list_proposal_votes_args,
    funded_account_info,
    find_proposals_by_creator_name,
    find_proposals_by_voter_name,
    prepare_proposal,
    format_datetime,
)


def list_proposals_by_creator(wallet: tt.Wallet, creator_name: str) -> list:
    all_proposals = wallet.api.list_proposals(**get_list_proposal_args(creator_name))
    return find_proposals_by_creator_name(_creator=creator_name, _proposal_list=all_proposals)


def list_proposal_votes_by_voter(wallet: tt.Wallet, voter_name: str):
    all_proposal_votes = wallet.api.list_proposal_votes(**get_list_proposal_votes_args(start=[voter_name]))
    return find_proposals_by_voter_name(voter_name, all_proposal_votes)


def test_create_proposal(wallet: tt.Wallet, creator: tt.Account, creator_proposal_id):
    creator_proposals_after_count = len(list_proposals_by_creator(wallet, creator.name))
    assert creator_proposals_after_count == 1


def test_remove_proposal(
    wallet: tt.Wallet,
    funded_account: funded_account_info,
    creator_proposal_id: prepared_proposal_data_with_id,
    account_proposal_id: prepared_proposal_data_with_id,
):
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
    wallet: tt.Wallet, creator_proposal_id: prepared_proposal_data_with_id, creator: tt.Account
):
    voter_proposals_before_count = len(list_proposal_votes_by_voter(wallet, creator.name))

    wallet.api.update_proposal_votes(voter=creator.name, proposals=[creator_proposal_id.id], approve=True)

    voter_proposals_after_count = len(list_proposal_votes_by_voter(wallet, creator.name))
    assert voter_proposals_before_count + 1 == voter_proposals_after_count


def test_create_proposal_fail_negative_payment(
    wallet: tt.Wallet, funded_account: funded_account_info, creator: tt.Account
):
    assert len(list_proposals_by_creator(wallet, creator.name)) == 0

    prepared_proposal = prepare_proposal(funded_account)
    prepared_proposal.create_proposal_arguments["daily_pay"] = tt.Asset.Tbd(-1)  # "-1.000 TBD"
    wallet.api.post_comment(**prepared_proposal.post_comment_arguments)

    with pytest.raises(tt.exceptions.CommunicationError) as exception:
        wallet.api.create_proposal(**prepared_proposal.create_proposal_arguments)

    response = exception.value.response
    assert "daily_pay.amount >= 0: Daily pay can't be negative value" in response["error"]["message"]

    assert len(list_proposals_by_creator(wallet, creator.name)) == 0


def test_update_proposal_xxx(wallet: tt.Wallet, funded_account: funded_account_info):
    from datetime import datetime as date_type

    def check_is_proposal_update_exists(block_number: int, end_date: date_type) -> bool:
        from time import sleep

        if not isinstance(end_date, str):
            end_date = format_datetime(end_date)

        TIME_TO_MAKE_BLOCKS_IRREVERRSIBBLE = 3 * 22

        tt.logger.info(f"Awaiting for making current reversible - irreversible: {TIME_TO_MAKE_BLOCKS_IRREVERRSIBBLE} s")
        sleep(TIME_TO_MAKE_BLOCKS_IRREVERRSIBBLE)
        tt.logger.info("End of waiting")

        for bn in range(block_number, block_number + 5):
            tt.logger.info(f"checking block: {bn}")
            response = wallet.api.get_ops_in_block(block_num=bn, only_virtual=False)
            if len(response) > 0:
                tt.logger.info(f"got_ops_in_block response: {response}")
                break

        op = response[0]["op"]
        extensions = op[1]["extensions"][0]
        return extensions[0] == "update_proposal_end_date" and extensions[1]["end_date"] == end_date

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
        "daily_pay": str(current_daily_pay),
        "subject": "updated subject",
        "permlink": prepared_proposal.permlink,
        "end_date": format_datetime(prepared_proposal.end_date - timedelta(days=2)),
    }
    block = wallet.api.update_proposal(**update_args)["ref_block_num"] + 1
    assert check_is_proposal_update_exists(block, update_args["end_date"])

    proposal = wallet.api.find_proposals([proposal_id])[0]

    assert proposal["daily_pay"] == current_daily_pay
    assert proposal["subject"] == update_args["subject"]
    assert proposal["permlink"] == prepared_proposal.permlink
    assert proposal["end_date"] == update_args["end_date"]

    tt.logger.info("testing updating the proposal without the end date")
    from copy import deepcopy

    last_date = deepcopy(update_args["end_date"])

    current_daily_pay -= tt.Asset.Tbd(1)
    tt.logger.info(current_daily_pay)
    update_args["daily_pay"] = str(current_daily_pay)
    update_args["subject"] = "updated subject again"
    update_args["end_date"] = None

    wallet.api.update_proposal(**update_args)
    proposal = wallet.api.find_proposals([proposal_id])[0]

    assert proposal["daily_pay"] == current_daily_pay
    assert proposal["subject"] == update_args["subject"]
    assert proposal["permlink"] == prepared_proposal.permlink
    assert proposal["end_date"] == last_date
