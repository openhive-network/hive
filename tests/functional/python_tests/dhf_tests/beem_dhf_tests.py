#!/usr/bin/python3

import sys

sys.path.append("../../")
import hive_utils

from uuid import uuid4
import logging
import os
import test_utils

from hive_utils.common import junit_test_case
from junit_xml import TestSuite

MODULE_NAME = "Beem-Dhf-Tester"
LOG_LEVEL = logging.INFO
LOG_FORMAT = "%(asctime)-15s - %(name)s - %(levelname)s - %(message)s"

MAIN_LOG_PATH = "beem_dhf_tests.log"
log_dir = os.environ.get("TEST_LOG_DIR", None)
if log_dir is not None:
    MAIN_LOG_PATH = log_dir + "/" + MAIN_LOG_PATH
else:
    MAIN_LOG_PATH = "./" + MAIN_LOG_PATH

logger = logging.getLogger(MODULE_NAME)
logger.setLevel(LOG_LEVEL)

ch = logging.StreamHandler(sys.stdout)
ch.setLevel(LOG_LEVEL)
ch.setFormatter(logging.Formatter(LOG_FORMAT))

fh = logging.FileHandler(MAIN_LOG_PATH)
fh.setLevel(LOG_LEVEL)
fh.setFormatter(logging.Formatter(LOG_FORMAT))

if not logger.hasHandlers():
    logger.addHandler(ch)
    logger.addHandler(fh)

try:
    from beem import Hive
except Exception as ex:
    print("beem library is not installed.")
    sys.exit(1)


@junit_test_case
def test_create_proposal(node, creator_account, receiver_account, wif, subject):
    logger.info("Testing: create_proposal")
    s = Hive(node=[node], no_broadcast=False, keys=[wif])

    import datetime

    now = datetime.datetime.now()

    start_date, end_date = test_utils.get_start_and_end_date(now, 10, 2)

    from beem.account import Account

    try:
        creator = Account(creator_account, hive_instance=s)
    except Exception as ex:
        logger.error("Account: {} not found. {}".format(creator_account, ex))
        raise ex

    try:
        receiver = Account(receiver_account, hive_instance=s)
    except Exception as ex:
        logger.error("Account: {} not found. {}".format(receiver_account, ex))
        raise ex

    ret = s.post(
        "Hivepy proposal title",
        "Hivepy proposal body",
        creator["name"],
        permlink="hivepy-proposal-title",
        tags="proposals",
    )
    from beembase.operations import Create_proposal

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
        ret = s.finalizeOp(op, creator["name"], "active")
    except Exception as ex:
        logger.exception("Exception: {}".format(ex))
        raise ex

    assert ret["operations"][0][1]["creator"] == creator["name"]
    assert ret["operations"][0][1]["receiver"] == receiver["name"]
    assert ret["operations"][0][1]["start_date"] == start_date
    assert ret["operations"][0][1]["end_date"] == end_date
    assert ret["operations"][0][1]["daily_pay"] == "16.000 TBD"
    assert ret["operations"][0][1]["subject"] == subject
    assert ret["operations"][0][1]["permlink"] == "hivepy-proposal-title"


@junit_test_case
def test_list_proposals(node, account, wif, subject):
    logger.info("Testing: list_proposals")
    s = Hive(node=[node], no_broadcast=False, keys=[wif])
    # list inactive proposals, our proposal shoud be here
    proposals = s.rpc.list_proposals([account], 1000, "by_creator", "ascending", "inactive")
    found = None
    for proposal in proposals:
        if proposal["subject"] == subject:
            found = proposal

    assert found is not None

    # list active proposals, our proposal shouldnt be here
    proposals = s.rpc.list_proposals([account], 1000, "by_creator", "ascending", "active")
    found = None
    for proposal in proposals:
        if proposal["subject"] == subject:
            found = proposal

    assert found is None

    # list all proposals, our proposal should be here
    proposals = s.rpc.list_proposals([account], 1000, "by_creator", "ascending", "all")

    found = None
    for proposal in proposals:
        if proposal["subject"] == subject:
            found = proposal

    assert found is not None


@junit_test_case
def test_find_proposals(node, account, wif, subject):
    logger.info("Testing: find_proposals")
    s = Hive(node=[node], no_broadcast=False, keys=[wif])
    # first we will find our special proposal and get its proposal_id
    proposals = s.rpc.list_proposals([account], 1000, "by_creator", "ascending", "inactive")

    found = None
    for proposal in proposals:
        if proposal["subject"] == subject:
            found = proposal

    assert found is not None
    proposal_id = int(found["proposal_id"])

    ret = s.rpc.find_proposals([proposal_id])
    assert ret[0]["subject"] == found["subject"]


@junit_test_case
def test_vote_proposal(node, account, wif, subject):
    logger.info("Testing: vote_proposal")
    s = Hive(node=[node], no_broadcast=False, keys=[wif])
    # first we will find our special proposal and get its proposal_id
    proposals = s.rpc.list_proposals([account], 1000, "by_creator", "ascending", "inactive")

    found = None
    for proposal in proposals:
        if proposal["subject"] == subject:
            found = proposal

    assert found is not None
    proposal_id = int(found["proposal_id"])

    # now lets vote
    from beembase.operations import Update_proposal_votes

    op = Update_proposal_votes(**{"voter": account, "proposal_ids": [proposal_id], "approve": True})

    ret = None
    try:
        ret = s.finalizeOp(op, account, "active")
    except Exception as ex:
        logger.exception("Exception: {}".format(ex))
        raise ex

    assert ret["operations"][0][1]["voter"] == account
    assert ret["operations"][0][1]["proposal_ids"][0] == proposal_id
    assert ret["operations"][0][1]["approve"] == True
    hive_utils.common.wait_n_blocks(s.rpc.url, 2)


@junit_test_case
def test_list_voter_proposals(node, account, wif, subject):
    logger.info("Testing: list_voter_proposals")
    s = Hive(node=[node], no_broadcast=False, keys=[wif])
    voter_proposals = s.rpc.list_proposal_votes([account], 1000, "by_voter_proposal", "ascending", "inactive")

    found = None
    for proposals in voter_proposals:
        if proposals["proposal"]["subject"] == subject:
            found = proposals

    assert found is not None


@junit_test_case
def test_remove_proposal(node, account, wif, subject):
    logger.info("Testing: remove_proposal")
    s = Hive(node=[node], no_broadcast=False, keys=[wif])
    # first we will find our special proposal and get its proposal_id
    proposals = s.rpc.list_proposals([account], 1000, "by_creator", "ascending", "inactive")

    found = None
    for proposal in proposals:
        if proposal["subject"] == subject:
            found = proposal

    assert found is not None, "Not found"
    proposal_id = int(found["proposal_id"])

    # remove proposal
    print(account)
    from beembase.operations import Remove_proposal

    op = Remove_proposal(**{"voter": account, "proposal_owner": account, "proposal_ids": [proposal_id]})

    try:
        s.finalizeOp(op, account, "active")
    except Exception as ex:
        logger.exception("Exception: {}".format(ex))
        raise ex

    # try to find our special proposal
    proposals = s.rpc.list_proposals([account], 1000, "by_creator", "ascending", "inactive")

    found = None
    for proposal in proposals:
        if proposal["subject"] == subject:
            found = proposal

    assert found is None, "Not found"


@junit_test_case
def test_iterate_results_test(node, creator_account, receiver_account, wif, subject, remove):
    logger.info("Testing: test_iterate_results_test")
    # test for iterate prosals
    # 1 we will create n proposals of which k proposal will have the same value in one of the fields
    # 2 then we will list proposals starting from kth proposal with limit set to m < k
    # 3 we list proposals again with the same conditiona as in 2, we should get the same set of results
    #   in real life scenatio pagination scheme with limit set to value lower than "k" will be showing
    #   the same results and will hang
    # 4 then we will use newly introduced last_id field, we should see diferent set of proposals
    s = Hive(node=[node], no_broadcast=False, keys=[wif])

    from beem.account import Account

    try:
        creator = Account(creator_account, hive_instance=s)
    except Exception as ex:
        logger.error("Account: {} not found. {}".format(creator_account, ex))
        raise ex

    try:
        receiver = Account(receiver_account, hive_instance=s)
    except Exception as ex:
        logger.error("Account: {} not found. {}".format(receiver_account, ex))
        raise ex

    import datetime

    now = datetime.datetime.now()

    # 1 we will create n proposals of which k proposal will have the same value in one of the fields
    # here we have 5 proposals with the same start date
    start_end_pairs = [[1, 1], [2, 2], [4, 3], [5, 4], [5, 5], [5, 6], [5, 7], [5, 8], [6, 9]]

    from beembase.operations import Create_proposal

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
            s.finalizeOp(op, creator["name"], "active")
        except Exception as ex:
            logger.exception("Exception: {}".format(ex))
            raise ex
    hive_utils.common.wait_n_blocks(node, 5)

    start_date = test_utils.date_to_iso(now + datetime.timedelta(days=5))

    # 2 then we will list proposals starting from kth proposal with limit set to m < k
    proposals = s.rpc.list_proposals([start_date], 3, "by_start_date", "descending", "all")
    assert len(proposals) == 3, "Expected {} elements got {}".format(3, len(proposals))
    ids = []
    for proposal in proposals:
        assert proposal["start_date"] == start_date, "Expected start_date do not match {} != {}".format(
            start_date, proposals[-1]["start_date"]
        )
        ids.append(proposal["proposal_id"])
    assert len(ids) == 3, "Expected {} elements got {}".format(3, len(ids))

    # 3 we list proposals again with the same conditiona as in 2, we should get the same set of results
    proposals = s.rpc.list_proposals([start_date], 3, "by_start_date", "descending", "all")
    assert len(proposals) == 3, "Expected {} elements got {}".format(3, len(proposals))
    oids = []
    for proposal in proposals:
        assert proposal["start_date"] == start_date, "Expected start_date do not match {} != {}".format(
            start_date, proposals[-1]["start_date"]
        )
        oids.append(proposal["proposal_id"])
    assert len(oids) == 3, "Expected {} elements got {}".format(3, len(oids))

    # the same set of results check
    for id in ids:
        assert id in oids, "Id not found in expected results array {}".format(id)

    # 4 then we will use newly introduced last_id field, we should see diferent set of proposals
    proposals = s.rpc.list_proposals([start_date], 3, "by_start_date", "descending", "all", oids[-1])

    start_date, end_date = test_utils.get_start_and_end_date(now, 5, 4)

    assert proposals[-1]["start_date"] == start_date, "Expected start_date do not match {} != {}".format(
        start_date, proposals[-1]["start_date"]
    )
    assert proposals[-1]["end_date"] == end_date, "Expected end_date do not match {} != {}".format(
        end_date, proposals[-1]["end_date"]
    )

    # remove all created proposals
    from beembase.operations import Remove_proposal

    if not remove:
        start_date = test_utils.date_to_iso(now + datetime.timedelta(days=6))
        for _ in range(0, 2):
            proposals = s.list_proposals([start_date], 5, "by_start_date", "descending", "all")
            ids = []
            for proposal in proposals:
                ids.append(int(proposal["proposal_id"]))

            op = Remove_proposal(**{"voter": creator["name"], "proposal_ids": ids})
            try:
                s.finalizeOp(op, creator["name"], "active")
            except Exception as ex:
                logger.exception("Exception: {}".format(ex))
                raise ex
            hive_utils.common.wait_n_blocks(node, 3)


@junit_test_case
def test_update_proposal(node, creator, wif):
    from beembase.operations import Update_proposal
    from datetime import timedelta
    import dateutil.parser

    s = Hive(node=[node], no_broadcast=False, keys=[wif])

    logger.info("Testing: update_proposal without updating the end date")
    proposals = s.rpc.list_proposals([creator], 1000, "by_creator", "ascending", "all")
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
        s.finalizeOp(op, creator, "active")
    except Exception as ex:
        logger.exception("Exception: {}".format(ex))
        raise ex
    hive_utils.common.wait_n_blocks(node, 3)

    proposals = s.rpc.list_proposals([creator], 1000, "by_creator", "ascending", "all")
    print(proposals[0])
    assert proposals[0]["subject"] == new_subject, "Subjects dont match"
    assert proposals[0]["daily_pay"] == new_daily_pay, "daily pay dont match"

    logger.info("Testing: update_proposal and updating the end date")
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
        s.finalizeOp(op, creator, "active")
    except Exception as ex:
        logger.exception("Exception: {}".format(ex))
        raise ex
    hive_utils.common.wait_n_blocks(node, 3)

    proposals = s.rpc.list_proposals([creator], 1000, "by_creator", "ascending", "all")
    print(proposals[0])
    assert proposals[0]["end_date"] == end_date, "End date doesn't match"


if __name__ == "__main__":
    logger.info("Performing SPS tests")
    import argparse

    parser = argparse.ArgumentParser()
    parser.add_argument("creator", help="Account to create proposals with")
    parser.add_argument("receiver", help="Account to receive funds")
    parser.add_argument("wif", help="Private key for creator account")
    parser.add_argument("--node-url", dest="node_url", default="http://127.0.0.1:8090", help="Url of working hive node")
    parser.add_argument(
        "--run-hived",
        dest="hived_path",
        help="Path to hived executable. Warning: using this option will erase contents of selected hived working directory.",
    )
    parser.add_argument(
        "--working-dir", dest="hived_working_dir", default="/tmp/hived-data/", help="Path to hived working directory"
    )
    parser.add_argument(
        "--config-path",
        dest="hived_config_path",
        default="../../hive_utils/resources/config.ini.in",
        help="Path to source config.ini file",
    )
    parser.add_argument(
        "--no-erase-proposal",
        action="store_true",
        dest="no_erase_proposal",
        help="Do not erase proposal created with this test",
    )
    parser.add_argument(
        "--junit-output", dest="junit_output", default=None, help="Filename for generating jUnit-compatible XML output"
    )

    args = parser.parse_args()

    node = None

    if args.hived_path:
        logger.info(
            "Running hived via {} in {} with config {}".format(
                args.hived_path, args.hived_working_dir, args.hived_config_path
            )
        )

        node = hive_utils.hive_node.HiveNodeInScreen(args.hived_path, args.hived_working_dir, args.hived_config_path)

    node_url = args.node_url
    wif = args.wif

    logger.info("Using node at: {}".format(node_url))
    logger.info("Using private-key: {}".format(wif))

    subject = str(uuid4())
    logger.info("Subject of testing proposal is set to: {}".format(subject))
    if node is not None:
        node.run_hive_node(["--enable-stale-production"])
    try:
        if node is None or node.is_running():
            test_create_proposal(node_url, args.creator, args.receiver, wif, subject)
            hive_utils.common.wait_n_blocks(node_url, 3)
            test_list_proposals(node_url, args.creator, wif, subject)
            test_find_proposals(node_url, args.creator, wif, subject)
            test_vote_proposal(node_url, args.creator, wif, subject)
            test_list_voter_proposals(node_url, args.creator, wif, subject)
            hive_utils.common.wait_n_blocks(node_url, 3)
            if not args.no_erase_proposal:
                test_remove_proposal(node_url, args.creator, wif, subject)
            test_iterate_results_test(
                node_url, args.creator, args.receiver, args.wif, str(uuid4()), args.no_erase_proposal
            )
            hive_utils.common.wait_n_blocks(node_url, 3)
            test_update_proposal(node_url, args.creator, wif)
            if node is not None:
                node.stop_hive_node()
            sys.exit(0)
        sys.exit(1)
    except Exception as ex:
        logger.error("Exception: {}".format(ex))
        if node is not None:
            node.stop_hive_node()
    finally:
        if args.junit_output is not None:
            test_suite = TestSuite("dhf_tests", hive_utils.common.junit_test_cases)
            with open(args.junit_output, "w") as junit_xml:
                TestSuite.to_file(junit_xml, [test_suite], prettyprint=False)
    sys.exit(2)
