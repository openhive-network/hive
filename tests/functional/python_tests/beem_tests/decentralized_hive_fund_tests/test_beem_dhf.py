import datetime
from datetime import timedelta
from uuid import uuid4

from beem import Hive
from beem.account import Account
from beembase.operations import Create_proposal
from beembase.operations import Remove_proposal
from beembase.operations import Update_proposal
from beembase.operations import Update_proposal_votes
import dateutil.parser
import test_tools as tt

from . import test_utils
from .conftest import CREATOR,  TREASURY
from ..conftest import NodeClientMaker
from .... import hive_utils


def create_proposal(node, creator_account, receiver_account, wif, subject):
    tt.logger.info("Testing: create_proposal")
    now = datetime.datetime.now()

    start_date, end_date = test_utils.get_start_and_end_date(now, 10, 2)

    try:
        creator = Account(creator_account, hive_instance=node)
    except Exception as ex:
        tt.logger.error(f"Account: {creator_account} not found. {ex}")
        raise ex

    try:
        receiver = Account(receiver_account, hive_instance=node)
    except Exception as ex:
        tt.logger.error(f"Account: {receiver_account} not found. {ex}")
        raise ex

    ret = node.post(
        "Hivepy proposal title",
        "Hivepy proposal body",
        creator["name"],
        permlink="hivepy-proposal-title",
        tags="proposals",
    )

    op = Create_proposal(
        **{
            "creator": creator["name"],
            "receiver": receiver["name"],
            "start_date": start_date,
            "end_date": end_date,
            "daily_pay": "16.000 TBD",
            "subject": subject,
            "permlink": "hivepy-proposal-title",
        }
    )
    ret = None
    try:
        ret = node.finalizeOp(op, creator["name"], "active")
    except Exception as ex:
        tt.logger.exception(f"Exception: {ex}")
        raise ex

    assert ret["operations"][0][1]["creator"] == creator["name"]
    assert ret["operations"][0][1]["receiver"] == receiver["name"]
    assert ret["operations"][0][1]["start_date"] == start_date
    assert ret["operations"][0][1]["end_date"] == end_date
    assert ret["operations"][0][1]["daily_pay"] == "16.000 TBD"
    assert ret["operations"][0][1]["subject"] == subject
    assert ret["operations"][0][1]["permlink"] == "hivepy-proposal-title"


def list_proposals(node, account, wif, subject):
    tt.logger.info("Testing: list_proposals")
    # list inactive proposals, our proposal shoud be here
    proposals = node.rpc.list_proposals([account], 1000, "by_creator", "ascending", "inactive")
    found = None
    for proposal in proposals:
        if proposal["subject"] == subject:
            found = proposal

    assert found is not None

    # list active proposals, our proposal shouldnt be here
    proposals = node.rpc.list_proposals([account], 1000, "by_creator", "ascending", "active")
    found = None
    for proposal in proposals:
        if proposal["subject"] == subject:
            found = proposal

    assert found is None

    # list all proposals, our proposal should be here
    proposals = node.rpc.list_proposals([account], 1000, "by_creator", "ascending", "all")

    found = None
    for proposal in proposals:
        if proposal["subject"] == subject:
            found = proposal

    assert found is not None


def find_proposals(node, account, wif, subject):
    tt.logger.info("Testing: find_proposals")
    # first we will find our special proposal and get its proposal_id
    proposals = node.rpc.list_proposals([account], 1000, "by_creator", "ascending", "inactive")

    found = None
    for proposal in proposals:
        if proposal["subject"] == subject:
            found = proposal

    assert found is not None
    proposal_id = int(found["proposal_id"])

    ret = node.rpc.find_proposals([proposal_id])
    assert ret[0]["subject"] == found["subject"]


def vote_proposal(node, account, wif, subject):
    tt.logger.info("Testing: vote_proposal")
    # first we will find our special proposal and get its proposal_id
    proposals = node.rpc.list_proposals([account], 1000, "by_creator", "ascending", "inactive")

    found = None
    for proposal in proposals:
        if proposal["subject"] == subject:
            found = proposal

    assert found is not None
    proposal_id = int(found["proposal_id"])

    # now lets vote

    op = Update_proposal_votes(**{"voter": account, "proposal_ids": [proposal_id], "approve": True})

    ret = None
    try:
        ret = node.finalizeOp(op, account, "active")
    except Exception as ex:
        tt.logger.exception(f"Exception: {ex}")
        raise ex

    assert ret["operations"][0][1]["voter"] == account
    assert ret["operations"][0][1]["proposal_ids"][0] == proposal_id
    assert ret["operations"][0][1]["approve"] == True
    hive_utils.common.wait_n_blocks(node.rpc.url, 2)


def list_voter_proposals(node, account, wif, subject):
    tt.logger.info("Testing: list_voter_proposals")
    voter_proposals = node.rpc.list_proposal_votes([account], 1000, "by_voter_proposal", "ascending", "inactive")

    found = None
    for proposals in voter_proposals:
        if proposals["proposal"]["subject"] == subject:
            found = proposals

    assert found is not None


def remove_proposal(node, account, wif, subject):
    tt.logger.info("Testing: remove_proposal")
    # first we will find our special proposal and get its proposal_id
    proposals = node.rpc.list_proposals([account], 1000, "by_creator", "ascending", "inactive")

    found = None
    for proposal in proposals:
        if proposal["subject"] == subject:
            found = proposal

    assert found is not None, "Not found"
    proposal_id = int(found["proposal_id"])

    # remove proposal
    print(account)

    op = Remove_proposal(**{"voter": account, "proposal_owner": account, "proposal_ids": [proposal_id]})

    try:
        node.finalizeOp(op, account, "active")
    except Exception as ex:
        tt.logger.exception(f"Exception: {ex}")
        raise ex

    # try to find our special proposal
    proposals = node.rpc.list_proposals([account], 1000, "by_creator", "ascending", "inactive")

    found = None
    for proposal in proposals:
        if proposal["subject"] == subject:
            found = proposal

    assert found is None, "Not found"


def iterate_results_test(node, creator_account, receiver_account, wif, subject, remove):
    tt.logger.info("Testing: test_iterate_results_test")
    # test for iterate prosals
    # 1 we will create n proposals of which k proposal will have the same value in one of the fields
    # 2 then we will list proposals starting from kth proposal with limit set to m < k
    # 3 we list proposals again with the same conditiona as in 2, we should get the same set of results
    #   in real life scenatio pagination scheme with limit set to value lower than "k" will be showing
    #   the same results and will hang
    # 4 then we will use newly introduced last_id field, we should see diferent set of proposals
    try:
        creator = Account(creator_account, hive_instance=node)
    except Exception as ex:
        tt.logger.error(f"Account: {creator_account} not found. {ex}")
        raise ex

    try:
        receiver = Account(receiver_account, hive_instance=node)
    except Exception as ex:
        tt.logger.error(f"Account: {receiver_account} not found. {ex}")
        raise ex

    now = datetime.datetime.now()

    # 1 we will create n proposals of which k proposal will have the same value in one of the fields
    # here we have 5 proposals with the same start date
    start_end_pairs = [[1, 1], [2, 2], [4, 3], [5, 4], [5, 5], [5, 6], [5, 7], [5, 8], [6, 9]]

    for start_end_pair in start_end_pairs:
        start_date, end_date = test_utils.get_start_and_end_date(now, start_end_pair[0], start_end_pair[1])
        op = Create_proposal(
            **{
                "creator": creator["name"],
                "receiver": receiver["name"],
                "start_date": start_date,
                "end_date": end_date,
                "daily_pay": "16.000 TBD",
                "subject": subject,
                "permlink": "hivepy-proposal-title",
            }
        )
        try:
            node.finalizeOp(op, creator["name"], "active")
        except Exception as ex:
            tt.logger.exception(f"Exception: {ex}")
            raise ex
    hive_utils.common.wait_n_blocks(node.rpc.url, 5)

    start_date = test_utils.date_to_iso(now + datetime.timedelta(days=5))

    # 2 then we will list proposals starting from kth proposal with limit set to m < k
    proposals = node.rpc.list_proposals([start_date], 3, "by_start_date", "descending", "all")
    assert len(proposals) == 3, f"Expected {3} elements got {len(proposals)}"
    ids = []
    for proposal in proposals:
        assert proposal["start_date"] == start_date, "Expected start_date do not match {} != {}".format(
            start_date, proposals[-1]["start_date"]
        )
        ids.append(proposal["proposal_id"])
    assert len(ids) == 3, f"Expected {3} elements got {len(ids)}"

    # 3 we list proposals again with the same conditiona as in 2, we should get the same set of results
    proposals = node.rpc.list_proposals([start_date], 3, "by_start_date", "descending", "all")
    assert len(proposals) == 3, f"Expected {3} elements got {len(proposals)}"
    oids = []
    for proposal in proposals:
        assert proposal["start_date"] == start_date, "Expected start_date do not match {} != {}".format(
            start_date, proposals[-1]["start_date"]
        )
        oids.append(proposal["proposal_id"])
    assert len(oids) == 3, f"Expected {3} elements got {len(oids)}"

    # the same set of results check
    for id in ids:
        assert id in oids, f"Id not found in expected results array {id}"

    # 4 then we will use newly introduced last_id field, we should see diferent set of proposals
    proposals = node.rpc.list_proposals([start_date], 3, "by_start_date", "descending", "all", oids[-1])

    start_date, end_date = test_utils.get_start_and_end_date(now, 5, 4)

    assert proposals[-1]["start_date"] == start_date, "Expected start_date do not match {} != {}".format(
        start_date, proposals[-1]["start_date"]
    )
    assert proposals[-1]["end_date"] == end_date, "Expected end_date do not match {} != {}".format(
        end_date, proposals[-1]["end_date"]
    )

    # remove all created proposals

    if not remove:
        start_date = test_utils.date_to_iso(now + datetime.timedelta(days=6))
        for _ in range(0, 2):
            proposals = node.list_proposals([start_date], 5, "by_start_date", "descending", "all")
            ids = []
            for proposal in proposals:
                ids.append(int(proposal["proposal_id"]))

            op = Remove_proposal(**{"voter": creator["name"], "proposal_ids": ids})
            try:
                node.finalizeOp(op, creator["name"], "active")
            except Exception as ex:
                tt.logger.exception(f"Exception: {ex}")
                raise ex
            hive_utils.common.wait_n_blocks(node.rpc.url, 3)


def update_proposal(node, creator, wif):
    tt.logger.info("Testing: update_proposal without updating the end date")
    proposals = node.rpc.list_proposals([creator], 1000, "by_creator", "ascending", "all")
    print(proposals[0])
    new_subject = "Some new proposal subject"
    new_daily_pay = "15.000 TBD"
    op = Update_proposal(
        **{
            "proposal_id": proposals[0]["proposal_id"],
            "creator": proposals[0]["creator"],
            "daily_pay": new_daily_pay,
            "subject": new_subject,
            "permlink": proposals[0]["permlink"],
        }
    )
    try:
        node.finalizeOp(op, creator, "active")
    except Exception as ex:
        tt.logger.exception(f"Exception: {ex}")
        raise ex
    hive_utils.common.wait_n_blocks(node.rpc.url, 3)

    proposals = node.rpc.list_proposals([creator], 1000, "by_creator", "ascending", "all")
    print(proposals[0])
    assert proposals[0]["subject"] == new_subject, "Subjects dont match"
    assert proposals[0]["daily_pay"] == new_daily_pay, "daily pay dont match"

    tt.logger.info("Testing: update_proposal and updating the end date")
    end_date = test_utils.date_to_iso(dateutil.parser.parse(proposals[0]["end_date"]) - timedelta(days=1))

    op = Update_proposal(
        **{
            "proposal_id": proposals[0]["proposal_id"],
            "creator": proposals[0]["creator"],
            "daily_pay": "15.000 TBD",
            "subject": new_subject,
            "prefix": "TST",
            "permlink": proposals[0]["permlink"],
            "end_date": end_date,
        }
    )
    try:
        node.finalizeOp(op, creator, "active")
    except Exception as ex:
        tt.logger.exception(f"Exception: {ex}")
        raise ex
    hive_utils.common.wait_n_blocks(node.rpc.url, 3)

    proposals = node.rpc.list_proposals([creator], 1000, "by_creator", "ascending", "all")
    print(proposals[0])
    assert proposals[0]["end_date"] == end_date, "End date doesn't match"


def test_beem_dhf(node_client: NodeClientMaker):
    no_erase_proposal = True

    wif = tt.Account("initminer").private_key
    node_client = node_client()

    subject = str(uuid4())
    tt.logger.info(f"Subject of testing proposal is set to: {subject}")

    create_proposal(node_client, CREATOR, TREASURY, wif, subject)
    hive_utils.common.wait_n_blocks(node_client.rpc.url, 3)
    list_proposals(node_client, CREATOR, wif, subject)
    find_proposals(node_client, CREATOR, wif, subject)
    vote_proposal(node_client, CREATOR, wif, subject)
    list_voter_proposals(node_client, CREATOR, wif, subject)
    hive_utils.common.wait_n_blocks(node_client.rpc.url, 3)
    if not no_erase_proposal:
        remove_proposal(node_client, CREATOR, wif, subject)
    iterate_results_test(node_client, CREATOR, TREASURY, wif, str(uuid4()), no_erase_proposal)
    hive_utils.common.wait_n_blocks(node_client.rpc.url, 3)
    update_proposal(node_client, CREATOR, wif)
