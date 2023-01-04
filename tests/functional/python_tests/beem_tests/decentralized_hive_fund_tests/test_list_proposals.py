import datetime

from beem import Hive
from beem.account import Account
from beembase.operations import Create_proposal

import test_tools as tt
import hive_utils
from hive_local_tools.functional.python.beem import NodeClientMaker
from hive_local_tools.functional.python.beem.decentralized_hive_fund import CREATOR, TREASURY

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
    now = datetime.datetime.now()

    try:
        creator = Account(creator_account, hive_instance=node_client)
    except Exception as ex:
        tt.logger.error(f"Account: {creator_account} not found. {ex}")
        raise ex

    try:
        receiver = Account(receiver_account, hive_instance=node_client)
    except Exception as ex:
        tt.logger.error(f"Account: {receiver_account} not found. {ex}")
        raise ex

    tt.logger.info("Creating initial post...")
    node_client.post(
        "Hivepy proposal title",
        "Hivepy proposal body",
        creator["name"],
        permlink="hivepy-proposal-title",
        tags="proposals",
    )

    tt.logger.info("Creating proposals...")

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
            tt.logger.exception(f"Exception: {ex}")
            raise ex

        hive_utils.common.wait_n_blocks(node_client.rpc.url, 1)
    hive_utils.common.wait_n_blocks(node_client.rpc.url, 2)


def list_proposals_test(node_client, creator):
    tt.logger.info("Testing direction ascending with start field given")
    proposals = node_client.rpc.list_proposals([creator], 1000, "by_creator", "ascending", "all")
    # we should get len(START_END_SUBJECTS) proposals with wirs proposal with subject Subject001
    # and last with subject Subject009
    assert len(proposals) == len(
        START_END_SUBJECTS
    ), f"Proposals count do not match assumed proposal count {len(proposals)}!={len(START_END_SUBJECTS)}"
    assert (
        proposals[0]["subject"] == START_END_SUBJECTS[0][2]
    ), f"Subject of the first proposal does not match with assumed proposal subject {proposals[0]['subject']}!={START_END_SUBJECTS[0][2]}"
    assert (
        proposals[-1]["subject"] == START_END_SUBJECTS[-1][2]
    ), f"Subject of the last proposal does not match with assumed proposal subject {proposals[-1]['subject']}!={START_END_SUBJECTS[-1][2]}"

    tt.logger.info("Testing direction by_creator ascending with no start field given")
    proposals = node_client.rpc.list_proposals([], 1000, "by_creator", "ascending", "all")
    # we should get len(START_END_SUBJECTS) proposals with wirs proposal with subject Subject001
    # and last with subject Subject009
    assert len(proposals) == len(
        START_END_SUBJECTS
    ), f"Proposals count do not match assumed proposal count {len(proposals)}!={len(START_END_SUBJECTS)}"
    assert (
        proposals[0]["subject"] == START_END_SUBJECTS[0][2]
    ), f"Subject of the first proposal does not match with assumed proposal subject {proposals[0]['subject']}!={START_END_SUBJECTS[0][2]}"
    assert (
        proposals[-1]["subject"] == START_END_SUBJECTS[-1][2]
    ), f"Subject of the last proposal does not match with assumed proposal subject {proposals[-1]['subject']}!={START_END_SUBJECTS[-1][2]}"

    id_first = proposals[0]["id"]
    id_last = proposals[-1]["id"]

    tt.logger.info("Testing direction descending with start field given")
    proposals = node_client.rpc.list_proposals([creator], 1000, "by_creator", "descending", "all")
    # we should get len(START_END_SUBJECTS) proposals with first proposal with subject Subject009
    # and last with subject Subject001
    assert len(proposals) == len(
        START_END_SUBJECTS
    ), f"Proposals count do not match assumed proposal count {len(proposals)}!={len(START_END_SUBJECTS)}"
    assert (
        proposals[0]["subject"] == START_END_SUBJECTS[-1][2]
    ), f"Subject of the first proposal does not match with assumed proposal subject {proposals[0]['subject']}!={START_END_SUBJECTS[-1][2]}"
    assert (
        proposals[-1]["subject"] == START_END_SUBJECTS[0][2]
    ), f"Subject of the last proposal does not match with assumed proposal subject {proposals[-1]['subject']}!={START_END_SUBJECTS[0][2]}"

    tt.logger.info("Testing direction descending bu_creator with no start field given")
    proposals = node_client.rpc.list_proposals([], 1000, "by_creator", "descending", "all")
    assert len(proposals) == len(
        START_END_SUBJECTS
    ), f"Proposals count do not match assumed proposal count {len(proposals)}!={len(START_END_SUBJECTS)}"
    assert (
        proposals[0]["subject"] == START_END_SUBJECTS[-1][2]
    ), f"Subject of the first proposal does not match with assumed proposal subject {proposals[0]['subject']}!={START_END_SUBJECTS[-1][2]}"
    assert (
        proposals[-1]["subject"] == START_END_SUBJECTS[0][2]
    ), f"Subject of the last proposal does not match with assumed proposal subject {proposals[-1]['subject']}!={START_END_SUBJECTS[0][2]}"

    # if all pass we can proceed with other tests
    # first we will test empty start string in defferent directions
    tt.logger.info("Testing empty start string and ascending direction")
    proposals = node_client.rpc.list_proposals([""], 1, "by_start_date", "ascending", "all")
    # we shoud get proposal with Subject001
    assert (
        proposals[0]["subject"] == START_END_SUBJECTS[0][2]
    ), f"Subject of the proposal does not match with assumed proposal subject {proposals[0]['subject']}!={START_END_SUBJECTS[0][2]}"

    tt.logger.info("Testing by_start_date no start string and ascending direction")
    proposals = node_client.rpc.list_proposals([], 1, "by_start_date", "ascending", "all")
    # we shoud get proposal with Subject001
    assert (
        proposals[0]["subject"] == START_END_SUBJECTS[0][2]
    ), f"Subject of the proposal does not match with assumed proposal subject {proposals[0]['subject']}!={START_END_SUBJECTS[0][2]}"

    # now we will test empty start string in descending
    tt.logger.info("Testing empty start string and descending direction")
    proposals = node_client.rpc.list_proposals([""], 1, "by_start_date", "descending", "all")
    assert (
        proposals[0]["subject"] == START_END_SUBJECTS[-1][2]
    ), f"Subject of the proposal does not match with assumed proposal subject {proposals[-1]['subject']}!={START_END_SUBJECTS[-1][2]}"

    # now we will test no start parameter in descending
    tt.logger.info("Testing by_start_data with no start and descending direction")
    proposals = node_client.rpc.list_proposals([], 1, "by_start_date", "descending", "all")
    assert (
        proposals[0]["subject"] == START_END_SUBJECTS[-1][2]
    ), f"Subject of the proposal does not match with assumed proposal subject {proposals[-1]['subject']}!={START_END_SUBJECTS[-1][2]}"

    # now we will test empty start string with ascending order and last_id set

    tt.logger.info("Testing empty start string and ascending direction and last_id set")
    proposals = node_client.rpc.list_proposals([""], 100, "by_start_date", "ascending", "all", 5)
    assert proposals[0]["id"] == 5, f"First proposal should have id == 5, has {proposals[0]['id']}"
    assert (
        proposals[-1]["subject"] == START_END_SUBJECTS[-1][2]
    ), f"Subject of the proposal does not match with assumed proposal subject {proposals[-1]['subject']}!={START_END_SUBJECTS[-1][2]}"

    tt.logger.info("Testing no start parameter and ascending direction and last_id set")
    proposals = node_client.rpc.list_proposals([], 100, "by_start_date", "ascending", "all", 5)
    assert proposals[0]["id"] == 5, f"First proposal should have id == 5, has {proposals[0]['id']}"
    assert (
        proposals[-1]["subject"] == START_END_SUBJECTS[-1][2]
    ), f"Subject of the proposal does not match with assumed proposal subject {proposals[-1]['subject']}!={START_END_SUBJECTS[-1][2]}"

    # now we will test empty start string with descending order and last_id set
    tt.logger.info("Testing empty start string and descending direction and last_id set")
    proposals = node_client.rpc.list_proposals([""], 100, "by_start_date", "descending", "all", 5)
    assert proposals[0]["id"] == 5, f"First proposal should have id == 5, has {proposals[0]['id']}"
    assert (
        proposals[-1]["subject"] == START_END_SUBJECTS[0][2]
    ), f"Subject of the proposal does not match with assumed proposal subject {proposals[-1]['subject']}!={START_END_SUBJECTS[0][2]}"

    # now we will test empty start string with descending order and last_id set
    tt.logger.info("Testing no start string and descending direction and last_id set")
    proposals = node_client.rpc.list_proposals([], 100, "by_start_date", "descending", "all", 5)
    assert proposals[0]["id"] == 5, f"First proposal should have id == 5, has {proposals[0]['id']}"
    assert (
        proposals[-1]["subject"] == START_END_SUBJECTS[0][2]
    ), f"Subject of the proposal does not match with assumed proposal subject {proposals[-1]['subject']}!={START_END_SUBJECTS[0][2]}"

    # now we will test empty start string with ascending order and last_id set
    tt.logger.info("Testing not empty start string and ascenging direction and last_id set")
    proposals = node_client.rpc.list_proposals([creator], 100, "by_creator", "ascending", "all", 5)
    assert proposals[0]["id"] == 5, f"First proposal should have id == 5, has {proposals[0]['id']}"
    assert (
        proposals[-1]["subject"] == START_END_SUBJECTS[-1][2]
    ), f"Subject of the proposal does not match with assumed proposal subject {proposals[-1]['subject']}!={START_END_SUBJECTS[-1][2]}"

    # now we will test empty start string with descending order and last_id set
    tt.logger.info("Testing not empty start string and descending direction and last_id set")
    proposals = node_client.rpc.list_proposals([creator], 100, "by_creator", "descending", "all", 5)
    assert proposals[0]["id"] == 5, f"First proposal should have id == 5, has {proposals[0]['id']}"
    assert (
        proposals[-1]["subject"] == START_END_SUBJECTS[0][2]
    ), f"Subject of the proposal does not match with assumed proposal subject {proposals[-1]['subject']}!={START_END_SUBJECTS[0][2]}"

    tt.logger.info("Testing not empty start string and ascending direction and last_id set to the last element")
    proposals = node_client.rpc.list_proposals([creator], 100, "by_creator", "ascending", "all", id_last)
    assert len(proposals) == 1
    assert proposals[0]["id"] == id_last

    tt.logger.info("Testing not empty start string and descending direction and last_id set to the first element")
    proposals = node_client.rpc.list_proposals([creator], 100, "by_creator", "descending", "all", id_first)
    assert len(proposals) == 1
    assert proposals[0]["id"] == id_first

    tt.logger.info("Testing not empty start string and ascending direction and last_id set to the first element")
    proposals = node_client.rpc.list_proposals([creator], 100, "by_creator", "ascending", "all", id_first)
    assert len(proposals) == len(START_END_SUBJECTS)

    tt.logger.info("Testing not empty start string and descending direction and last_id set to the last element")
    proposals = node_client.rpc.list_proposals([creator], 100, "by_creator", "descending", "all", id_last)
    assert len(proposals) == len(START_END_SUBJECTS)


def test_list_proposals(node_client: NodeClientMaker):
    node_client = node_client()

    create_proposals(node_client, CREATOR, TREASURY)
    list_proposals_test(node_client, CREATOR)
