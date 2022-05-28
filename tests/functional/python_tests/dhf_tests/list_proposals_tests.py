#!/usr/bin/python3

import sys

sys.path.append("../../")

import logging

import hive_utils
import os

from hive_utils.common import junit_test_case
from junit_xml import TestSuite

LOG_LEVEL = logging.INFO
LOG_FORMAT = "%(asctime)-15s - %(name)s - %(levelname)s - %(message)s"
MAIN_LOG_PATH = "hdf_list_proposal.log"
log_dir = os.environ.get("TEST_LOG_DIR", None)
if log_dir is not None:
    MAIN_LOG_PATH = log_dir + "/" + MAIN_LOG_PATH
else:
    MAIN_LOG_PATH = "./" + MAIN_LOG_PATH

MODULE_NAME = "DHF-Tests"
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
    logger.error("beem library is not installed.")
    sys.exit(1)

START_END_SUBJECTS = [
    [1, 3, "Subject001"],
    [2, 3, "Subject002"],
    [3, 3, "Subject003"],
    [4, 3, "Subject004"],
    [5, 3, "Subject005"],
    [6, 3, "Subject006"],
    [7, 3, "Subject007"],
    [8, 3, "Subject008"],
    [9, 3, "Subject009"],
]


def create_proposals(node_client, creator_account, receiver_account):
    import datetime

    now = datetime.datetime.now()

    start_date, end_date = hive_utils.common.get_isostr_start_end_date(now, 10, 2)

    from beem.account import Account

    try:
        creator = Account(creator_account, hive_instance=node_client)
    except Exception as ex:
        logger.error("Account: {} not found. {}".format(creator_account, ex))
        raise ex

    try:
        receiver = Account(receiver_account, hive_instance=node_client)
    except Exception as ex:
        logger.error("Account: {} not found. {}".format(receiver_account, ex))
        raise ex

    logger.info("Creating initial post...")
    node_client.post(
        "Hivepy proposal title",
        "Hivepy proposal body",
        creator["name"],
        permlink="hivepy-proposal-title",
        tags="proposals",
    )

    logger.info("Creating proposals...")
    from beembase.operations import Create_proposal

    for start_end_subject in START_END_SUBJECTS:
        start_date, end_date = hive_utils.common.get_isostr_start_end_date(
            now, start_end_subject[0], start_end_subject[1]
        )
        op = Create_proposal(
            **{
                "creator": creator["name"],
                "receiver": receiver["name"],
                "start_date": start_date,
                "end_date": end_date,
                "daily_pay": "16.000 TBD",
                "subject": start_end_subject[2],
                "permlink": "hivepy-proposal-title",
            }
        )
        try:
            node_client.finalizeOp(op, creator["name"], "active")
        except Exception as ex:
            logger.exception("Exception: {}".format(ex))
            raise ex

        hive_utils.common.wait_n_blocks(node_client.rpc.url, 1)
    hive_utils.common.wait_n_blocks(node_client.rpc.url, 2)


@junit_test_case
def list_proposals_test(node_client, creator):
    logger.info("Testing direction ascending with start field given")
    proposals = node_client.rpc.list_proposals([creator], 1000, "by_creator", "ascending", "all")
    # we should get len(START_END_SUBJECTS) proposals with wirs proposal with subject Subject001
    # and last with subject Subject009
    assert len(proposals) == len(
        START_END_SUBJECTS
    ), "Proposals count do not match assumed proposal count {}!={}".format(len(proposals), len(START_END_SUBJECTS))
    assert (
        proposals[0]["subject"] == START_END_SUBJECTS[0][2]
    ), "Subject of the first proposal does not match with assumed proposal subject {}!={}".format(
        proposals[0]["subject"], START_END_SUBJECTS[0][2]
    )
    assert (
        proposals[-1]["subject"] == START_END_SUBJECTS[-1][2]
    ), "Subject of the last proposal does not match with assumed proposal subject {}!={}".format(
        proposals[-1]["subject"], START_END_SUBJECTS[-1][2]
    )

    logger.info("Testing direction by_creator ascending with no start field given")
    proposals = node_client.rpc.list_proposals([], 1000, "by_creator", "ascending", "all")
    # we should get len(START_END_SUBJECTS) proposals with wirs proposal with subject Subject001
    # and last with subject Subject009
    assert len(proposals) == len(
        START_END_SUBJECTS
    ), "Proposals count do not match assumed proposal count {}!={}".format(len(proposals), len(START_END_SUBJECTS))
    assert (
        proposals[0]["subject"] == START_END_SUBJECTS[0][2]
    ), "Subject of the first proposal does not match with assumed proposal subject {}!={}".format(
        proposals[0]["subject"], START_END_SUBJECTS[0][2]
    )
    assert (
        proposals[-1]["subject"] == START_END_SUBJECTS[-1][2]
    ), "Subject of the last proposal does not match with assumed proposal subject {}!={}".format(
        proposals[-1]["subject"], START_END_SUBJECTS[-1][2]
    )

    id_first = proposals[0]["id"]
    id_last = proposals[-1]["id"]

    logger.info("Testing direction descending with start field given")
    proposals = node_client.rpc.list_proposals([creator], 1000, "by_creator", "descending", "all")
    # we should get len(START_END_SUBJECTS) proposals with first proposal with subject Subject009
    # and last with subject Subject001
    assert len(proposals) == len(
        START_END_SUBJECTS
    ), "Proposals count do not match assumed proposal count {}!={}".format(len(proposals), len(START_END_SUBJECTS))
    assert (
        proposals[0]["subject"] == START_END_SUBJECTS[-1][2]
    ), "Subject of the first proposal does not match with assumed proposal subject {}!={}".format(
        proposals[0]["subject"], START_END_SUBJECTS[-1][2]
    )
    assert (
        proposals[-1]["subject"] == START_END_SUBJECTS[0][2]
    ), "Subject of the last proposal does not match with assumed proposal subject {}!={}".format(
        proposals[-1]["subject"], START_END_SUBJECTS[0][2]
    )

    logger.info("Testing direction descending bu_creator with no start field given")
    proposals = node_client.rpc.list_proposals([], 1000, "by_creator", "descending", "all")
    assert len(proposals) == len(
        START_END_SUBJECTS
    ), "Proposals count do not match assumed proposal count {}!={}".format(len(proposals), len(START_END_SUBJECTS))
    assert (
        proposals[0]["subject"] == START_END_SUBJECTS[-1][2]
    ), "Subject of the first proposal does not match with assumed proposal subject {}!={}".format(
        proposals[0]["subject"], START_END_SUBJECTS[-1][2]
    )
    assert (
        proposals[-1]["subject"] == START_END_SUBJECTS[0][2]
    ), "Subject of the last proposal does not match with assumed proposal subject {}!={}".format(
        proposals[-1]["subject"], START_END_SUBJECTS[0][2]
    )

    # if all pass we can proceed with other tests
    # first we will test empty start string in defferent directions
    logger.info("Testing empty start string and ascending direction")
    proposals = node_client.rpc.list_proposals([""], 1, "by_start_date", "ascending", "all")
    # we shoud get proposal with Subject001
    assert (
        proposals[0]["subject"] == START_END_SUBJECTS[0][2]
    ), "Subject of the proposal does not match with assumed proposal subject {}!={}".format(
        proposals[0]["subject"], START_END_SUBJECTS[0][2]
    )

    logger.info("Testing by_start_date no start string and ascending direction")
    proposals = node_client.rpc.list_proposals([], 1, "by_start_date", "ascending", "all")
    # we shoud get proposal with Subject001
    assert (
        proposals[0]["subject"] == START_END_SUBJECTS[0][2]
    ), "Subject of the proposal does not match with assumed proposal subject {}!={}".format(
        proposals[0]["subject"], START_END_SUBJECTS[0][2]
    )

    # now we will test empty start string in descending
    logger.info("Testing empty start string and descending direction")
    proposals = node_client.rpc.list_proposals([""], 1, "by_start_date", "descending", "all")
    assert (
        proposals[0]["subject"] == START_END_SUBJECTS[-1][2]
    ), "Subject of the proposal does not match with assumed proposal subject {}!={}".format(
        proposals[-1]["subject"], START_END_SUBJECTS[-1][2]
    )

    # now we will test no start parameter in descending
    logger.info("Testing by_start_data with no start and descending direction")
    proposals = node_client.rpc.list_proposals([], 1, "by_start_date", "descending", "all")
    assert (
        proposals[0]["subject"] == START_END_SUBJECTS[-1][2]
    ), "Subject of the proposal does not match with assumed proposal subject {}!={}".format(
        proposals[-1]["subject"], START_END_SUBJECTS[-1][2]
    )

    # now we will test empty start string with ascending order and last_id set

    logger.info("Testing empty start string and ascending direction and last_id set")
    proposals = node_client.rpc.list_proposals([""], 100, "by_start_date", "ascending", "all", 5)
    assert proposals[0]["id"] == 5, "First proposal should have id == 5, has {}".format(proposals[0]["id"])
    assert (
        proposals[-1]["subject"] == START_END_SUBJECTS[-1][2]
    ), "Subject of the proposal does not match with assumed proposal subject {}!={}".format(
        proposals[-1]["subject"], START_END_SUBJECTS[-1][2]
    )

    logger.info("Testing no start parameter and ascending direction and last_id set")
    proposals = node_client.rpc.list_proposals([], 100, "by_start_date", "ascending", "all", 5)
    assert proposals[0]["id"] == 5, "First proposal should have id == 5, has {}".format(proposals[0]["id"])
    assert (
        proposals[-1]["subject"] == START_END_SUBJECTS[-1][2]
    ), "Subject of the proposal does not match with assumed proposal subject {}!={}".format(
        proposals[-1]["subject"], START_END_SUBJECTS[-1][2]
    )

    # now we will test empty start string with descending order and last_id set
    logger.info("Testing empty start string and descending direction and last_id set")
    proposals = node_client.rpc.list_proposals([""], 100, "by_start_date", "descending", "all", 5)
    assert proposals[0]["id"] == 5, "First proposal should have id == 5, has {}".format(proposals[0]["id"])
    assert (
        proposals[-1]["subject"] == START_END_SUBJECTS[0][2]
    ), "Subject of the proposal does not match with assumed proposal subject {}!={}".format(
        proposals[-1]["subject"], START_END_SUBJECTS[0][2]
    )

    # now we will test empty start string with descending order and last_id set
    logger.info("Testing no start string and descending direction and last_id set")
    proposals = node_client.rpc.list_proposals([], 100, "by_start_date", "descending", "all", 5)
    assert proposals[0]["id"] == 5, "First proposal should have id == 5, has {}".format(proposals[0]["id"])
    assert (
        proposals[-1]["subject"] == START_END_SUBJECTS[0][2]
    ), "Subject of the proposal does not match with assumed proposal subject {}!={}".format(
        proposals[-1]["subject"], START_END_SUBJECTS[0][2]
    )

    # now we will test empty start string with ascending order and last_id set
    logger.info("Testing not empty start string and ascenging direction and last_id set")
    proposals = node_client.rpc.list_proposals([creator], 100, "by_creator", "ascending", "all", 5)
    assert proposals[0]["id"] == 5, "First proposal should have id == 5, has {}".format(proposals[0]["id"])
    assert (
        proposals[-1]["subject"] == START_END_SUBJECTS[-1][2]
    ), "Subject of the proposal does not match with assumed proposal subject {}!={}".format(
        proposals[-1]["subject"], START_END_SUBJECTS[-1][2]
    )

    # now we will test empty start string with descending order and last_id set
    logger.info("Testing not empty start string and descending direction and last_id set")
    proposals = node_client.rpc.list_proposals([creator], 100, "by_creator", "descending", "all", 5)
    assert proposals[0]["id"] == 5, "First proposal should have id == 5, has {}".format(proposals[0]["id"])
    assert (
        proposals[-1]["subject"] == START_END_SUBJECTS[0][2]
    ), "Subject of the proposal does not match with assumed proposal subject {}!={}".format(
        proposals[-1]["subject"], START_END_SUBJECTS[0][2]
    )

    logger.info("Testing not empty start string and ascending direction and last_id set to the last element")
    proposals = node_client.rpc.list_proposals([creator], 100, "by_creator", "ascending", "all", id_last)
    assert len(proposals) == 1
    assert proposals[0]["id"] == id_last

    logger.info("Testing not empty start string and descending direction and last_id set to the first element")
    proposals = node_client.rpc.list_proposals([creator], 100, "by_creator", "descending", "all", id_first)
    assert len(proposals) == 1
    assert proposals[0]["id"] == id_first

    logger.info("Testing not empty start string and ascending direction and last_id set to the first element")
    proposals = node_client.rpc.list_proposals([creator], 100, "by_creator", "ascending", "all", id_first)
    assert len(proposals) == len(START_END_SUBJECTS)

    logger.info("Testing not empty start string and descending direction and last_id set to the last element")
    proposals = node_client.rpc.list_proposals([creator], 100, "by_creator", "descending", "all", id_last)
    assert len(proposals) == len(START_END_SUBJECTS)


if __name__ == "__main__":
    logger.info("Performing SPS tests")
    import argparse

    parser = argparse.ArgumentParser()
    parser.add_argument("creator", help="Account to create test accounts with")
    parser.add_argument("receiver", help="Account to receive payment")
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
        action="store_false",
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

    if len(wif) == 0:
        logger.error("Private-key is not set in config.ini")
        sys.exit(1)

    logger.info("Using node at: {}".format(node_url))
    logger.info("Using private-key: {}".format(wif))

    keys = [wif]

    if node is not None:
        node.run_hive_node(["--enable-stale-production"])
    try:
        if node is None or node.is_running():
            node_client = Hive(node=[node_url], no_broadcast=False, keys=keys)

            create_proposals(node_client, args.creator, args.receiver)
            list_proposals_test(node_client, args.creator)

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
            test_suite = TestSuite("list_proposals_test", hive_utils.common.junit_test_cases)
            with open(args.junit_output, "w") as junit_xml:
                TestSuite.to_file(junit_xml, [test_suite], prettyprint=False)
    sys.exit(2)
